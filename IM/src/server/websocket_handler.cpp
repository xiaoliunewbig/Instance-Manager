#include "server/websocket_handler.h"
#include "server/utils/logger.h"
#include "server/utils/datetime.h"
#include "server/utils/security.h"
#include "server/utils/config.h"
#include <nlohmann/json.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/beast/http.hpp>

namespace im {

using json = nlohmann::json;
namespace http = boost::beast::http;

// WebSocketSession 实现
WebSocketSession::WebSocketSession(tcp::socket socket, const std::string& path)
    : ws_(std::move(socket)), path_(path) {
    // 生成会话ID
    boost::uuids::random_generator gen;
    boost::uuids::uuid uuid = gen();
    session_id_ = boost::uuids::to_string(uuid);

    // 配置WebSocket选项
    ws_.set_option(websocket::stream_base::timeout::suggested(
        beast::role_type::server));
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, "IMServer");
        }));

    // 设置握手超时时间为10秒
    ws_.set_option(websocket::stream_base::timeout{
        std::chrono::seconds(10),  // 读超时
        std::chrono::seconds(10),  // 写超时
        false                      // 是否启用ping/pong
    });

    LOG_INFO("WebSocket会话创建: id={}", session_id_);
}


void WebSocketSession::Start() {
    LOG_INFO("启动WebSocket会话: id={}", session_id_);
    DoAccept();
}

void WebSocketSession::Send(const std::string& message) {
    DoWrite(message);
}

void WebSocketSession::Close() {
    boost::system::error_code ec;
    ws_.close(websocket::close_code::normal, ec);
    if (ec) {
        LOG_ERROR("关闭WebSocket会话失败: {}", ec.message());
    } else {
        LOG_INFO("WebSocket会话已关闭: id={}, user_id={}", session_id_, user_id_);
    }
}

const std::string& WebSocketSession::GetID() const {
    return session_id_;
}

int64_t WebSocketSession::GetUserID() const {
    return user_id_;
}

void WebSocketSession::SetUserID(int64_t user_id) {
    user_id_ = user_id;
    LOG_INFO("WebSocket会话关联用户: session_id={}, user_id={}", session_id_, user_id_);
}

void WebSocketSession::SetHandler(std::shared_ptr<WebSocketHandler> handler) {
    handler_ = handler;
}

bool WebSocketSession::IsExpired(int64_t now) const {
    int64_t expire_time = utils::Config::GetInstance().GetInt("websocket.session_expire_seconds", 300);
    return (now - last_active_time_) > expire_time;
}

void WebSocketSession::UpdateActiveTime() {
    last_active_time_ = utils::DateTime::NowSeconds();
}

void WebSocketSession::DoAccept() {
    auto self = shared_from_this();
    ws_.async_accept(
        [self](boost::system::error_code ec) {
            if (ec) {
                LOG_ERROR("WebSocket 握手失败: {}", ec.message());
                return;
            }
            
            LOG_INFO("WebSocket 握手成功: id={}", self->session_id_);
            
            // 发送欢迎消息
            json welcome;
            welcome["type"] = "welcome";
            welcome["session_id"] = self->session_id_;
            welcome["timestamp"] = utils::DateTime::NowSeconds();
            welcome["message"] = "欢迎连接到IM服务器！";
            
            self->DoWrite(welcome.dump());
            
            // 开始读取消息
            self->DoRead();
        });
}


void WebSocketSession::DoRead() {
    auto self = shared_from_this();
    ws_.async_read(
        buffer_,
        [self](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                if (ec == websocket::error::closed) {
                    LOG_INFO("WebSocket连接已关闭: id={}", self->session_id_);
                } else {
                    LOG_ERROR("消息读取失败: {}", ec.message());
                }
                return;
            }
            
            self->UpdateActiveTime();
            
            // 处理消息
            std::string message = beast::buffers_to_string(self->buffer_.data());
            self->buffer_.consume(bytes_transferred);  // 清空缓冲区
            
            self->OnMessage(message);
            LOG_INFO("收到消息: id={}, 内容={}", self->session_id_, message);
            
            // 继续读取下一条消息
            self->DoRead();
        });
}


void WebSocketSession::DoWrite(const std::string& message) {
    if (message.empty()) {
        LOG_WARN("消息为空, 跳过发送");
        return;
    }

    auto self = shared_from_this();
    ws_.async_write(
        net::buffer(message),
        [self](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                LOG_ERROR("WebSocket写入失败: {}", ec.message());
                return;
            }
            
            self->UpdateActiveTime();
            LOG_INFO("消息发送成功: 字节数={}", bytes_transferred);
        });
}


