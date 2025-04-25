#include "../../include/server/file_service.h"
#include "../../include/server/utils/logger.h"
#include "../../include/server/utils/security.h"
#include "../../include/server/utils/datetime.h"
#include "../../include/server/utils/config.h"
#include "../../include/server/utils/string_util.h"
#include <nlohmann/json.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ctime>

namespace fs = boost::filesystem;
namespace im {

using json = nlohmann::json;

FileServiceImpl::FileServiceImpl(
    std::shared_ptr<db::MySQLConnection> mysql_conn,
    std::shared_ptr<db::RedisClient> redis_client,
    std::shared_ptr<kafka::KafkaProducer> kafka_producer
) : mysql_conn_(mysql_conn),
    redis_client_(redis_client),
    kafka_producer_(kafka_producer) {
    // 初始化文件存储路径
    file_storage_path_ = utils::Config::GetInstance().GetString("file.storage_path", "./files");
    
    // 确保目录存在
    if (!CreateDirectory(file_storage_path_)) {
        LOG_ERROR("创建文件存储目录失败: {}", file_storage_path_);
    }
    
    LOG_INFO("FileServiceImpl initialized, storage path: {}", file_storage_path_);
}

grpc::Status FileServiceImpl::UploadFile(
    grpc::ServerContext* context,
    const UploadFileRequest* request,
    UploadFileResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t user_id;
    if (!ValidateToken(token, user_id)) {
        response->set_success(false);
        response->set_message("未授权");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "无效的令牌");
    }
    
    // 验证文件信息
    if (request->file_name().empty() || request->file_size() <= 0) {
        response->set_success(false);
        response->set_message("无效的文件信息");
        return grpc::Status::OK;
    }
    
    // 检查文件大小限制
    int64_t max_file_size = utils::Config::GetInstance().GetInt("file.max_size", 100 * 1024 * 1024); // 默认100MB
    if (request->file_size() > max_file_size) {
        response->set_success(false);
        response->set_message("文件大小超过限制，最大允许" + std::to_string(max_file_size / 1024 / 1024) + "MB");
        return grpc::Status::OK;
    }
    
    // 创建文件记录
    FileInfo file_info;
    file_info.set_file_name(request->file_name());
    file_info.set_file_size(request->file_size());
    file_info.set_file_type(request->file_type());
    file_info.set_uploader_id(request->uploader_id());
    
    int64_t file_id = CreateFileRecord(file_info, request->uploader_id());
    if (file_id == 0) {
        response->set_success(false);
        response->set_message("创建文件记录失败");
        return grpc::Status::OK;
    }
    
    // 创建临时文件路径
    std::string temp_dir = file_storage_path_ + "/temp";
    if (!CreateDirectory(temp_dir)) {
        response->set_success(false);
        response->set_message("创建临时目录失败");
        return grpc::Status::OK;
    }
    
    std::string temp_path = temp_dir + "/" + std::to_string(file_id) + "_" + std::to_string(user_id) + ".tmp";
    
    // 保存临时文件路径到会话缓存
    {
        std::lock_guard<std::mutex> lock(upload_sessions_mutex_);
        upload_sessions_[file_id] = temp_path;
    }
    
    // 返回响应
    response->set_success(true);
    response->set_message("文件准备上传");
    response->set_file_id(file_id);
    
    LOG_INFO("用户 {} 准备上传文件: {}，大小: {} 字节", user_id, request->file_name(), request->file_size());
    return grpc::Status::OK;
}

