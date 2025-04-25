#include "include/server/server.h"
#include "include/server/push_notification_service.h"
#include "include/server/notification_service.h"
#include "include/server/message_service.h"
#include "include/server/utils/logger.h"
#include "include/server/utils/security.h"
#include "include/server/utils/datetime.h"
#include "include/server/utils/config.h"
#include "include/server/utils/string_util.h"
#include "include/server/utils/jwt_verifier.h"
#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>
#include <thread>
#include <chrono>

namespace im {

using json = nlohmann::json;

// 添加NotificationDoneCallback类的正确定义
class NotificationServiceImpl::NotificationDoneCallback {
public:
    NotificationDoneCallback(im::NotificationServiceImpl* service, int64_t user_id, 
                    grpc::ServerWriter<Message>* writer, grpc::ServerContext* ctx)
            : service_(service), user_id_(user_id), writer_(writer), context_(ctx) {}
    
    ~NotificationDoneCallback() = default;
    
    // 静态回调方法，符合gRPC要求
    static void Done(void* tag, bool ok) {
        // 接管内存所有权并处理
        std::unique_ptr<NotificationDoneCallback> callback(
            static_cast<NotificationDoneCallback*>(tag));
        callback->HandleDone(ok);
    }
    
    void HandleDone(bool ok) {
        // 检查上下文是否已被取消
        const bool cancelled = !ok || context_->IsCancelled();
        if (cancelled) {
            LOG_INFO("通知流被取消，用户: {}", user_id_);
        } else {
            LOG_INFO("通知流正常完成，用户: {}", user_id_);
        }
        
        // 清理流
        service_->RemoveActiveStream(user_id_, writer_);
    }
    
private:
    im::NotificationServiceImpl* service_; 
    int64_t user_id_;
    grpc::ServerWriter<Message>* writer_;  
    grpc::ServerContext* context_;  
};

im::NotificationServiceImpl::NotificationServiceImpl(
    std::shared_ptr<im::db::MySQLConnection> mysql_conn,
    std::shared_ptr<im::db::RedisClient> redis_client,
    std::shared_ptr<im::kafka::KafkaProducer> kafka_producer,
    im::MessageServiceImpl* message_service,
    im::UserServiceImpl* user_service,
    im::IMServer* server
) : mysql_conn_(mysql_conn),
    redis_client_(redis_client),
    kafka_producer_(kafka_producer),
    message_service_(message_service),
    user_service_(user_service),
    server_(server),
    running_(true) {
    
    LOG_INFO("NotificationServiceImpl initialized");
    
    // 启动Kafka消费线程
    kafka_consumer_thread_ = std::thread(&NotificationServiceImpl::KafkaConsumerThread, this);
}

NotificationServiceImpl::~NotificationServiceImpl() {
    running_ = false;
    if (kafka_consumer_thread_.joinable()) {
        kafka_consumer_thread_.join();
    }
    LOG_INFO("NotificationServiceImpl destroyed");
}

grpc::Status NotificationServiceImpl::SubscribeNotifications(
        grpc::ServerContext* context, 
        const SubscriptionRequest* request,
        grpc::ServerWriter<Message>* writer) {
    // 验证令牌
    int64_t current_user_id;
    if (!ValidateToken(context, current_user_id)) {
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "身份验证失败");
    }
    