void WebSocketSession::OnMessage(const std::string& message) {
    try {
        json data = json::parse(message);
        std::string type = data["type"].get<std::string>();
        
        if (type == "auth") {
            // 处理认证消息
            std::string token = data["token"].get<std::string>();
            
            // 验证令牌
            std::map<std::string, std::string> payload;
            std::string secret = utils::Config::GetInstance().GetString("security.jwt_secret", "your_jwt_secret");
            
            if (utils::Security::VerifyJWT(token, secret, payload)) {
                auto it = payload.find("user_id");
                if (it != payload.end()) {
                    int64_t user_id = std::stoll(it->second);
                    
                    // 更新会话状态
                    SetUserID(user_id);
                    authorized_ = true;
                    
                    // 将会话添加到用户会话映射
                    std::shared_ptr<WebSocketHandler> handler = handler_.lock();
                    if (handler) {
                        handler->AddUserSession(user_id, session_id_);
                        
                        // 通知用户上线状态
                        handler->NotifyUserStatusChange(user_id, "online");
                    }
                    
                    // 发送认证成功响应
                    json response;
                    response["type"] = "auth_response";
                    response["success"] = true;
                    response["user_id"] = user_id;
                    response["message"] = "认证成功";
                    response["timestamp"] = utils::DateTime::NowSeconds();
                    
                    DoWrite(response.dump());
                    LOG_INFO("WebSocket认证成功: session_id={}, user_id={}", session_id_, user_id);
                    return;
                }
            }
            
            // 认证失败
            json response;
            response["type"] = "auth_response";
            response["success"] = false;
            response["message"] = "认证失败，无效的令牌";
            response["timestamp"] = utils::DateTime::NowSeconds();
            
            DoWrite(response.dump());
            LOG_WARN("WebSocket认证失败: session_id={}", session_id_);
        }
        else if (type == "ping") {
            // 处理心跳消息
            json response;
            response["type"] = "pong";
            response["timestamp"] = utils::DateTime::NowSeconds();
            
            DoWrite(response.dump());
        }
        else if (type == "group_message") {
            // 处理群组消息
            int64_t group_id = 0;
            try {
                group_id = data["group_id"].get<int64_t>();
            } catch (const std::exception& e) {
                LOG_ERROR("解析群组消息目标群组ID失败: {}", e.what());
                
                json error_response;
                error_response["type"] = "error";
                error_response["code"] = 400;
                error_response["message"] = "无效的目标群组ID";
                error_response["timestamp"] = utils::DateTime::NowSeconds();
                
                DoWrite(error_response.dump());
                return;
            }
            
            std::string content;
            try {
                content = data["content"].get<std::string>();
            } catch (const std::exception& e) {
                LOG_ERROR("解析群组消息内容失败: {}", e.what());
                
                json error_response;
                error_response["type"] = "error";
                error_response["code"] = 400;
                error_response["message"] = "无效的消息内容";
                error_response["timestamp"] = utils::DateTime::NowSeconds();
                
                DoWrite(error_response.dump());
                return;
            }
            
            // 检查用户是否在群组中
            std::shared_ptr<WebSocketHandler> handler = handler_.lock();
            if (handler) {
                std::vector<int64_t> user_groups = handler->GetUserGroups(user_id_);
                if (std::find(user_groups.begin(), user_groups.end(), group_id) == user_groups.end()) {
                    // 用户不在群组中
                    json error_response;
                    error_response["type"] = "error";
                    error_response["code"] = 403;
                    error_response["message"] = "您不是该群组的成员";
                    error_response["timestamp"] = utils::DateTime::NowSeconds();
                    
                    DoWrite(error_response.dump());
                    LOG_WARN("非群组成员尝试发送消息: user_id={}, group_id={}", user_id_, group_id);
                    return;
                }
                
                // 构建转发消息
                json forward_message;
                forward_message["type"] = "group_message";
                forward_message["group_id"] = group_id;
                forward_message["from_user_id"] = user_id_;
                forward_message["content"] = content;
                forward_message["timestamp"] = utils::DateTime::NowSeconds();
                
                // 发送给群组的其他成员（排除发送者自己）
                int sent_count = handler->SendToGroup(group_id, forward_message.dump(), user_id_);
                
                // 发送确认响应
                json ack_response;
                ack_response["type"] = "group_message_ack";
                ack_response["success"] = true;
                ack_response["group_id"] = group_id;
                ack_response["message_id"] = data.value("message_id", 0);
                ack_response["timestamp"] = utils::DateTime::NowSeconds();
                
                DoWrite(ack_response.dump());
                LOG_INFO("群组消息已发送: user_id={}, group_id={}, sent_count={}", 
                        user_id_, group_id, sent_count);
                
                // 向发送者发送消息已送达确认
                int64_t message_id = data.value("message_id", 0);
                if (message_id > 0) {
                    handler->SendMessageAcknowledgement(user_id_, message_id, "delivered");
                }
            }
        }
        else if (type == "status_update") {
            // 处理状态更新消息
            std::string status;
            try {
                status = data["status"].get<std::string>();
            } catch (const std::exception& e) {
                LOG_ERROR("解析状态更新失败: {}", e.what());
                
                json error_response;
                error_response["type"] = "error";
                error_response["code"] = 400;
                error_response["message"] = "无效的状态参数";
                error_response["timestamp"] = utils::DateTime::NowSeconds();
                
                DoWrite(error_response.dump());
                return;
            }
            
            // 更新用户状态
            std::string status_key = "user:" + std::to_string(user_id_) + ":status";
            std::shared_ptr<WebSocketHandler> handler = handler_.lock();
            if (handler) {
                auto redis_client = handler->GetRedisClient();
                if (redis_client) {
                    redis_client->SetValue(status_key, status, 3600);  // 一小时过期
                    
                    // 通知其他用户状态变更
                    handler->NotifyUserStatusChange(user_id_, status);
                    
                    // 发送确认响应
                    json ack_response;
                    ack_response["type"] = "status_ack";
                    ack_response["success"] = true;
                    ack_response["timestamp"] = utils::DateTime::NowSeconds();
                    
                    DoWrite(ack_response.dump());
                    LOG_INFO("WebSocket用户状态更新: user_id={}, status={}", user_id_, status);
                }
            }
        }
        else if (type == "broadcast") {
            // 处理广播消息（仅限管理员）
            // 这里可以添加管理员权限检查
            std::string content;
            try {
                content = data["content"].get<std::string>();
            } catch (const std::exception& e) {
                LOG_ERROR("解析广播消息内容失败: {}", e.what());
                
                json error_response;
                error_response["type"] = "error";
                error_response["code"] = 400;
                error_response["message"] = "无效的广播内容";
                error_response["timestamp"] = utils::DateTime::NowSeconds();
                
                DoWrite(error_response.dump());
                return;
            }
            
            // 广播消息给所有用户
            std::shared_ptr<WebSocketHandler> handler = handler_.lock();
            if (handler) {
                json broadcast_message;
                broadcast_message["type"] = "system_broadcast";
                broadcast_message["from_user_id"] = user_id_;
                broadcast_message["content"] = content;
                broadcast_message["timestamp"] = utils::DateTime::NowSeconds();
                
                handler->Broadcast(broadcast_message.dump());
                
                // 发送确认响应
                json ack_response;
                ack_response["type"] = "broadcast_ack";
                ack_response["success"] = true;
                ack_response["timestamp"] = utils::DateTime::NowSeconds();
                
                DoWrite(ack_response.dump());
                LOG_INFO("WebSocket广播消息: from_user_id={}", user_id_);
            }
        }
        else if (type == "read_receipt") {
            // 处理已读回执
            int64_t message_id = 0;
            try {
                message_id = data["message_id"].get<int64_t>();
            } catch (const std::exception& e) {
                LOG_ERROR("解析已读回执消息ID失败: {}", e.what());
                
                json error_response;
                error_response["type"] = "error";
                error_response["code"] = 400;
                error_response["message"] = "无效的消息ID";
                error_response["timestamp"] = utils::DateTime::NowSeconds();
                
                DoWrite(error_response.dump());
                return;
            }
            
            int64_t sender_id = 0;
            try {
                sender_id = data["sender_id"].get<int64_t>();
            } catch (const std::exception& e) {
                LOG_ERROR("解析已读回执发送者ID失败: {}", e.what());
                
                json error_response;
                error_response["type"] = "error";
                error_response["code"] = 400;
                error_response["message"] = "无效的发送者ID";
                error_response["timestamp"] = utils::DateTime::NowSeconds();
                
                DoWrite(error_response.dump());
                return;
            }
            
            // 向原消息发送者发送已读确认
            std::shared_ptr<WebSocketHandler> handler = handler_.lock();
            if (handler) {
                if (handler->SendMessageAcknowledgement(sender_id, message_id, "read")) {
                    // 发送成功，通知当前用户
                    json ack_response;
                    ack_response["type"] = "read_receipt_ack";
                    ack_response["success"] = true;
                    ack_response["message_id"] = message_id;
                    ack_response["timestamp"] = utils::DateTime::NowSeconds();
                    
                    DoWrite(ack_response.dump());
                    LOG_INFO("已读回执已发送: from_user_id={}, to_user_id={}, message_id={}", 
                            user_id_, sender_id, message_id);
                } else {
                    // 发送失败，通知当前用户
                    json ack_response;
                    ack_response["type"] = "read_receipt_ack";
                    ack_response["success"] = false;
                    ack_response["message"] = "发送已读回执失败，原发送者可能不在线";
                    ack_response["message_id"] = message_id;
                    ack_response["timestamp"] = utils::DateTime::NowSeconds();
                    
                    DoWrite(ack_response.dump());
                    LOG_WARN("发送已读回执失败: from_user_id={}, to_user_id={}, message_id={}", 
                            user_id_, sender_id, message_id);
                }
            }
        }
        else {
            // 检查是否已授权
            if (!authorized_) {
                json response;
                response["type"] = "error";
                response["code"] = 401;
                response["message"] = "未授权，请先进行认证";
                response["timestamp"] = utils::DateTime::NowSeconds();
                
                DoWrite(response.dump());
                LOG_WARN("未授权的WebSocket消息: session_id={}, type={}", session_id_, type);
                return;
            }
            
            // 处理其他类型的消息
            if (type == "chat_message") {
                // 处理聊天消息
                int64_t to_user_id = 0;
                try {
                    to_user_id = data["to_user_id"].get<int64_t>();
                } catch (const std::exception& e) {
                    LOG_ERROR("解析聊天消息目标用户ID失败: {}", e.what());
                    
                    json error_response;
                    error_response["type"] = "error";
                    error_response["code"] = 400;
                    error_response["message"] = "无效的目标用户ID";
                    error_response["timestamp"] = utils::DateTime::NowSeconds();
                    
                    DoWrite(error_response.dump());
                    return;
                }
                
                std::string content;
                try {
                    content = data["content"].get<std::string>();
                } catch (const std::exception& e) {
                    LOG_ERROR("解析聊天消息内容失败: {}", e.what());
                    
                    json error_response;
                    error_response["type"] = "error";
                    error_response["code"] = 400;
                    error_response["message"] = "无效的消息内容";
                    error_response["timestamp"] = utils::DateTime::NowSeconds();
                    
                    DoWrite(error_response.dump());
                    return;
                }
                
                // 转发消息给目标用户
                std::shared_ptr<WebSocketHandler> handler = handler_.lock();
                if (handler) {
                    json forward_message;
                    forward_message["type"] = "chat_message";
                    forward_message["from_user_id"] = user_id_;
                    forward_message["content"] = content;
                    forward_message["timestamp"] = utils::DateTime::NowSeconds();
                    
                    if (handler->SendToUser(to_user_id, forward_message.dump())) {
                        // 发送成功，通知发送者
                        json ack_response;
                        ack_response["type"] = "message_ack";
                        ack_response["success"] = true;
                        ack_response["message_id"] = data.value("message_id", 0);
                        ack_response["timestamp"] = utils::DateTime::NowSeconds();
                        
                        DoWrite(ack_response.dump());
                        LOG_INFO("WebSocket消息转发成功: from_user_id={}, to_user_id={}", user_id_, to_user_id);
                        
                        // 向发送者发送消息已送达确认
                        int64_t message_id = data.value("message_id", 0);
                        if (message_id > 0) {
                            handler->SendMessageAcknowledgement(user_id_, message_id, "delivered");
                        }
                    } else {
                        // 用户不在线，通知发送者
                        json ack_response;
                        ack_response["type"] = "message_ack";
                        ack_response["success"] = false;
                        ack_response["message"] = "目标用户不在线，消息将稍后发送";
                        ack_response["message_id"] = data.value("message_id", 0);
                        ack_response["timestamp"] = utils::DateTime::NowSeconds();
                        
                        DoWrite(ack_response.dump());
                        LOG_INFO("WebSocket消息目标用户不在线: from_user_id={}, to_user_id={}", user_id_, to_user_id);
                        
                        // 这里可以添加离线消息存储逻辑
                        // TODO: 将消息存储到数据库或Redis中
                    }
                }
            }
            else {
                // 未知的消息类型
                LOG_WARN("未知的WebSocket消息类型: session_id={}, user_id={}, type={}", 
                        session_id_, user_id_, type);
                
                json error_response;
                error_response["type"] = "error";
                error_response["code"] = 400;
                error_response["message"] = "未知的消息类型";
                error_response["timestamp"] = utils::DateTime::NowSeconds();
                
                DoWrite(error_response.dump());
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("处理WebSocket消息失败: {}", e.what());
        
        json response;
        response["type"] = "error";
        response["code"] = 400;
        response["message"] = "无效的消息格式";
        response["timestamp"] = utils::DateTime::NowSeconds();
        
        DoWrite(response.dump());
    }
}

// WebSocketHandler 实现
WebSocketHandler::WebSocketHandler(std::shared_ptr<db::RedisClient> redis_client)
    : redis_client_(redis_client), running_(true) {
    LOG_INFO("WebSocketHandler initialized");
    
    // 启动会话管理线程
    cleanup_thread_ = std::thread([this]() {
        while (running_) {
            try {
                // 管理会话，清理过期会话
                ManageSessions();
                
                // 休眠一段时间再检查
                int check_interval = utils::Config::GetInstance().GetInt("websocket.session_check_interval_seconds", 60);
                std::this_thread::sleep_for(std::chrono::seconds(check_interval));
            } catch (const std::exception& e) {
                LOG_ERROR("WebSocket会话管理异常: {}", e.what());
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    });
}

WebSocketHandler::~WebSocketHandler() {
    // 关闭所有会话
    try {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        LOG_INFO("正在关闭所有WebSocket会话: count={}", sessions_.size());
        
        for (auto& session_pair : sessions_) {
            try {
                session_pair.second->Close();
            } catch (const std::exception& e) {
                LOG_ERROR("关闭WebSocket会话失败: session_id={}, error={}", 
                          session_pair.first, e.what());
            }
        }
        
        // 清空会话映射
        sessions_.clear();
        user_sessions_.clear();
    } catch (const std::exception& e) {
        LOG_ERROR("WebSocketHandler析构时发生错误: {}", e.what());
    }
}

void WebSocketHandler::HandleNewConnection(tcp::socket socket) {
    // 读取 HTTP 握手请求
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    try {
        http::read(socket, buffer, req);  // 读取请求
    } catch (const std::exception& e) {
        LOG_ERROR("读取HTTP握手请求失败: {}", e.what());
        return;
    }

    // 获取请求路径
    std::string path(req.target());  // 直接构造 std::string
    LOG_INFO("收到WebSocket连接请求: path={}", path);

    // 检查路径是否匹配
    if (path != "/ws") {
        LOG_WARN("路径不匹配，拒绝连接: path={}", path);
        return;
    }

    // 创建会话
    auto session = std::make_shared<WebSocketSession>(std::move(socket), path);
    session->SetHandler(shared_from_this());

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[session->GetID()] = session;
    }

    // 启动会话
    session->Start();
}


bool WebSocketHandler::SendToUser(int64_t user_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = user_sessions_.find(user_id);
    if (it == user_sessions_.end() || it->second.empty()) {
        return false;
    }
    
    bool sent = false;
    for (const auto& session_id : it->second) {
        auto session_it = sessions_.find(session_id);
        if (session_it != sessions_.end()) {
            try {
                session_it->second->Send(message);
                sent = true;
            } catch (const std::exception& e) {
                LOG_ERROR("发送WebSocket消息失败: {}", e.what());
            }
        }
    }
    
    return sent;
}

void WebSocketHandler::Broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    for (auto& session_pair : sessions_) {
        try {
            session_pair.second->Send(message);
        } catch (const std::exception& e) {
            LOG_ERROR("广播WebSocket消息失败: {}", e.what());
        }
    }
}

void WebSocketHandler::ManageSessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    int64_t now = utils::DateTime::NowSeconds();
    std::vector<std::string> expired_sessions;
    
    // 查找过期的会话
    for (auto& session_pair : sessions_) {
        if (session_pair.second->IsExpired(now)) {
            expired_sessions.push_back(session_pair.first);
        }
    }
    
    // 移除过期会话
    for (const auto& session_id : expired_sessions) {
        RemoveSession(session_id);
    }
    
    if (!expired_sessions.empty()) {
        LOG_INFO("清理过期WebSocket会话: count={}", expired_sessions.size());
    }
}

void WebSocketHandler::RemoveSession(const std::string& session_id) {
    auto session_it = sessions_.find(session_id);
    if (session_it != sessions_.end()) {
        int64_t user_id = session_it->second->GetUserID();
        
        // 关闭会话
        try {
            session_it->second->Close();
        } catch (const std::exception& e) {
            LOG_ERROR("关闭WebSocket会话失败: {}", e.what());
        }
        
        // 从用户会话映射中移除
        if (user_id > 0) {
            auto user_it = user_sessions_.find(user_id);
            if (user_it != user_sessions_.end()) {
                auto& sessions = user_it->second;
                sessions.erase(std::remove(sessions.begin(), sessions.end(), session_id), sessions.end());
                
                if (sessions.empty()) {
                    user_sessions_.erase(user_it);
                    
                    // 检查该用户是否还有其他连接方式（例如gRPC）
                    std::string online_key = "user:" + std::to_string(user_id) + ":online";
                    auto redis_client = GetRedisClient();
                    if (redis_client && redis_client->KeyExists(online_key)) {
                        // 用户可能还通过其他方式在线，例如gRPC
                    } else {
                        // 用户完全离线，可以通知其他用户
                        LOG_INFO("用户完全离线: user_id={}", user_id);
                        
                        // 更新用户最后在线时间
                        UpdateUserLastSeen(user_id);
                        
                        // 通知用户状态变更
                        NotifyUserStatusChange(user_id, "offline");
                    }
                }
            }
        }
        
        // 从会话映射中移除
        sessions_.erase(session_it);
        
        LOG_INFO("WebSocket会话已移除: id={}, user_id={}", session_id, user_id);
    }
}

void WebSocketHandler::AddUserSession(int64_t user_id, const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    user_sessions_[user_id].push_back(session_id);
    LOG_INFO("添加用户WebSocket会话: user_id={}, session_id={}", user_id, session_id);
}

size_t WebSocketHandler::GetUserSessionCount(int64_t user_id) const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = user_sessions_.find(user_id);
    if (it != user_sessions_.end()) {
        return it->second.size();
    }
    return 0;
}