grpc::Status FileServiceImpl::UploadFileChunk(
    grpc::ServerContext* context,
    grpc::ServerReader<FileChunk>* reader,
    CommonResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t user_id;
    if (!ValidateToken(token, user_id)) {
        response->set_success(false);
        response->set_message("未授权");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "无效的令牌");
    }
    
    // 读取第一个块
    FileChunk chunk;
    if (!reader->Read(&chunk)) {
        response->set_success(false);
        response->set_message("无效的文件数据");
        return grpc::Status::OK;
    }
    
    // 获取文件ID
    int64_t file_id = chunk.file_id();
    if (file_id <= 0) {
        response->set_success(false);
        response->set_message("无效的文件ID");
        return grpc::Status::OK;
    }
    
    // 检查文件访问权限
    if (!CheckFileAccess(user_id, file_id)) {
        response->set_success(false);
        response->set_message("无权访问该文件");
        return grpc::Status::OK;
    }
    
    // 获取临时文件路径
    std::string temp_path;
    {
        std::lock_guard<std::mutex> lock(upload_sessions_mutex_);
        auto it = upload_sessions_.find(file_id);
        if (it == upload_sessions_.end()) {
            response->set_success(false);
            response->set_message("找不到上传会话");
            return grpc::Status::OK;
        }
        temp_path = it->second;
    }
    
    // 创建并打开临时文件
    std::ofstream file(temp_path, std::ios::binary | std::ios::app);
    if (!file) {
        LOG_ERROR("无法创建临时文件: {}", temp_path);
        response->set_success(false);
        response->set_message("无法创建临时文件");
        return grpc::Status::OK;
    }
    
    // 写入第一个块
    file.write(chunk.chunk_data().data(), chunk.chunk_data().size());
    
    // 读取并写入剩余块
    int chunks_received = 1;
    while (reader->Read(&chunk)) {
        file.write(chunk.chunk_data().data(), chunk.chunk_data().size());
        chunks_received++;
        
        if (chunk.is_last_chunk()) {
            break;
        }
    }
    
    file.close();
    
    // 检查是否是最后一个块
    if (!chunk.is_last_chunk()) {
        response->set_success(false);
        response->set_message("文件传输未完成");
        return grpc::Status::OK;
    }
    
    // 获取文件信息
    FileInfo file_info;
    if (!GetFileInfo(file_id, &file_info)) {
        LOG_ERROR("获取文件信息失败: file_id={}", file_id);
        response->set_success(false);
        response->set_message("获取文件信息失败");
        return grpc::Status::OK;
    }
    
    // 移动临时文件到最终位置
    std::string final_path = GenerateFilePath(file_id, user_id, file_info.file_name());
    std::string final_dir = fs::path(final_path).parent_path().string();
    
    if (!CreateDirectory(final_dir)) {
        LOG_ERROR("创建文件目录失败: {}", final_dir);
        response->set_success(false);
        response->set_message("创建文件目录失败");
        return grpc::Status::OK;
    }
    
    try {
        fs::rename(temp_path, final_path);
    } catch (const std::exception& e) {
        LOG_ERROR("移动文件失败: {} -> {}, 错误: {}", temp_path, final_path, e.what());
        response->set_success(false);
        response->set_message("移动文件失败");
        return grpc::Status::OK;
    }
    
    // 更新文件记录
    try {
        std::string sql = "UPDATE files SET status = 1, file_path = ?, updated_at = ? WHERE id = ?";
        
        int64_t now = utils::DateTime::NowSeconds();
        bool success = mysql_conn_->ExecuteUpdate(sql, {
            final_path,
            std::to_string(now),
            std::to_string(file_id)
        });
        
        if (!success) {
            LOG_ERROR("更新文件记录失败: file_id={}", file_id);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("更新文件记录出错: {}", e.what());
    }
    
    // 从会话缓存中移除
    {
        std::lock_guard<std::mutex> lock(upload_sessions_mutex_);
        upload_sessions_.erase(file_id);
    }
    
    // 最后返回成功响应
    response->set_success(true);
    response->set_message("文件上传成功");
    
    LOG_INFO("用户 {} 成功上传文件: id={}, 名称={}, 大小={}, 块数={}",
             user_id, file_id, file_info.file_name(), file_info.file_size(), chunks_received);
    return grpc::Status::OK;
}

grpc::Status FileServiceImpl::DownloadFile(
    grpc::ServerContext* context,
    const DownloadFileRequest* request,
    DownloadFileResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t user_id;
    if (!ValidateToken(token, user_id)) {
        response->set_success(false);
        response->set_message("未授权");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "无效的令牌");
    }
    
    int64_t file_id = request->file_id();
    
    // 检查文件访问权限
    if (!CheckFileAccess(user_id, file_id)) {
        response->set_success(false);
        response->set_message("无权访问此文件");
        return grpc::Status::OK;
    }
    
    // 获取文件信息
    FileInfo file_info;
    if (!GetFileInfo(file_id, &file_info)) {
        response->set_success(false);
        response->set_message("文件不存在");
        return grpc::Status::OK;
    }
    
    // 返回文件信息
    response->set_success(true);
    response->set_message("文件准备下载");
    *response->mutable_file_info() = file_info;
    
    LOG_INFO("用户 {} 准备下载文件: {}, 大小: {} 字节", user_id, file_info.file_name(), file_info.file_size());
    return grpc::Status::OK;
}