    // 验证用户身份
    if (request->user_id() != current_user_id) {
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "无权订阅其他用户的通知");
    }
    
    // 注册通知流
    AddActiveStream(current_user_id, writer);
    
    // 设置完成事件回调
    auto callback = new NotificationDoneCallback(this, current_user_id, writer, context);
    context->AsyncNotifyWhenDone(callback);
    
    // 发送欢迎消息
    Message welcome_msg;
    welcome_msg.set_message_id(0);
    welcome_msg.set_from_user_id(0);  // 系统消息
    welcome_msg.set_to_user_id(current_user_id);
    welcome_msg.set_message_type(static_cast<im::MessageType>(im::MessageType::TEXT));
    welcome_msg.set_content("欢迎回来！您已成功连接到通知服务。");
    welcome_msg.set_send_time(utils::DateTime::NowSeconds());
    welcome_msg.set_is_read(false);
    welcome_msg.set_extra_info("{\"type\": \"system_notification\", \"category\": \"welcome\"}");
    
    if (!writer->Write(welcome_msg)) {
        RemoveActiveStream(current_user_id, writer);
        return grpc::Status(grpc::StatusCode::INTERNAL, "写入欢迎消息失败");
    }
    
    // 查询未读通知
    try {
        std::string sql = "SELECT id, type, content, create_time, is_read "
                         "FROM notifications "
                         "WHERE user_id = ? AND is_read = 0 "
                         "ORDER BY create_time DESC LIMIT 10";
        
        im::db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {std::to_string(current_user_id)});
        
        for (const auto& notification : results) {
            Message notify_msg;
            // MySQL结果是std::map<std::string, std::string>，直接使用字符串
            std::string id_str = notification.at("id");
            notify_msg.set_message_id(std::stoll(id_str));
            notify_msg.set_from_user_id(0);  // 系统消息
            notify_msg.set_to_user_id(current_user_id);
            notify_msg.set_message_type(static_cast<im::MessageType>(im::MessageType::TEXT));
            notify_msg.set_content(notification.at("content"));
            
            std::string create_time_str = notification.at("create_time");
            notify_msg.set_send_time(std::stoll(create_time_str));
            notify_msg.set_is_read(false);
            
            // 构造额外信息
            json extra;
            extra["type"] = "system_notification";
            extra["category"] = notification.at("type");
            extra["notification_id"] = notification.at("id");
            notify_msg.set_extra_info(extra.dump());
            
            if (!writer->Write(notify_msg)) {
                RemoveActiveStream(current_user_id, writer);
                return grpc::Status(grpc::StatusCode::INTERNAL, "写入未读通知失败");
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("获取未读通知失败: {}", e.what());
    }
    
    // 从Redis获取通知
    try {
        std::string notification_key = "user:" + std::to_string(current_user_id) + ":notifications";
        std::vector<std::string> notifications = redis_client_->ListRange(notification_key, 0, 9);
        
        for (const auto& notification_json : notifications) {
            try {
                // 注意：这里的notification是json对象，与MySQL结果不同
                json notification = json::parse(notification_json);
                
                Message notify_msg;
                // 根据不同类型进行处理
                int64_t msg_id = 0;
                
                if (notification.contains("id")) {
                    if (notification["id"].is_string()) {
                        msg_id = std::stoll(notification["id"].get<std::string>());
                    } else if (notification["id"].is_number()) {
                        msg_id = notification["id"].get<int64_t>();
                    }
                }
                
                notify_msg.set_message_id(msg_id);
                notify_msg.set_from_user_id(0);  // 系统消息
                notify_msg.set_to_user_id(current_user_id);
                notify_msg.set_message_type(im::MessageType::TEXT);
                
                std::string type = "general";
                if (notification.contains("type") && notification["type"].is_string()) {
                    type = notification["type"].get<std::string>();
                }
                
                if (type == "friend_request") {
                    int64_t from_user_id = 0;
                    std::string message_str;
                    
                    if (notification.contains("from_user_id")) {
                        if (notification["from_user_id"].is_number()) {
                            from_user_id = notification["from_user_id"].get<int64_t>();
                        } else if (notification["from_user_id"].is_string()) {
                            from_user_id = std::stoll(notification["from_user_id"].get<std::string>());
                        }
                    }
                    
                    if (notification.contains("message") && notification["message"].is_string()) {
                        message_str = notification["message"].get<std::string>();
                    }
                    
                    notify_msg.set_content("用户 " + std::to_string(from_user_id) + 
                                         " 发送了好友请求：" + message_str);
                    
                    // 设置额外信息
                    json extra;
                    extra["type"] = "friend_request";
                    extra["from_user_id"] = from_user_id;
                    
                    if (notification.contains("request_id")) {
                        if (notification["request_id"].is_string()) {
                            extra["request_id"] = notification["request_id"].get<std::string>();
                        } else if (notification["request_id"].is_number()) {
                            extra["request_id"] = notification["request_id"].get<int64_t>();
                        }
                    }
                    
                    notify_msg.set_extra_info(extra.dump());
                } 
                else if (type == "file_transfer_request") {
                    int64_t from_user_id = 0;
                    std::string file_name;
                    
                    if (notification.contains("from_user_id")) {
                        if (notification["from_user_id"].is_number()) {
                            from_user_id = notification["from_user_id"].get<int64_t>();
                        } else if (notification["from_user_id"].is_string()) {
                            from_user_id = std::stoll(notification["from_user_id"].get<std::string>());
                        }
                    }
                    
                    if (notification.contains("file_name") && notification["file_name"].is_string()) {
                        file_name = notification["file_name"].get<std::string>();
                    }
                    
                    notify_msg.set_content("用户 " + std::to_string(from_user_id) + 
                                         " 想要发送文件：" + file_name);
                    
                    // 设置额外信息
                    json extra;
                    extra["type"] = "file_transfer_request";
                    extra["from_user_id"] = from_user_id;
                    
                    if (notification.contains("request_id")) {
                        if (notification["request_id"].is_string()) {
                            extra["request_id"] = notification["request_id"].get<std::string>();
                        } else if (notification["request_id"].is_number()) {
                            extra["request_id"] = notification["request_id"].get<int64_t>();
                        }
                    }
                    
                    extra["file_name"] = file_name;
                    
                    if (notification.contains("file_size")) {
                        if (notification["file_size"].is_number()) {
                            extra["file_size"] = notification["file_size"].get<int64_t>();
                        } else if (notification["file_size"].is_string()) {
                            extra["file_size"] = notification["file_size"].get<std::string>();
                        }
                    }
                    
                    notify_msg.set_extra_info(extra.dump());
                } 
                else {
                    // 其他类型通知
                    if (notification.contains("content") && notification["content"].is_string()) {
                        notify_msg.set_content(notification["content"].get<std::string>());
                    } else {
                        notify_msg.set_content("您有一条新通知");
                    }
                    notify_msg.set_extra_info(notification_json);
                }
                
                // 处理时间戳
                int64_t timestamp = utils::DateTime::NowSeconds();
                if (notification.contains("timestamp")) {
                    if (notification["timestamp"].is_number()) {
                        timestamp = notification["timestamp"].get<int64_t>();
                    } else if (notification["timestamp"].is_string()) {
                        timestamp = std::stoll(notification["timestamp"].get<std::string>());
                    }
                }
                
                notify_msg.set_send_time(timestamp);
                notify_msg.set_is_read(false);
                
                if (!writer->Write(notify_msg)) {
                    RemoveActiveStream(current_user_id, writer);
                    return grpc::Status(grpc::StatusCode::INTERNAL, "写入Redis通知失败");
                }
            } catch (const std::exception& e) {
                LOG_ERROR("解析通知JSON失败: {}", e.what());
                continue;
            }
        }
        
        // 标记通知为已读
        if (!notifications.empty()) {
            redis_client_->ListTrim(notification_key, notifications.size(), -1);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("获取Redis通知失败: {}", e.what());
    }
    
    // 设置用户在线状态
    std::string online_key = "user:" + std::to_string(current_user_id) + ":online";
    redis_client_->SetValue(online_key, "1", 3600);  // 1小时过期
    
    // 阻塞直到客户端断开连接
    while (!context->IsCancelled()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 定期更新在线状态
        redis_client_->SetValue(online_key, "1", 3600);
    }
    
    return grpc::Status::OK;
}

void NotificationServiceImpl::SendNotification(int64_t user_id, const Notification& notification) {
    // 记录通知发送
    LOG_INFO("发送通知给用户 {}: {}", user_id, notification.content);
    
    // 存储通知到数据库
    StoreNotification(user_id, notification);
    
    // 向活跃连接发送通知
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        auto it = active_streams_.find(user_id);
        if (it != active_streams_.end()) {
            // 向所有活跃的流发送通知
            for (auto writer : it->second) {
                Message message = ConvertToMessage(notification);
                if (!writer->Write(message)) {
                    LOG_ERROR("发送通知到用户 {} 的流失败", user_id);
                }
            }
        }
    }
    
    // 如果服务器实例存在，尝试发送推送通知
    if (server_) {
        auto push_service = server_->GetPushNotificationService();
        if (push_service) {
            // 准备推送通知数据
            std::string title;
            std::string body = notification.content;
            PushNotificationType push_type;
            
            // 准备额外的数据映射
            std::map<std::string, std::string> extra_data;
            extra_data["notification_id"] = std::to_string(notification.notification_id);
            extra_data["type"] = std::to_string(static_cast<int>(notification.type));
            extra_data["sender_id"] = std::to_string(notification.sender_id);
            extra_data["created_at"] = std::to_string(notification.created_time);
            
            if (!notification.extra_info.empty()) {
                try {
                    json extra = json::parse(notification.extra_info);
                    if (extra.contains("category") && extra["category"].is_string()) {
                        std::string type = extra["category"].get<std::string>();
                        extra_data["category"] = type;
                    }
                    
                    // 将其他数据添加到推送数据中
                    for (auto& [key, value] : extra.items()) {
                        if (value.is_string()) {
                            extra_data[key] = value.get<std::string>();
                        } else {
                            extra_data[key] = value.dump();
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("解析通知额外信息失败: {}", e.what());
                }
            }
            
            // 根据通知类型设置标题和推送类型
            switch (notification.type) {
                case NotificationType::MESSAGE:
                    title = "新消息";
                    push_type = PushNotificationType::MESSAGE;
                    break;
                case NotificationType::FRIEND_REQUEST:
                    title = "好友请求";
                    push_type = PushNotificationType::FRIEND_REQUEST;
                    break;
                case NotificationType::FRIEND_ACCEPTED:
                    title = "好友请求已接受";
                    push_type = PushNotificationType::FRIEND_ACCEPTED;
                    break;
                case NotificationType::GROUP_INVITATION:
                    title = "群组邀请";
                    push_type = PushNotificationType::GROUP_INVITATION;
                    break;
                case NotificationType::FILE_TRANSFER:
                    title = "文件传输";
                    push_type = PushNotificationType::FILE_TRANSFER;
                    break;
                default:
                    title = "系统通知";
                    push_type = PushNotificationType::SYSTEM;
                    break;
            }
            
            // 发送推送通知
            if (push_service) {
            push_service->SendPushNotification(user_id, title, body, push_type, extra_data);
        }
        }
    }
    
    // 尝试通过Redis发布通知，供WebSocket服务监听
    try {
        json redis_notification;
        redis_notification["content"] = notification.content;
        redis_notification["type"] = "system_notification";
        redis_notification["timestamp"] = std::to_string(notification.created_time);
        redis_notification["id"] = std::to_string(notification.notification_id);
        
        try {
            if (!notification.extra_info.empty()) {
                json extra = json::parse(notification.extra_info);
                for (auto& [key, value] : extra.items()) {
                    redis_notification[key] = value;
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("解析通知额外信息失败: {}", e.what());
        }
        
        // 发布到Redis
        std::string channel = "user:" + std::to_string(user_id) + ":notification";
        redis_client_->Publish(channel, redis_notification.dump());
        
    } catch (const std::exception& e) {
        LOG_ERROR("发布通知到Redis失败: {}", e.what());
    }
}

void NotificationServiceImpl::BroadcastNotification(const Message& notification) {
    // 向所有活跃的用户发送通知
    std::lock_guard<std::mutex> lock(streams_mutex_);
    for (auto& user_streams : active_streams_) {
        for (auto writer : user_streams.second) {
            if (!writer->Write(notification)) {
                LOG_ERROR("广播通知到用户 {} 的流失败", user_streams.first);
            }
        }
    }
    
    // 将系统公告存储到数据库，以便离线用户登录时查看
    try {
        std::string sql = "INSERT INTO system_announcements (title, content, sender_id, create_time) "
                        "VALUES (?, ?, ?, ?)";
        
        int64_t now = utils::DateTime::NowSeconds();
        std::string title = "系统通知";
        
        try {
            json extra = json::parse(notification.extra_info());
            if (extra.contains("title") && extra["title"].is_string()) {
                title = extra["title"].get<std::string>();
            }
        } catch (const std::exception& e) {
            LOG_ERROR("解析通知额外信息失败: {}", e.what());
        }
        
        mysql_conn_->ExecuteInsert(sql, {
            title,
            notification.content(),
            std::to_string(notification.from_user_id()),
            std::to_string(now)
        });
    } catch (const std::exception& e) {
        LOG_ERROR("存储系统公告失败: {}", e.what());
    }
}

bool NotificationServiceImpl::ValidateToken(grpc::ServerContext* context, int64_t& user_id) {
    try {
        std::string token = GetAuthToken(context);
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

std::string NotificationServiceImpl::GetAuthToken(grpc::ServerContext* context) {
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

bool NotificationServiceImpl::IsUserOnline(int64_t user_id) const {
    // 简化实现，直接检查Redis键是否存在
    // 不使用streams_mutex_，避免const方法中使用非const mutex的问题
    std::string online_key = "user:" + std::to_string(user_id) + ":online";
    return redis_client_->KeyExists(online_key);
}

void NotificationServiceImpl::KafkaConsumerThread() {
    // TODO: 实现Kafka消费逻辑
    // 此处需要使用Kafka消费API订阅im_events主题
    // 并处理各种事件通知
    
    LOG_INFO("Kafka消费线程启动");
    
    while (running_) {
        try {
            // 模拟Kafka消费逻辑
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 在实际应用中，这里应该是从Kafka接收消息
            // 并调用HandleKafkaEvent处理
        } catch (const std::exception& e) {
            LOG_ERROR("Kafka消费线程异常: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    LOG_INFO("Kafka消费线程退出");
}

void NotificationServiceImpl::HandleKafkaEvent(const std::string& event_json) {
    using json = nlohmann::json;
    
    try {
        // 解析JSON
        json notification = json::parse(event_json);
        
        if (!notification.contains("type") || !notification.contains("user_id")) {
            LOG_ERROR("Invalid notification format: missing required fields");
            return;
        }
        
        std::string type = notification["type"].get<std::string>();
        int64_t current_user_id = std::stoll(notification["user_id"].get<std::string>());
        
        if (type == "friend_request") {
            int64_t from_user_id = 0;
            
            if (notification.contains("from_user_id")) {
                if (notification["from_user_id"].is_string()) {
                    from_user_id = std::stoll(notification["from_user_id"].get<std::string>());
                } else if (notification["from_user_id"].is_number()) {
                    from_user_id = notification["from_user_id"].get<int64_t>();
                }
            }
            
            // 创建系统消息通知
            Message notify_msg;
            int64_t msg_id = 0;
            
            if (notification.contains("id")) {
                if (notification["id"].is_string()) {
                    msg_id = std::stoll(notification["id"].get<std::string>());
                } else if (notification["id"].is_number()) {
                    msg_id = notification["id"].get<int64_t>();
                }
            }
            
            notify_msg.set_message_id(msg_id);
            notify_msg.set_from_user_id(0);  // 系统消息
            notify_msg.set_to_user_id(current_user_id);
            notify_msg.set_message_type(im::MessageType::TEXT);
            
            std::string type = "general";
            if (notification.contains("type") && notification["type"].is_string()) {
                type = notification["type"].get<std::string>();
            }
            
            // 设置消息内容
            if (type == "friend_request") {
                std::string content = "您收到一个好友请求";
                if (notification.contains("content") && notification["content"].is_string()) {
                    content = notification["content"].get<std::string>();
                }
                notify_msg.set_content(content);
                
                // 构建额外信息JSON
                json extra;
                extra["type"] = "friend_request";
                extra["from_user_id"] = from_user_id;
                
                if (notification.contains("request_id")) {
                    if (notification["request_id"].is_string()) {
                        extra["request_id"] = notification["request_id"].get<std::string>();
                    } else if (notification["request_id"].is_number()) {
                        extra["request_id"] = notification["request_id"].get<int64_t>();
                    }
                }
                
                notify_msg.set_extra_info(extra.dump());
            } 
            else if (type == "file_transfer_request") {
                int64_t from_user_id = 0;
                std::string file_name;
                
                if (notification.contains("from_user_id")) {
                    if (notification["from_user_id"].is_string()) {
                        from_user_id = std::stoll(notification["from_user_id"].get<std::string>());
                    } else if (notification["from_user_id"].is_number()) {
                        from_user_id = notification["from_user_id"].get<int64_t>();
                    }
                }
                
                if (notification.contains("file_name") && notification["file_name"].is_string()) {
                    file_name = notification["file_name"].get<std::string>();
                }
                
                std::string content = "您收到一个文件传输请求";
                if (notification.contains("content") && notification["content"].is_string()) {
                    content = notification["content"].get<std::string>();
                }
                notify_msg.set_content(content);
                
                // 构建额外信息JSON
                json extra;
                extra["type"] = "file_transfer_request";
                extra["from_user_id"] = from_user_id;
                
                if (notification.contains("request_id")) {
                    if (notification["request_id"].is_string()) {
                        extra["request_id"] = notification["request_id"].get<std::string>();
                    } else if (notification["request_id"].is_number()) {
                        extra["request_id"] = notification["request_id"].get<int64_t>();
                    }
                }
                
                extra["file_name"] = file_name;
                
                if (notification.contains("file_size")) {
                    if (notification["file_size"].is_number()) {
                        extra["file_size"] = notification["file_size"].get<int64_t>();
                    } else if (notification["file_size"].is_string()) {
                        extra["file_size"] = std::stoll(notification["file_size"].get<std::string>());
                    }
                }
                
                notify_msg.set_extra_info(extra.dump());
            } 
            else {
                // 其他类型通知
                if (notification.contains("content") && notification["content"].is_string()) {
                    notify_msg.set_content(notification["content"].get<std::string>());
                } else {
                    notify_msg.set_content("您有一条新的系统通知");
                }
                
                json extra;
                extra["type"] = type;
                
                if (notification.contains("extra") && notification["extra"].is_object()) {
                    for (auto& [key, value] : notification["extra"].items()) {
                        extra[key] = value;
                    }
                }
                
                notify_msg.set_extra_info(extra.dump());
            }
            
            // 发送通知
            if (server_) {
                server_->SendMessageToUser(current_user_id, notify_msg);
            }
            
            // 通过活跃的通知流推送
            std::lock_guard<std::mutex> lock(streams_mutex_);
            auto it = active_streams_.find(current_user_id);
            if (it != active_streams_.end()) {
                for (auto writer : it->second) {
                    writer->Write(notify_msg);
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("处理Kafka事件失败: {}", e.what());
    }
}

void NotificationServiceImpl::AddActiveStream(int64_t user_id, grpc::ServerWriter<Message>* writer) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    active_streams_[user_id].push_back(writer);
    LOG_INFO("用户 {} 添加了活跃通知流，当前共 {} 个", user_id, active_streams_[user_id].size());
}

void NotificationServiceImpl::RemoveActiveStream(int64_t user_id, grpc::ServerWriter<Message>* writer) {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = active_streams_.find(user_id);
    if (it != active_streams_.end()) {
        auto& streams = it->second;
        streams.erase(std::remove(streams.begin(), streams.end(), writer), streams.end());
        
        if (streams.empty()) {
            active_streams_.erase(it);
            LOG_INFO("用户 {} 的所有通知流已移除", user_id);
        } else {
            LOG_INFO("用户 {} 移除了一个通知流，当前还有 {} 个", user_id, streams.size());
        }
    }
}

// 修复消息类型转换方法
Message NotificationServiceImpl::ConvertToMessage(const Notification& notification) {
    Message message;
    message.set_message_id(notification.notification_id);
    message.set_from_user_id(notification.sender_id);
    message.set_to_user_id(0); // 或设置适当的接收者ID
    message.set_message_type(static_cast<im::MessageType>(im::NotificationMessageType::TEXT));
    message.set_content(notification.content);
    message.set_send_time(notification.created_time);
    message.set_is_read(false);
    message.set_extra_info(notification.extra_info);
    return message;
}

Notification NotificationServiceImpl::ConvertToNotification(const Message& message) {
    Notification notification;
    notification.notification_id = message.message_id();
    notification.sender_id = message.from_user_id();
    notification.type = NotificationType::MESSAGE; // 默认类型
    notification.content = message.content();
    notification.created_time = message.send_time();
    notification.extra_info = message.extra_info();
    return notification;
}

int64_t NotificationServiceImpl::StoreNotification(int64_t user_id, const Notification& notification) {
    try {
        // 存储到数据库
        std::string sql = "INSERT INTO notifications (user_id, type, content, sender_id, create_time, extra_info, is_read) "
                         "VALUES (?, ?, ?, ?, ?, ?, 0)";
        
        int64_t now = utils::DateTime::NowSeconds();
        std::string type_str = "general";
        
        // 根据通知类型设置类型字符串
        switch (notification.type) {
            case NotificationType::MESSAGE:
                type_str = "message";
                break;
            case NotificationType::FRIEND_REQUEST:
                type_str = "friend_request";
                break;
            case NotificationType::FRIEND_ACCEPTED:
                type_str = "friend_accepted";
                break;
            case NotificationType::GROUP_INVITATION:
                type_str = "group_invitation";
                break;
            case NotificationType::FILE_TRANSFER:
                type_str = "file_transfer";
                break;
            case NotificationType::SYSTEM:
                type_str = "system";
                break;
            default:
                type_str = "general";
                break;
        }
        
        int64_t notification_id = mysql_conn_->ExecuteInsert(sql, {
            std::to_string(user_id),
            type_str,
            notification.content,
            std::to_string(notification.sender_id),
            std::to_string(notification.created_time > 0 ? notification.created_time : now),
            notification.extra_info
        });
        
        // 存储到Redis缓存
        std::string notification_key = "user:" + std::to_string(user_id) + ":notifications";
        
        json redis_notification;
        redis_notification["id"] = std::to_string(notification_id);
        redis_notification["content"] = notification.content;
        redis_notification["type"] = type_str;
        redis_notification["sender_id"] = std::to_string(notification.sender_id);
        redis_notification["timestamp"] = std::to_string(notification.created_time > 0 ? notification.created_time : now);
        redis_notification["is_read"] = "0";
        
        if (!notification.extra_info.empty()) {
            try {
                json extra = json::parse(notification.extra_info);
                for (auto& [key, value] : extra.items()) {
                    redis_notification[key] = value;
                }
            } catch (const std::exception& e) {
                LOG_ERROR("解析通知额外信息失败: {}", e.what());
            }
        }
        
        redis_client_->ListPush(notification_key, redis_notification.dump());
        redis_client_->Expire(notification_key, 604800);  // 一周过期
        
        return notification_id;
    } catch (const std::exception& e) {
        LOG_ERROR("存储通知失败: {}", e.what());
        return -1;
    }
}

} // namespace im