std::vector<int64_t> WebSocketHandler::GetOnlineUsers() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::vector<int64_t> online_users;
    online_users.reserve(user_sessions_.size());
    
    for (const auto& user_pair : user_sessions_) {
        if (!user_pair.second.empty()) {
            online_users.push_back(user_pair.first);
        }
    }
    
    return online_users;
}

bool WebSocketHandler::IsUserOnline(int64_t user_id) const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = user_sessions_.find(user_id);
    return (it != user_sessions_.end() && !it->second.empty());
}

std::vector<std::shared_ptr<WebSocketSession>> WebSocketHandler::GetSessions(
    std::function<bool(const std::shared_ptr<WebSocketSession>&)> filter) const {
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::vector<std::shared_ptr<WebSocketSession>> result;
    result.reserve(sessions_.size());
    
    for (const auto& session_pair : sessions_) {
        if (!filter || filter(session_pair.second)) {
            result.push_back(session_pair.second);
        }
    }
    
    return result;
}

size_t WebSocketHandler::GetSessionCount() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

std::vector<int64_t> WebSocketHandler::GetUserFriends(int64_t user_id) const {
    std::vector<int64_t> friends;
    
    auto redis_client = GetRedisClient();
    if (redis_client) {
        std::string friends_key = "user:" + std::to_string(user_id) + ":friends";
        std::vector<std::string> friend_ids = redis_client->SetMembers(friends_key);
        
        for (const auto& id_str : friend_ids) {
            try {
                int64_t friend_id = std::stoll(id_str);
                friends.push_back(friend_id);
            } catch (const std::exception& e) {
                LOG_ERROR("解析好友ID失败: {}", e.what());
            }
        }
    }
    
    return friends;
}