grpc::Status FileServiceImpl::DownloadFileChunk(
    grpc::ServerContext* context,
    const DownloadFileRequest* request,
    grpc::ServerWriter<FileChunk>* writer
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t user_id;
    if (!ValidateToken(token, user_id)) {
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "无效的令牌");
    }
    
    int64_t file_id = request->file_id();
    
    // 检查文件访问权限
    if (!CheckFileAccess(user_id, file_id)) {
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "无权访问此文件");
    }
    
    // 获取文件信息
    FileInfo file_info;
    if (!GetFileInfo(file_id, &file_info)) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "文件不存在");
    }
    
    // 打开文件
    std::string file_path = file_info.file_path();
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        LOG_ERROR("无法打开文件: {}", file_path);
        return grpc::Status(grpc::StatusCode::INTERNAL, "无法打开文件");
    }
    
    // 设置块大小
    const int chunk_size = 1024 * 1024; // 1MB
    char buffer[chunk_size];
    int chunk_index = 0;
    bool is_last_chunk = false;
    
    // 逐块发送文件
    while (!file.eof() && !is_last_chunk) {
        file.read(buffer, chunk_size);
        int bytes_read = file.gcount();
        
        is_last_chunk = file.eof();
        
        FileChunk chunk;
        chunk.set_file_id(file_id);
        chunk.set_chunk_index(chunk_index);
        chunk.set_chunk_data(buffer, bytes_read);
        chunk.set_is_last_chunk(is_last_chunk);
        
        if (!writer->Write(chunk)) {
            LOG_ERROR("写入文件块失败: file_id={}, chunk_index={}", file_id, chunk_index);
            return grpc::Status(grpc::StatusCode::INTERNAL, "写入文件块失败");
        }
        
        chunk_index++;
    }
    
    LOG_INFO("用户 {} 成功下载文件: id={}, 名称={}, 块数={}", user_id, file_id, file_info.file_name(), chunk_index);
    return grpc::Status::OK;
}

