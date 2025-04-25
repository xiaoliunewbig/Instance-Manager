#include "include/server/push_notification_service.h"  // 只包含必要的头文件

// 在实现文件中包含完整定义
#include "include/server/db/mysql_connection.h"
#include "include/server/db/redis_client.h"
#include "include/server/kafka/kafka_producer.h"
#include "include/server/utils/logger.h"
#include "include/server/utils/config.h"
#include "include/server/utils/string_util.h"
#include "include/server/utils/datetime.h"
#include "include/server/utils/jwt_verifier.h"
#include "include/server/utils/security.h"

// 放到其他包含的后面
#include <nlohmann/json.hpp>

namespace im {

using json = nlohmann::json;

// 添加 PushNotificationServiceImpl 类的实现
PushNotificationServiceImpl::PushNotificationServiceImpl(
    std::shared_ptr<im::db::MySQLConnection> mysql_conn,
    std::shared_ptr<im::db::RedisClient> redis_client,
    std::shared_ptr<im::kafka::KafkaProducer> kafka_producer
) : mysql_conn_(mysql_conn),
    redis_client_(redis_client),
    kafka_producer_(kafka_producer),
    push_service_(mysql_conn, redis_client) {
    
    LOG_INFO("PushNotificationServiceImpl 初始化");
}

grpc::Status PushNotificationServiceImpl::SubscribeNotifications(
    grpc::ServerContext* context,
    const SubscriptionRequest* request,
    grpc::ServerWriter<Message>* writer
) {
    LOG_INFO("用户 {} 订阅通知", request->user_id());
    
    // 验证用户
    std::string token = GetAuthToken(context);
    int64_t user_id;
    if (!ValidateToken(token, user_id)) {
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "无效的认证令牌");
    }
    
    // 确保请求的用户ID与认证的用户ID匹配
    if (user_id != request->user_id()) {
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "用户ID不匹配");
    }
    
    // 如果提供了设备令牌，则注册
    if (!request->device_token().empty()) {
        push_service_.RegisterDeviceToken(
            user_id,
            request->device_token(),
            DeviceType::WEB // 默认为Web设备
        );
    }
    
    // 记录用户订阅
    // 这里应该实现实际的订阅逻辑，例如添加到活跃订阅列表等
    // 由于是简化实现，我们这里只返回成功状态
    
    // 在实际实现中，这里应该使用长连接保持通知流
    // 可以实现以下逻辑：
    // 1. 将writer添加到用户的订阅列表中
    // 2. 定期检查用户是否有新通知
    // 3. 有新通知时通过writer发送
    // 4. 当客户端断开连接时清理订阅
    
    // 发送一条测试通知
    Message notification;
    notification.set_message_id(0);
    notification.set_from_user_id(0);  // 系统消息
    notification.set_to_user_id(user_id);
    notification.set_message_type(MessageType::NOTIFICATION);
    notification.set_content("通知订阅已激活");
    notification.set_send_time(utils::DateTime::NowMilliseconds());
    notification.set_is_read(false);
    
    if (!writer->Write(notification)) {
        LOG_ERROR("发送测试通知失败");
    }
    
    // 在真实环境中，这里应该保持连接直到客户端断开
    // 目前简单返回成功
    return grpc::Status::OK;
}

bool PushNotificationServiceImpl::SendPushNotification(
    int64_t user_id,
    const std::string& title,
    const std::string& body,
    PushNotificationType type,
    const std::map<std::string, std::string>& data
) {
    // 使用基础服务发送推送
    bool result = push_service_.SendPushNotification(user_id, title, body, type, data);
    
    // 同时发送到Kafka以便其他服务可以消费
    try {
        nlohmann::json notification_data;
        notification_data["user_id"] = user_id;
        notification_data["title"] = title;
        notification_data["body"] = body;
        notification_data["type"] = static_cast<int>(type);
        
        // 添加额外数据
        if (!data.empty()) {
            nlohmann::json extra_data;
            for (const auto& [key, value] : data) {
                extra_data[key] = value;
            }
            notification_data["data"] = extra_data;
        }
        
        // 发送到Kafka
        std::string topic = "im_notifications";
        std::string key = std::to_string(user_id);
        
        kafka_producer_->SendMessage(topic, notification_data.dump(), key);
    } catch (const std::exception& e) {
        LOG_ERROR("发送通知到Kafka失败: {}", e.what());
    }
    
    return result;
}