int WebSocketHandler::SendToUserFriends(int64_t user_id, const std::string& message, int64_t exclude_user_id) {
    // 获取用户的好友列表
    std::vector<int64_t> friends = GetUserFriends(user_id);
    
    // 如果好友列表为空且不排除用户，则返回0
    if (friends.empty()) {
        return 0;
    }
    
    int sent_count = 0;
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // 遍历所有会话，向好友发送消息
    for (const auto& session_pair : sessions_) {
        if (!session_pair.second->IsAuthorized()) {
            continue;
        }
        
        int64_t session_user_id = session_pair.second->GetUserID();
        
        // 排除指定的用户ID
        if (exclude_user_id > 0 && session_user_id == exclude_user_id) {
            continue;
        }
        
        // 只向好友发送消息
        if (std::find(friends.begin(), friends.end(), session_user_id) != friends.end()) {
            try {
                session_pair.second->Send(message);
                sent_count++;
            } catch (const std::exception& e) {
                LOG_ERROR("向好友发送消息失败: user_id={}, friend_id={}, error={}", 
                          user_id, session_user_id, e.what());
            }
        }
    }
    
    return sent_count;
}

void WebSocketHandler::NotifyUserStatusChange(int64_t user_id, const std::string& status) {
    // 创建状态变更通知
    json notification;
    notification["type"] = "user_status";
    notification["user_id"] = user_id;
    notification["status"] = status;
    notification["timestamp"] = utils::DateTime::NowSeconds();
    
    // 向用户的好友发送状态变更通知（排除自己）
    int notification_count = SendToUserFriends(user_id, notification.dump(), user_id);
    
    LOG_INFO("用户状态变更通知已发送: user_id={}, status={}, notify_count={}", 
             user_id, status, notification_count);
             
    // 同时通知群组成员
    NotifyGroupUserStatusChange(user_id, status);
}