grpc::Status FileServiceImpl::SendFileTransfer(
    grpc::ServerContext* context,
    const SendFileTransferRequest* request,
    SendFileTransferResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t user_id;
    if (!ValidateToken(token, user_id)) {
        response->set_success(false);
        response->set_message("未授权");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "未授权");
    }
    
    // 验证用户是否是发送者
    if (request->from_user_id() != user_id) {
        response->set_success(false);
        response->set_message("无权发送此请求");
        return grpc::Status::OK;
    }
    
    // 检查文件是否存在
    int64_t file_id = request->file_id();
    FileInfo file_info;
    if (!GetFileInfo(file_id, &file_info)) {
        response->set_success(false);
        response->set_message("文件不存在");
        return grpc::Status::OK;
    }
    
    // 检查接收者是否存在
    int64_t to_user_id = request->to_user_id();
    try {
        std::string sql = "SELECT user_id FROM users WHERE user_id = ?";
        auto results = mysql_conn_->ExecuteQuery(sql, {std::to_string(to_user_id)});
        if (results.empty()) {
            response->set_success(false);
            response->set_message("接收者不存在");
            return grpc::Status::OK;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("查询用户失败: {}", e.what());
        response->set_success(false);
        response->set_message("服务器错误");
        return grpc::Status::OK;
    }
    
    // 创建文件传输请求记录
    int64_t request_id = 0;
    try {
        std::string sql = "INSERT INTO file_transfer_requests (from_user_id, to_user_id, file_id, file_name, file_size, status, created_at, updated_at) "
                          "VALUES (?, ?, ?, ?, ?, 0, ?, ?)";
        
        int64_t now = utils::DateTime::NowSeconds();
        
        request_id = mysql_conn_->ExecuteInsert(sql, {
            std::to_string(request->from_user_id()),
            std::to_string(request->to_user_id()),
            std::to_string(file_id),
            file_info.file_name(),
            std::to_string(file_info.file_size()),
            std::to_string(now),
            std::to_string(now)
        });
        
        if (request_id == 0) {
            LOG_ERROR("创建文件传输请求失败");
            response->set_success(false);
            response->set_message("创建请求失败");
            return grpc::Status::OK;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("创建文件传输请求出错: {}", e.what());
        response->set_success(false);
        response->set_message("服务器错误");
        return grpc::Status::OK;
    }
    
    // 通知接收者
    // 发送事件到Kafka
    if (kafka_producer_) {
        try {
            nlohmann::json event;
            event["type"] = "file_transfer_request";
            event["request_id"] = request_id;
            event["from_user_id"] = user_id;
            event["to_user_id"] = to_user_id;
            event["file_name"] = file_info.file_name();
            event["file_size"] = file_info.file_size();
            event["timestamp"] = utils::DateTime::NowSeconds();
            
            kafka_producer_->SendMessage("im_notifications", event.dump());
        } catch (const std::exception& e) {
            LOG_ERROR("发送Kafka事件失败: {}", e.what());
        }
    }
    
    // 尝试通过WebSocket实时通知
    if (redis_client_) {
        try {
            // 获取接收者的会话ID
            std::string sessions_key = "user:" + std::to_string(to_user_id) + ":sessions";
            std::vector<std::string> session_ids;
            
            // 从Redis集合中获取会话ID
            auto sessions = redis_client_->SetMembers(sessions_key);
            if (!sessions.empty()) {
                session_ids.assign(sessions.begin(), sessions.end());
            }
            
            if (!session_ids.empty()) {
                nlohmann::json notification;
                notification["type"] = "file_transfer_request";
                notification["request_id"] = request_id;
                notification["from_user_id"] = user_id;
                notification["file_name"] = file_info.file_name();
                notification["file_size"] = file_info.file_size();
                notification["timestamp"] = utils::DateTime::NowSeconds();
                
                std::string message = notification.dump();
                
                // 发送通知到接收者所有会话
                for (const auto& session_id : session_ids) {
                    redis_client_->Publish("ws_message:" + session_id, message);
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("发送WebSocket通知失败: {}", e.what());
        }
    }
    
    // 设置响应
    response->set_success(true);
    response->set_message("文件传输请求已发送");
    response->set_request_id(request_id);
    
    LOG_INFO("用户 {} 向用户 {} 发送文件传输请求, id={}, 文件名={}",
            user_id, to_user_id, request_id, file_info.file_name());
    return grpc::Status::OK;
}

grpc::Status FileServiceImpl::HandleFileTransfer(
    grpc::ServerContext* context,
    const HandleFileTransferRequest* request,
    HandleFileTransferResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t user_id;
    if (!ValidateToken(token, user_id)) {
        response->set_success(false);
        response->set_message("未授权");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "未授权");
    }
    
    int64_t request_id = request->request_id();
    bool accept = request->accept();
    
    // 获取请求信息
    try {
        std::string sql = "SELECT * FROM file_transfer_requests WHERE id = ?";
        auto results = mysql_conn_->ExecuteQuery(sql, {std::to_string(request_id)});
        
        if (results.empty()) {
            response->set_success(false);
            response->set_message("请求不存在");
            return grpc::Status::OK;
        }
        
        auto& row = results[0];
        int64_t to_user_id = std::stoll(row["to_user_id"]);
        
        // 检查是否是接收者
        if (to_user_id != user_id) {
            response->set_success(false);
            response->set_message("无权处理此请求");
            return grpc::Status::OK;
        }
        
        // 检查状态是否为待处理
        int status = std::stoi(row["status"]);
        if (status != 0) { // Assuming 0 = PENDING
            response->set_success(false);
            response->set_message("请求已被处理");
            return grpc::Status::OK;
        }
        
        // 更新状态
        int new_status = accept ? 1 : 2; // Assuming 1 = ACCEPTED, 2 = REJECTED
        std::string update_sql = "UPDATE file_transfer_requests SET status = ?, updated_at = ? WHERE id = ?";
        
        int64_t now = utils::DateTime::NowSeconds();
        bool success = mysql_conn_->ExecuteUpdate(update_sql, {
            std::to_string(new_status),
            std::to_string(now),
            std::to_string(request_id)
        });
        
        if (!success) {
            LOG_ERROR("更新文件传输请求状态失败: request_id={}", request_id);
            response->set_success(false);
            response->set_message("处理请求失败");
            return grpc::Status::OK;
        }
        
        // 通知发送者
        int64_t from_user_id = std::stoll(row["from_user_id"]);
        int64_t file_id = std::stoll(row["file_id"]);
        std::string file_name = row["file_name"];
        
        // 发送Kafka事件
        if (kafka_producer_) {
            try {
                nlohmann::json event;
                event["type"] = accept ? "file_transfer_accepted" : "file_transfer_rejected";
                event["request_id"] = request_id;
                event["from_user_id"] = user_id;
                event["to_user_id"] = from_user_id;
                event["file_id"] = file_id;
                event["file_name"] = file_name;
                event["timestamp"] = now;
                
                kafka_producer_->SendMessage("im_notifications", event.dump());
            } catch (const std::exception& e) {
                LOG_ERROR("发送Kafka事件失败: {}", e.what());
            }
        }
        
        // 通过WebSocket通知
        if (redis_client_) {
            try {
                // 获取发送者的会话ID
                std::string sessions_key = "user:" + std::to_string(from_user_id) + ":sessions";
                std::vector<std::string> session_ids;
                
                // 从Redis集合中获取会话ID
                auto sessions = redis_client_->SetMembers(sessions_key);
                if (!sessions.empty()) {
                    session_ids.assign(sessions.begin(), sessions.end());
                }
                
                if (!session_ids.empty()) {
                    nlohmann::json notification;
                    notification["type"] = accept ? "file_transfer_accepted" : "file_transfer_rejected";
                    notification["request_id"] = request_id;
                    notification["user_id"] = user_id;
                    notification["file_id"] = file_id;
                    notification["file_name"] = file_name;
                    notification["timestamp"] = now;
                    
                    std::string message = notification.dump();
                    
                    // 发送通知到发送者所有会话
                    for (const auto& session_id : session_ids) {
                        redis_client_->Publish("ws_message:" + session_id, message);
                    }
                }
            } catch (const std::exception& e) {
                LOG_ERROR("发送WebSocket通知失败: {}", e.what());
            }
        }
        
        // 设置响应
        response->set_success(true);
        response->set_message(accept ? "已接受文件传输请求" : "已拒绝文件传输请求");
        
        LOG_INFO("用户 {} {} 用户 {} 的文件传输请求，文件名={}",
                 user_id, accept ? "接受" : "拒绝", from_user_id, file_name);
        
    } catch (const std::exception& e) {
        LOG_ERROR("处理文件传输请求出错: {}", e.what());
        response->set_success(false);
        response->set_message("服务器错误");
    }
    
    return grpc::Status::OK;
}

std::string FileServiceImpl::GetAuthToken(const grpc::ServerContext* context) const {
    auto metadata = context->client_metadata();
    auto it = metadata.find("authorization");
    if (it != metadata.end()) {
        std::string auth_header(it->second.data(), it->second.size());
        if (auth_header.substr(0, 7) == "Bearer ") {
            return auth_header.substr(7);
        }
    }
    return "";
}

bool FileServiceImpl::ValidateToken(const std::string& token, int64_t& user_id) const {
    if (token.empty()) {
        return false;
    }
    
    std::map<std::string, std::string> payload;
    std::string secret = utils::Config::GetInstance().GetString("security.jwt_secret", "your_jwt_secret");
    
    if (!utils::Security::VerifyJWT(token, secret, payload)) {
        return false;
    }
    
    auto it = payload.find("user_id");
    if (it == payload.end()) {
        return false;
    }
    
    try {
        user_id = std::stoll(it->second);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Token中的user_id无效: {}", e.what());
        return false;
    }
}

int64_t FileServiceImpl::CreateFileRecord(const FileInfo& file_info, int64_t uploader_id) {
    try {
        std::string sql = "INSERT INTO files "
                         "(file_name, file_size, file_type, uploader_id, status, created_at, updated_at) "
                         "VALUES (?, ?, ?, ?, ?, ?, ?)";
        
        int64_t now = utils::DateTime::NowSeconds();
        int status = 0; // 0表示上传中，1表示上传完成
        
        int64_t file_id = mysql_conn_->ExecuteInsert(sql, {
            file_info.file_name(),
            std::to_string(file_info.file_size()),
            file_info.file_type(),
            std::to_string(uploader_id),
            std::to_string(status),
            std::to_string(now),
            std::to_string(now)
        });
        
        return file_id;
    } catch (const std::exception& e) {
        LOG_ERROR("创建文件记录失败: {}", e.what());
        return 0;
    }
}

bool FileServiceImpl::GetFileInfo(int64_t file_id, FileInfo* file_info) {
    try {
        std::string sql = "SELECT id, file_name, file_size, file_type, file_path, uploader_id, created_at "
                         "FROM files WHERE id = ?";
        
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {std::to_string(file_id)});
        
        if (results.empty()) {
            return false;
        }
        
        auto& file = results[0];
        file_info->set_file_id(std::stoll(file["id"]));
        file_info->set_file_name(file["file_name"]);
        file_info->set_file_size(std::stoll(file["file_size"]));
        file_info->set_file_type(file["file_type"]);
        
        if (file.find("file_path") != file.end() && !file["file_path"].empty()) {
            file_info->set_file_path(file["file_path"]);
        }
        
        file_info->set_uploader_id(std::stoll(file["uploader_id"]));
        file_info->set_upload_time(std::stoll(file["created_at"]));
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("获取文件信息失败: {}", e.what());
        return false;
    }
}

bool FileServiceImpl::CheckFileAccess(int64_t user_id, int64_t file_id) const {
    try {
        // 首先检查用户是否是文件上传者
        std::string sql = "SELECT uploader_id FROM files WHERE id = ?";
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {std::to_string(file_id)});
        
        if (!results.empty()) {
            int64_t uploader_id = std::stoll(results[0]["uploader_id"]);
            if (uploader_id == user_id) {
                return true;
            }
        } else {
            return false; // 文件不存在
        }
        
        // 检查用户是否有被共享的访问权限
        sql = "SELECT COUNT(*) as count FROM file_shares "
             "WHERE file_id = ? AND user_id = ? AND expires_at > ?";
        
        int64_t now = utils::DateTime::NowSeconds();
        results = mysql_conn_->ExecuteQuery(sql, {
            std::to_string(file_id),
            std::to_string(user_id),
            std::to_string(now)
        });
        
        if (!results.empty() && std::stoi(results[0]["count"]) > 0) {
            return true;
        }
        
        // 检查是否是通过文件传输请求获得的访问权限
        sql = "SELECT COUNT(*) as count FROM file_transfer_requests "
             "WHERE file_id = ? AND to_user_id = ? AND status = ?";
        
        results = mysql_conn_->ExecuteQuery(sql, {
            std::to_string(file_id),
            std::to_string(user_id),
            std::to_string(static_cast<int>(im::FileTransferStatus::COMPLETED))
        });
        
        if (!results.empty() && std::stoi(results[0]["count"]) > 0) {
            return true;
        }
        
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("检查文件访问权限失败: {}", e.what());
        return false;
    }
}

bool FileServiceImpl::CreateDirectory(const std::string& path) const {
    try {
        if (!fs::exists(path)) {
            return fs::create_directories(path);
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("创建目录失败: {}, 错误: {}", path, e.what());
        return false;
    }
}

std::string FileServiceImpl::GenerateFilePath(int64_t file_id, int64_t user_id, const std::string& file_name) const {
    // 创建基于用户ID和日期的目录结构，避免单一目录下文件过多
    // 格式: {存储根目录}/{用户ID}/{年月}/{文件ID}_{原始文件名}
    
    // 获取当前日期
    time_t now = time(nullptr);
    struct tm timeinfo;
    #ifdef _WIN32
    localtime_s(&timeinfo, &now);
    #else
    localtime_r(&now, &timeinfo);
    #endif
    
    char date_dir[10];
    strftime(date_dir, sizeof(date_dir), "%Y%m", &timeinfo);
    
    std::string user_dir = file_storage_path_ + "/" + std::to_string(user_id);
    std::string date_path = user_dir + "/" + date_dir;
    
    // 构建文件路径
    std::string file_path = date_path + "/" + std::to_string(file_id) + "_" + file_name;
    
    return file_path;
}

} // namespace im 