// 从gRPC上下文中获取认证令牌
std::string GetAuthToken(grpc::ServerContext* context) {
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

// 验证令牌并提取用户ID
bool ValidateToken(const std::string& token, int64_t& user_id) {
    try {
        if (token.empty()) {
            return false;
        }
        
        // 使用JWT验证工具验证令牌
        std::map<std::string, std::string> payload;
        im::utils::Security::VerifyJWT(token, "", payload); 
        
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

PushNotificationService::PushNotificationService(
    std::shared_ptr<im::db::MySQLConnection> mysql_conn,
    std::shared_ptr<im::db::RedisClient> redis_client
) : mysql_conn_(mysql_conn),
    redis_client_(redis_client) {
    
    LOG_INFO("推送通知服务初始化");
}

PushNotificationService::~PushNotificationService() {
    LOG_INFO("推送通知服务销毁");
}

bool PushNotificationService::SendPushNotification(
    int64_t user_id,
    const std::string& title,
    const std::string& body,
    PushNotificationType type,
    const std::map<std::string, std::string>& data
) {
    try {
        // 获取设备令牌
        auto device_tokens = GetUserDeviceTokens(user_id);
        if (device_tokens.empty()) {
            LOG_INFO("用户 {} 没有注册设备令牌，跳过推送", user_id);
            return true;
        }

        // 记录到数据库中
        std::string sql = "INSERT INTO notification_logs (user_id, title, body, type, created_at) "
                         "VALUES (?, ?, ?, ?, NOW())";
        mysql_conn_->ExecuteUpdate(sql, {
            std::to_string(user_id),
            title,
            body,
            std::to_string(static_cast<int>(type))
        });

        // 构建通知数据
        json notification_data = {
            {"user_id", user_id},
            {"title", title},
            {"body", body},
            {"type", static_cast<int>(type)},
            {"timestamp", utils::DateTime::NowSeconds()},
            {"data", data}
        };

        // 记录到日志
        LOG_INFO("发送推送通知到用户 {}: {}", user_id, notification_data.dump());

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("发送推送通知失败: {}", e.what());
        return false;
    }
}

bool PushNotificationService::RegisterDeviceToken(
    int64_t user_id,
    const std::string& device_token,
    DeviceType device_type
) {
    try {
        // 检查是否已经存在相同的令牌
        std::string check_sql = "SELECT COUNT(*) as count FROM device_tokens "
                               "WHERE user_id = ? AND device_token = ?";
        auto results = mysql_conn_->ExecuteQuery(check_sql, {
            std::to_string(user_id),
            device_token
        });
        
        if (!results.empty() && std::stoi(results[0]["count"]) > 0) {
            // 更新记录
            std::string update_sql = "UPDATE device_tokens SET device_type = ?, updated_at = NOW() "
                                    "WHERE user_id = ? AND device_token = ?";
            mysql_conn_->ExecuteUpdate(update_sql, {
                std::to_string(static_cast<int>(device_type)),
                std::to_string(user_id),
                device_token
            });
        } else {
            // 插入新记录
            std::string insert_sql = "INSERT INTO device_tokens (user_id, device_token, device_type, created_at) "
                                    "VALUES (?, ?, ?, NOW())";
            mysql_conn_->ExecuteUpdate(insert_sql, {
                std::to_string(user_id),
                device_token,
                std::to_string(static_cast<int>(device_type))
            });
        }
        
        // 更新缓存
        std::string cache_key = "device_tokens:" + std::to_string(user_id);
        redis_client_->SetHashField(cache_key, device_token, std::to_string(static_cast<int>(device_type)));
        redis_client_->SetExpire(cache_key, 3600); // 1小时过期
        
        LOG_INFO("用户 {} 注册设备令牌: {}, 类型: {}", user_id, device_token, static_cast<int>(device_type));
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("注册设备令牌失败: {}", e.what());
        return false;
    }
}

bool PushNotificationService::UnregisterDeviceToken(
    int64_t user_id,
    const std::string& device_token
) {
    try {
        // 从数据库删除令牌
        std::string sql = "DELETE FROM device_tokens WHERE user_id = ? AND device_token = ?";
        mysql_conn_->ExecuteUpdate(sql, {
            std::to_string(user_id),
            device_token
        });
        
        // 从缓存中删除令牌
        std::string cache_key = "device_tokens:" + std::to_string(user_id);
        redis_client_->DeleteHashField(cache_key, device_token);
        
        LOG_INFO("用户 {} 注销设备令牌: {}", user_id, device_token);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("注销设备令牌失败: {}", e.what());
        return false;
    }
}

std::vector<std::pair<std::string, DeviceType>> PushNotificationService::GetUserDeviceTokens(int64_t user_id) {
    std::vector<std::pair<std::string, DeviceType>> tokens;
    
    try {
        // 尝试从缓存中获取
        std::string cache_key = "device_tokens:" + std::to_string(user_id);
        auto cached_tokens = redis_client_->GetHashAll(cache_key);
        
        if (!cached_tokens.empty()) {
            for (const auto& pair : cached_tokens) {
                int type_int = std::stoi(pair.second);
                tokens.push_back({pair.first, static_cast<DeviceType>(type_int)});
            }
            return tokens;
        }
        
        // 从数据库获取
        std::string sql = "SELECT device_token, device_type FROM device_tokens WHERE user_id = ?";
        auto results = mysql_conn_->ExecuteQuery(sql, {std::to_string(user_id)});
        
        // 构建缓存和结果
        for (const auto& row : results) {
            std::string token = row.at("device_token");
            int type_int = std::stoi(row.at("device_type"));
            DeviceType type = static_cast<DeviceType>(type_int);
            
            tokens.push_back({token, type});
            
            // 更新缓存
            redis_client_->SetHashField(cache_key, token, std::to_string(type_int));
        }
        
        // 设置缓存过期时间
        if (!tokens.empty()) {
            redis_client_->SetExpire(cache_key, 3600); // 1小时过期
        }
        
        return tokens;
    } catch (const std::exception& e) {
        LOG_ERROR("获取用户设备令牌失败: {}", e.what());
        return {};
    }
}

} // namespace im 