std::vector<std::shared_ptr<WebSocketSession>> WebSocketHandler::GetUserSessions(int64_t user_id) const {
    std::vector<std::shared_ptr<WebSocketSession>> user_sessions;
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // 查找用户ID对应的会话ID列表
    auto it = user_sessions_.find(user_id);
    if (it != user_sessions_.end()) {
        // 预分配空间以提高性能
        user_sessions.reserve(it->second.size());
        
        // 获取每个会话ID对应的会话对象
        for (const auto& session_id : it->second) {
            auto session_it = sessions_.find(session_id);
            if (session_it != sessions_.end()) {
                user_sessions.push_back(session_it->second);
            }
        }
    }
    
    return user_sessions;
}

std::shared_ptr<WebSocketSession> WebSocketHandler::GetSession(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

int WebSocketHandler::HandleZombieSessions(int64_t inactive_threshold_seconds) {
    int64_t now = utils::DateTime::NowSeconds();
    std::vector<std::string> zombie_sessions;
    
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        // 查找僵尸会话（长时间不活跃但未过期的会话）
        for (const auto& session_pair : sessions_) {
            auto session = session_pair.second;
            
            // 如果会话的最后活跃时间距离现在超过了阈值，但还未过期
            if ((now - session->GetLastActiveTime()) > inactive_threshold_seconds && 
                !session->IsExpired(now)) {
                zombie_sessions.push_back(session_pair.first);
            }
        }
    }
    
    // 处理僵尸会话
    int processed_count = 0;
    for (const auto& session_id : zombie_sessions) {
        LOG_WARN("检测到僵尸WebSocket会话: id={}", session_id);
        
        // 发送ping消息来检测会话是否真的断开
        try {
            auto session = GetSession(session_id);
            if (session) {
                json ping;
                ping["type"] = "ping";
                ping["timestamp"] = now;
                session->Send(ping.dump());
                LOG_INFO("向僵尸会话发送ping消息: id={}", session_id);
                processed_count++;
            }
        } catch (const std::exception& e) {
            // 如果发送失败，说明会话确实已断开，移除它
            LOG_ERROR("向僵尸会话发送消息失败，移除会话: id={}, error={}", session_id, e.what());
            RemoveSession(session_id);
            processed_count++;
        }
    }
    
    if (processed_count > 0) {
        LOG_INFO("处理僵尸WebSocket会话: count={}", processed_count);
    }
    
    return processed_count;
}

