#include "include/server/server.h"
#include "include/server/push_notification_service.h"
#include "include/server/message_service.h"
#include "include/server/notification_service.h"
#include "include/server/utils/logger.h"
#include "include/server/utils/security.h"
#include "include/server/utils/datetime.h"
#include "include/server/utils/string_util.h"
#include "include/server/utils/jwt_verifier.h"
#include <nlohmann/json.hpp>
#include <string>
#include <sstream>

using json = nlohmann::json;

// 在 message_service.h 的开头部分添加：
namespace im {
    class IMServer;  // 前向声明
}

im::MessageServiceImpl::MessageServiceImpl(
    std::shared_ptr<im::db::MySQLConnection> mysql_conn,
    std::shared_ptr<im::db::RedisClient> redis_client,
    std::shared_ptr<im::kafka::KafkaProducer> kafka_producer,
    im::IMServer* server
) : mysql_conn_(mysql_conn),
    redis_client_(redis_client),
    kafka_producer_(kafka_producer),
    server_(server) {
    LOG_INFO("MessageService初始化");
}

grpc::Status im::MessageServiceImpl::SendMessage(grpc::ServerContext* context, const im::SendMessageRequest* request, im::SendMessageResponse* response) {
    // 验证请求
    if (request->from_user_id() == 0 || request->to_user_id() == 0 || request->content().empty()) {
        response->set_success(false);
        response->set_message("Invalid message format");
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Message format invalid");
    }
    
    // 验证token
    std::string token = this->GetAuthToken(context);
    int64_t sender_id;
    if (!this->ValidateToken(token, sender_id)) {
        response->set_success(false);
        response->set_message("Unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid token");
    }
    
    // 确保发送者ID匹配
    if (sender_id != request->from_user_id()) {
        response->set_success(false);
        response->set_message("User ID mismatch");
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "User ID in token does not match sender ID");
    }
    
    // 准备存储的消息
    im::Message stored_message;
    stored_message.set_message_id(0); // 将在存储时分配
    stored_message.set_from_user_id(request->from_user_id());
    stored_message.set_to_user_id(request->to_user_id());
    stored_message.set_message_type(request->message_type());
    stored_message.set_content(request->content());
    stored_message.set_send_time(im::utils::DateTime::NowMilliseconds());
    stored_message.set_is_read(false);
    stored_message.set_extra_info(request->extra_info());
    
    // 存储消息到数据库
    int64_t message_id = this->StoreMessage(stored_message);
    if (message_id == 0) {
        response->set_success(false);
        response->set_message("Failed to store message");
        return grpc::Status::OK;
    }
    
    // 设置返回的消息ID和时间
    response->set_success(true);
    response->set_message("Message sent successfully");
    response->set_message_id(message_id);
    response->set_send_time(stored_message.send_time());
    
    // 获取发送者和接收者
    int64_t from_user_id = stored_message.from_user_id();
    int64_t to_user_id = stored_message.to_user_id();
    
    // 将消息ID设置回消息对象
    stored_message.set_message_id(message_id);
    
    // 如果接收者在线，通过WebSocket发送消息
    if (server_) {
        server_->SendMessageToUser(to_user_id, stored_message);
    }
    
    // 存储离线消息
    this->StoreOfflineMessage(to_user_id, stored_message);
    
    // 如果接收者不在线且推送服务可用，发送推送通知
    auto push_service = server_ ? server_->GetPushNotificationService() : nullptr;
    if (push_service && !this->IsUserOnline(to_user_id)) {
        // 准备推送通知
        std::string sender_name = "用户";
        
        // 尝试获取发送者的用户名或昵称
        try {
            std::string sql = "SELECT nickname, username FROM users WHERE user_id = ?";
            auto results = this->mysql_conn_->ExecuteQuery(sql, {std::to_string(from_user_id)});
            
            if (!results.empty()) {
                if (!results[0]["nickname"].empty()) {
                    sender_name = results[0]["nickname"];
                } else if (!results[0]["username"].empty()) {
                    sender_name = results[0]["username"];
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("获取发送者信息失败: {}", e.what());
        }
        
        // 保留消息内容处理逻辑
        std::string title = sender_name + "发来新消息";
        std::string body;
        
        switch (stored_message.message_type()) {
            case static_cast<int>(im::NotificationMessageType::TEXT):
                body = stored_message.content();
                // 限制长度
                if (body.length() > 100) {
                    body = body.substr(0, 97) + "...";
                }
                break;
            case static_cast<int>(im::NotificationMessageType::IMAGE):
                body = "[图片]";
                break;
            // 其他消息类型...
            default:
                body = "[新消息]";
                break;
        }
        
        // 构建额外数据
        std::map<std::string, std::string> extra_data;
        extra_data["message_id"] = std::to_string(message_id);
        extra_data["from_user_id"] = std::to_string(from_user_id);
        extra_data["message_type"] = std::to_string(static_cast<int>(request->message_type()));
        
        if (!request->extra_info().empty()) {
            extra_data["extra_info"] = request->extra_info();
        }
        
        // 发送推送通知
        if (push_service) {
            push_service->SendPushNotification(
                to_user_id, title, body, im::PushNotificationType::MESSAGE, extra_data);
        }
    }
    
    // 另外，也可以发送到Kafka以供进一步处理
    json kafka_message;
    kafka_message["message_id"] = message_id;
    kafka_message["from_user_id"] = from_user_id;
    kafka_message["to_user_id"] = to_user_id;
    kafka_message["message_type"] = static_cast<int>(stored_message.message_type());
    kafka_message["content"] = stored_message.content();
    kafka_message["send_time"] = stored_message.send_time();
    
    this->kafka_producer_->SendMessage("im_messages", kafka_message.dump(), 
                               std::to_string(from_user_id) + "_" + std::to_string(to_user_id));
    
    return grpc::Status::OK;
}

grpc::Status im::MessageServiceImpl::GetMessageHistory(grpc::ServerContext* context, const im::GetMessageHistoryRequest* request, im::GetMessageHistoryResponse* response) {
    // 验证授权
    std::string token = this->GetAuthToken(context);
    int64_t user_id;
    if (!this->ValidateToken(token, user_id)) {
        response->set_success(false);
        response->set_message("Unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid token");
    }

    // 验证参数
    if (request->user_id() != user_id) {
        response->set_success(false);
        response->set_message("User ID doesn't match authenticated user");
        return grpc::Status::OK;
    }

    int64_t friend_id = request->friend_id();
    int64_t start_time = request->start_time();
    int64_t end_time = request->end_time();
    int32_t limit = request->limit();

    if (limit <= 0 || limit > 100) {
        limit = 20; // 默认值或限制最大值
    }

    // 计算唯一的聊天ID
    int64_t unique_chat_id = user_id < friend_id ? 
                           (user_id * 1000000000 + friend_id) : 
                           (friend_id * 1000000000 + user_id);

    // 尝试从Redis缓存获取最近消息
    std::string cache_key = "chat:personal:" + std::to_string(unique_chat_id) + ":messages";
    std::vector<std::string> cached_messages = redis_client_->GetList(cache_key, 0, limit - 1);

    if (!cached_messages.empty()) {
        // 使用缓存的消息
        for (const auto& msg_json : cached_messages) {
            try {
                nlohmann::json j = nlohmann::json::parse(msg_json);
                im::Message* msg = response->add_messages();
                msg->set_message_id(j["id"].get<int64_t>());
                msg->set_from_user_id(j["from_user_id"].get<int64_t>());
                msg->set_to_user_id(j["to_user_id"].get<int64_t>());
                msg->set_content(j["content"].get<std::string>());
                msg->set_message_type(static_cast<im::MessageType>(j["message_type"].get<int>()));
                msg->set_send_time(j["send_time"].get<int64_t>());
                msg->set_is_read(j["is_read"].get<bool>());
                if (j.contains("extra_info") && j["extra_info"].is_string()) {
                    msg->set_extra_info(j["extra_info"].get<std::string>());
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to parse cached message: {}", e.what());
            }
        }
    } else {
        // 从数据库获取消息历史
        try {
            std::string sql;
            std::vector<std::string> params;
            
            if (start_time > 0 && end_time > 0) {
                sql = "SELECT * FROM messages WHERE ((from_user_id = ? AND to_user_id = ?) OR (from_user_id = ? AND to_user_id = ?)) "
                      "AND send_time BETWEEN ? AND ? ORDER BY send_time DESC LIMIT ?";
                params = {
                    std::to_string(user_id),
                    std::to_string(friend_id),
                    std::to_string(friend_id),
                    std::to_string(user_id),
                    std::to_string(start_time),
                    std::to_string(end_time),
                    std::to_string(limit)
                };
            } else {
                sql = "SELECT * FROM messages WHERE ((from_user_id = ? AND to_user_id = ?) OR (from_user_id = ? AND to_user_id = ?)) "
                      "ORDER BY send_time DESC LIMIT ?";
                params = {
                    std::to_string(user_id),
                    std::to_string(friend_id),
                    std::to_string(friend_id),
                    std::to_string(user_id),
                    std::to_string(limit)
                };
            }
            
            db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, params);
            
            for (const auto& row : results) {
                try {
                    im::Message* msg = response->add_messages();
                    msg->set_message_id(std::stoll(row.at("id")));
                    msg->set_from_user_id(std::stoll(row.at("from_user_id")));
                    msg->set_to_user_id(std::stoll(row.at("to_user_id")));
                    msg->set_content(row.at("content"));
                    msg->set_message_type(static_cast<im::MessageType>(std::stoi(row.at("message_type"))));
                    msg->set_send_time(std::stoll(row.at("send_time")));
                    msg->set_is_read(std::stoi(row.at("is_read")) > 0);
                    if (row.find("extra_info") != row.end()) {
                        msg->set_extra_info(row.at("extra_info"));
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Failed to parse message from database: {}", e.what());
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to get message history: {}", e.what());
            response->set_success(false);
            response->set_message("Failed to get message history: " + std::string(e.what()));
            return grpc::Status::OK;
        }
    }

    response->set_success(true);
    response->set_message("Message history retrieved successfully");
    
    return grpc::Status::OK;
}

grpc::Status im::MessageServiceImpl::GetOfflineMessages(
    grpc::ServerContext* context,
    const im::GetOfflineMessagesRequest* request,
    im::GetOfflineMessagesResponse* response
) {
    // 验证授权
    std::string token = this->GetAuthToken(context);
    int64_t user_id;
    if (!this->ValidateToken(token, user_id)) {
        response->set_success(false);
        response->set_message("Unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid token");
    }

    // 从Redis获取离线消息
    std::string offline_key = "user:" + std::to_string(user_id) + ":offline_messages";
    std::vector<std::string> offline_messages = redis_client_->GetList(offline_key, 0, -1);
    
    // 将消息转换为Message对象
    for (const auto& msg_json : offline_messages) {
        try {
            nlohmann::json j = nlohmann::json::parse(msg_json);
            im::Message* msg = response->add_messages();
            msg->set_message_id(j["id"].get<int64_t>());
            msg->set_from_user_id(j["from_user_id"].get<int64_t>());
            msg->set_to_user_id(j["to_user_id"].get<int64_t>());
            msg->set_content(j["content"].get<std::string>());
            msg->set_message_type(static_cast<im::MessageType>(j["message_type"].get<int>()));
            msg->set_send_time(j["send_time"].get<int64_t>());
            msg->set_is_read(j["is_read"].get<bool>());
            if (j.contains("extra_info") && j["extra_info"].is_string()) {
                msg->set_extra_info(j["extra_info"].get<std::string>());
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse offline message: {}", e.what());
        }
    }
    
    // 如果请求中指定了需要清除离线消息，则从Redis中删除
    response->set_success(true);
    response->set_message("Offline messages retrieved successfully");
    
    return grpc::Status::OK;
}

grpc::Status im::MessageServiceImpl::MarkMessageRead(
    grpc::ServerContext* context,
    const im::MarkMessageReadRequest* request,
    im::MarkMessageReadResponse* response
) {
    // 验证授权
    std::string token = this->GetAuthToken(context);
    int64_t user_id;
    if (!this->ValidateToken(token, user_id)) {
        response->set_success(false);
        response->set_message("Unauthorized");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid token");
    }

    // 验证参数
    int64_t message_id = request->message_id();
    
    // 更新消息状态
    try {
        std::string sql = "UPDATE messages SET is_read = 1 WHERE id = ? AND to_user_id = ?";
        
        std::vector<std::string> params = {
            std::to_string(message_id),
            std::to_string(user_id)
        };
        
        bool success = mysql_conn_->ExecuteUpdate(sql, params);
        
        if (!success) {
            response->set_success(false);
            response->set_message("Failed to mark message as read");
            return grpc::Status::OK;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to mark message as read: {}", e.what());
        response->set_success(false);
        response->set_message("Database error");
        return grpc::Status::OK;
    }
    
    response->set_success(true);
    response->set_message("Message marked as read");
    
    return grpc::Status::OK;
}

grpc::Status im::MessageServiceImpl::MessageStream(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<im::Message, im::Message>* stream
) {
    // 验证授权
    std::string token = this->GetAuthToken(context);
    int64_t user_id;
    if (!this->ValidateToken(token, user_id)) {
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid token");
    }

    // 添加到活跃流列表
    AddActiveStream(user_id, stream);
    
    // 读取来自客户端的消息
    im::Message client_message;
    while (stream->Read(&client_message)) {
        // 设置发送者ID（以防客户端未设置或设置错误）
        client_message.set_from_user_id(user_id);
        client_message.set_send_time(utils::DateTime::NowMilliseconds());
        client_message.set_is_read(false);
        
        // 存储消息
        int64_t message_id = StoreMessage(client_message);
        if (message_id == 0) {
            LOG_ERROR("Failed to store message from user {}", user_id);
            continue;
        }
        
        // 设置消息ID
        client_message.set_message_id(message_id);
        
        // 获取接收者ID
        int64_t to_user_id = client_message.to_user_id();
            
        // 缓存消息到Redis
        CacheMessage("personal", user_id < to_user_id ? 
                    (user_id * 1000000000 + to_user_id) : 
                    (to_user_id * 1000000000 + user_id), 
                    client_message);

        // 发送消息到Kafka
        SendMessageToKafka(client_message, "personal");

        // 检查接收者是否在线
        if (IsUserOnline(to_user_id)) {
            // 通知接收者有新消息
            NotifyNewMessage(to_user_id, client_message);
        } else {
            // 存储离线消息
            StoreOfflineMessage(to_user_id, client_message);
        }
    }
    
    // 从活跃流列表中移除
    RemoveActiveStream(user_id, stream);
    
    return grpc::Status::OK;
}

void im::MessageServiceImpl::NotifyNewMessage(int64_t user_id, const im::Message& message) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    auto it = active_streams_.find(user_id);
    if (it != active_streams_.end()) {
        for (auto stream : it->second) {
            if (!stream->Write(message)) {
                LOG_ERROR("Failed to send notification to user {}", user_id);
            }
        }
    }
}

void im::MessageServiceImpl::AddActiveStream(int64_t user_id, grpc::ServerReaderWriter<Message, Message>* stream) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    active_streams_[user_id].push_back(stream);
    LOG_INFO("Added active stream for user {}, total streams: {}", user_id, active_streams_[user_id].size());
}

void im::MessageServiceImpl::RemoveActiveStream(int64_t user_id, grpc::ServerReaderWriter<Message, Message>* stream) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    
    auto it = active_streams_.find(user_id);
    if (it != active_streams_.end()) {
        auto& streams = it->second;
        streams.erase(std::remove(streams.begin(), streams.end(), stream), streams.end());
        
        if (streams.empty()) {
            active_streams_.erase(it);
        }
        
        LOG_INFO("Removed active stream for user {}", user_id);
    }
}

std::string im::MessageServiceImpl::GetAuthToken(grpc::ServerContext* context) {
    auto metadata = context->client_metadata();
    auto token_iter = metadata.find("authorization");
    if (token_iter != metadata.end()) {
        std::string token(token_iter->second.data(), token_iter->second.size());
        // 移除可能的前缀，如"Bearer "
        if (token.substr(0, 7) == "Bearer ") {
            token = token.substr(7);
        }
        return token;
    }
    return "";
}

bool im::MessageServiceImpl::ValidateToken(const std::string& token, int64_t& user_id) {
    try {
        if (token.empty()) {
            return false;
        }
        
        // 使用JWT验证工具验证令牌
        std::map<std::string, std::string> payload;
        utils::Security::VerifyJWT(token, "", payload); 
        
        // 检查用户ID声明
        auto it = payload.find("user_id");
        if (it != payload.end()) {
            user_id = std::stoll(it->second);
            return true;
        }
        
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("Token验证失败: {}", e.what());
        return false;
    }
}

int64_t im::MessageServiceImpl::StoreMessage(const im::Message& message) {
    try {
        std::string sql = "INSERT INTO messages (from_user_id, to_user_id, content, message_type, send_time, is_read, extra_info) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?)";
             
        std::vector<std::string> params = {
            std::to_string(message.from_user_id()),
            std::to_string(message.to_user_id()),
            message.content(),
            std::to_string(static_cast<int>(message.message_type())),
            std::to_string(message.send_time()),
            message.is_read() ? "1" : "0",
            message.extra_info()
        };
        
        return mysql_conn_->ExecuteInsert(sql, params);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to store message: {}", e.what());
        return 0;
    }
}

void im::MessageServiceImpl::CacheMessage(const std::string& chat_type, int64_t chat_id, const im::Message& message) {
    try {
        // 构建缓存键
        std::string cache_key = "chat:" + chat_type + ":" + std::to_string(chat_id) + ":messages";
        
        // 构建JSON数据
        json json_data;
        json_data["id"] = message.message_id();
        json_data["from_user_id"] = message.from_user_id();
        json_data["to_user_id"] = message.to_user_id();
        json_data["content"] = message.content();
        json_data["message_type"] = static_cast<int>(message.message_type());
        json_data["send_time"] = message.send_time();
        json_data["is_read"] = message.is_read();
        json_data["extra_info"] = message.extra_info();
        
        // 添加到Redis缓存（最新消息在前）
        redis_client_->PushFront(cache_key, json_data.dump());
        
        // 限制缓存大小，只保留最近的100条消息
        int64_t list_length = redis_client_->ListLength(cache_key);
        if (list_length > 100) {
            redis_client_->ListTrim(cache_key, 0, 99);
        }
        
        // 设置过期时间（7天）
        redis_client_->SetExpire(cache_key, 7 * 86400);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to cache message: {}", e.what());
    }
}

void im::MessageServiceImpl::SendMessageToKafka(const im::Message& message, const std::string& chat_type) {
    try {
        // 构建Kafka消息
        json json_data;
        json_data["id"] = message.message_id();
        json_data["from_user_id"] = message.from_user_id();
        json_data["to_user_id"] = message.to_user_id();
        json_data["content"] = message.content();
        json_data["message_type"] = static_cast<int>(message.message_type());
        json_data["send_time"] = message.send_time();
        json_data["is_read"] = message.is_read();
        json_data["extra_info"] = message.extra_info();
        
        // 发送到Kafka，使用不同主题区分个人聊天和群聊
        std::string topic = "im_messages_" + chat_type;
        std::string key = std::to_string(message.from_user_id()) + "_" + std::to_string(message.to_user_id());
                        
        kafka_producer_->SendMessage(topic, json_data.dump(), key);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to send message to Kafka: {}", e.what());
    }
}

bool im::MessageServiceImpl::IsUserOnline(int64_t user_id) const {
    try {
        std::string key = "user:" + std::to_string(user_id) + ":online";
        return redis_client_->KeyExists(key);
    } catch (const std::exception& e) {
        LOG_ERROR("检查用户在线状态失败: {}", e.what());
        return false;
    }
}

void im::MessageServiceImpl::StoreOfflineMessage(int64_t user_id, const im::Message& message) {
    try {
        // 构建离线消息键
        std::string offline_key = "user:" + std::to_string(user_id) + ":offline_messages";
        
        // 构建JSON数据
        json json_data;
        json_data["id"] = message.message_id();
        json_data["from_user_id"] = message.from_user_id();
        json_data["to_user_id"] = message.to_user_id();
        json_data["content"] = message.content();
        json_data["message_type"] = static_cast<int>(message.message_type());
        json_data["send_time"] = message.send_time();
        json_data["is_read"] = message.is_read();
        json_data["extra_info"] = message.extra_info();
        
        // 添加到Redis离线消息列表
        redis_client_->PushBack(offline_key, json_data.dump());
        
        // 设置过期时间（30天）
        redis_client_->SetExpire(offline_key, 30 * 86400);
        
        // 发送到Kafka离线消息主题
        kafka_producer_->SendMessage("im_offline_messages", json_data.dump(), std::to_string(user_id));
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to store offline message: {}", e.what());
    }
}

void im::MessageServiceImpl::LogDebug(const std::string& message) const {
    LOG_DEBUG("MessageService: {}", message);
}

void im::MessageServiceImpl::LogError(const std::string& message) const {
    LOG_ERROR("MessageService: {}", message);
}