std::vector<int64_t> WebSocketHandler::GetUserGroups(int64_t user_id) const {
    std::vector<int64_t> groups;
    
    auto redis_client = GetRedisClient();
    if (redis_client) {
        std::string groups_key = "user:" + std::to_string(user_id) + ":groups";
        std::vector<std::string> group_ids = redis_client->SetMembers(groups_key);
        
        for (const auto& id_str : group_ids) {
            try {
                int64_t group_id = std::stoll(id_str);
                groups.push_back(group_id);
            } catch (const std::exception& e) {
                LOG_ERROR("解析群组ID失败: {}", e.what());
            }
        }
    }
    
    return groups;
}

std::vector<int64_t> WebSocketHandler::GetGroupMembers(int64_t group_id) const {
    std::vector<int64_t> members;
    
    auto redis_client = GetRedisClient();
    if (redis_client) {
        std::string members_key = "group:" + std::to_string(group_id) + ":members";
        std::vector<std::string> member_ids = redis_client->SetMembers(members_key);
        
        for (const auto& id_str : member_ids) {
            try {
                int64_t member_id = std::stoll(id_str);
                members.push_back(member_id);
            } catch (const std::exception& e) {
                LOG_ERROR("解析群组成员ID失败: {}", e.what());
            }
        }
    }
    
    return members;
}

int WebSocketHandler::SendToGroup(int64_t group_id, const std::string& message, int64_t exclude_user_id) {
    // 获取群组成员列表
    std::vector<int64_t> members = GetGroupMembers(group_id);
    
    // 如果群组没有成员，返回0
    if (members.empty()) {
        return 0;
    }
    
    int sent_count = 0;
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // 向群组的所有在线成员发送消息
    for (int64_t member_id : members) {
        // 排除指定的用户ID
        if (exclude_user_id > 0 && member_id == exclude_user_id) {
            continue;
        }
        
        // 获取该成员的所有会话
        auto it = user_sessions_.find(member_id);
        if (it != user_sessions_.end() && !it->second.empty()) {
            for (const auto& session_id : it->second) {
                auto session_it = sessions_.find(session_id);
                if (session_it != sessions_.end()) {
                    try {
                        session_it->second->Send(message);
                        sent_count++;
                    } catch (const std::exception& e) {
                        LOG_ERROR("向群组成员发送消息失败: group_id={}, member_id={}, error={}", 
                                  group_id, member_id, e.what());
                    }
                }
            }
        }
    }
    
    return sent_count;
}

void WebSocketHandler::NotifyGroupUserStatusChange(int64_t user_id, const std::string& status, int64_t group_id) {
    // 创建状态变更通知
    json notification;
    notification["type"] = "group_user_status";
    notification["user_id"] = user_id;
    notification["status"] = status;
    notification["timestamp"] = utils::DateTime::NowSeconds();
    
    std::vector<int64_t> groups;
    
    // 如果指定了群组ID，只通知该群组成员
    if (group_id > 0) {
        groups.push_back(group_id);
    } else {
        // 否则获取用户所在的所有群组
        groups = GetUserGroups(user_id);
    }
    
    // 如果没有群组，直接返回
    if (groups.empty()) {
        return;
    }
    
    std::string message = notification.dump();
    int total_notified = 0;
    
    // 向每个群组的成员发送通知
    for (int64_t curr_group_id : groups) {
        notification["group_id"] = curr_group_id;
        std::string group_message = notification.dump();
        
        int notified = SendToGroup(curr_group_id, group_message, user_id);
        total_notified += notified;
    }
    
    if (total_notified > 0) {
        LOG_INFO("群组用户状态变更通知已发送: user_id={}, status={}, groups={}, notify_count={}", 
                user_id, status, groups.size(), total_notified);
    }
}

int WebSocketHandler::SendToUsers(const std::vector<int64_t>& user_ids, const std::string& message) {
    if (user_ids.empty()) {
        return 0;
    }
    
    int sent_count = 0;
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // 遍历指定的用户ID列表
    for (int64_t user_id : user_ids) {
        auto it = user_sessions_.find(user_id);
        if (it != user_sessions_.end() && !it->second.empty()) {
            // 获取该用户的所有会话
            for (const auto& session_id : it->second) {
                auto session_it = sessions_.find(session_id);
                if (session_it != sessions_.end()) {
                    try {
                        session_it->second->Send(message);
                        sent_count++;
                        // 每个用户只统计一次，即使有多个会话
                        break;
                    } catch (const std::exception& e) {
                        LOG_ERROR("批量发送消息失败: user_id={}, error={}", user_id, e.what());
                    }
                }
            }
        }
    }
    
    LOG_INFO("批量消息已发送: total_users={}, success_count={}", user_ids.size(), sent_count);
    return sent_count;
}

void WebSocketHandler::UpdateUserLastSeen(int64_t user_id, int64_t timestamp) {
    if (user_id <= 0) {
        return;
    }
    
    // 如果未提供时间戳，使用当前时间
    if (timestamp <= 0) {
        timestamp = utils::DateTime::NowSeconds();
    }
    
    // 将用户最后在线时间存储到Redis
    auto redis_client = GetRedisClient();
    if (redis_client) {
        std::string last_seen_key = "user:" + std::to_string(user_id) + ":last_seen";
        
        // 将时间戳存储为字符串
        if (redis_client->SetValue(last_seen_key, std::to_string(timestamp))) {
            LOG_DEBUG("更新用户最后在线时间: user_id={}, timestamp={}", user_id, timestamp);
        } else {
            LOG_ERROR("更新用户最后在线时间失败: user_id={}", user_id);
        }
    }
}

int64_t WebSocketHandler::GetUserLastSeen(int64_t user_id) const {
    if (user_id <= 0) {
        return 0;
    }
    
    // 如果用户当前在线，返回当前时间
    if (IsUserOnline(user_id)) {
        return utils::DateTime::NowSeconds();
    }
    
    // 从Redis获取用户最后在线时间
    auto redis_client = GetRedisClient();
    if (redis_client) {
        std::string last_seen_key = "user:" + std::to_string(user_id) + ":last_seen";
        std::string last_seen_str = redis_client->GetValue(last_seen_key);
        
        if (!last_seen_str.empty()) {
            try {
                return std::stoll(last_seen_str);
            } catch (const std::exception& e) {
                LOG_ERROR("解析用户最后在线时间失败: user_id={}, error={}", user_id, e.what());
            }
        }
    }
    
    return 0;  // 未找到记录
}

bool WebSocketHandler::SendMessageAcknowledgement(int64_t to_user_id, int64_t message_id, const std::string& status) {
    if (to_user_id <= 0 || message_id <= 0) {
        LOG_ERROR("发送消息确认参数无效: to_user_id={}, message_id={}", to_user_id, message_id);
        return false;
    }
    
    // 创建确认消息
    json ack_message;
    ack_message["type"] = "message_acknowledgement";
    ack_message["message_id"] = message_id;
    ack_message["status"] = status;
    ack_message["timestamp"] = utils::DateTime::NowSeconds();
    
    // 发送确认消息给指定用户
    bool result = SendToUser(to_user_id, ack_message.dump());
    
    if (result) {
        LOG_INFO("消息确认已发送: to_user_id={}, message_id={}, status={}", 
                to_user_id, message_id, status);
    } else {
        LOG_WARN("消息确认发送失败，用户可能不在线: to_user_id={}, message_id={}", 
                to_user_id, message_id);
    }
    
    return result;
}

} // namespace im

 
 
