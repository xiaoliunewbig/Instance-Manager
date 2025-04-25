#ifndef IM_SERVER_PUSH_NOTIFICATION_SERVICE_H
#define IM_SERVER_PUSH_NOTIFICATION_SERVICE_H

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <mutex>

// 使用前向声明代替完整包含
#include <grpcpp/impl/codegen/status.h>
// 前向声明
namespace grpc {
class ServerContext;
template <typename R>
class ServerWriter;
}

// 前向声明 protobuf 类
namespace im {
class Message;
class SubscriptionRequest;
class NotificationService;
}

// 使用前向声明
namespace im {
namespace db {
class MySQLConnection;
class RedisClient;
}

namespace kafka {
class KafkaProducer;
}
}
// 继承基类所需的实际包含
#include "../../build/generated/service.grpc.pb.h"

namespace im {

// 推送通知类型
enum class PushNotificationType {
    MESSAGE,           // 新消息
    FRIEND_REQUEST,    // 好友请求
    FRIEND_ACCEPTED,   // 好友请求接受
    GROUP_INVITATION,  // 群组邀请
    FILE_TRANSFER,     // 文件传输
    SYSTEM             // 系统通知
};

// 推送通知设备类型
enum class DeviceType {
    ANDROID,
    IOS,
    WEB
};

// 推送通知服务
class PushNotificationService {
public:
    PushNotificationService(
        std::shared_ptr<im::db::MySQLConnection> mysql_conn,
        std::shared_ptr<im::db::RedisClient> redis_client
    );
    ~PushNotificationService();

    // 发送推送通知
    bool SendPushNotification(
        int64_t user_id,
        const std::string& title,
        const std::string& body,
        PushNotificationType type,
        const std::map<std::string, std::string>& data = {}
    );

    // 注册设备令牌
    bool RegisterDeviceToken(
        int64_t user_id,
        const std::string& device_token,
        DeviceType device_type
    );

    // 删除设备令牌
    bool UnregisterDeviceToken(
        int64_t user_id,
        const std::string& device_token
    );

private:
    // 获取用户的设备令牌
    std::vector<std::pair<std::string, DeviceType>> GetUserDeviceTokens(int64_t user_id);

    // 数据库和缓存连接
    std::shared_ptr<im::db::MySQLConnection> mysql_conn_;
    std::shared_ptr<im::db::RedisClient> redis_client_;
    
    // 线程安全
    mutable std::mutex mutex_;
};

/**
 * @brief 推送通知服务实现类，提供gRPC服务接口
 */
class PushNotificationServiceImpl final : public NotificationService::Service {
public:
    /**
     * @brief 构造函数
     * @param mysql_conn MySQL连接
     * @param redis_client Redis客户端
     * @param kafka_producer Kafka生产者
     */
    PushNotificationServiceImpl(
        std::shared_ptr<im::db::MySQLConnection> mysql_conn,
        std::shared_ptr<im::db::RedisClient> redis_client,
        std::shared_ptr<im::kafka::KafkaProducer> kafka_producer
    );

    /**
     * @brief 析构函数
     */
    ~PushNotificationServiceImpl() override = default;

    /**
     * @brief 订阅通知
     * @param context gRPC上下文
     * @param request 订阅请求
     * @param writer 服务器写入流
     * @return gRPC状态
     */
    grpc::Status SubscribeNotifications(
        grpc::ServerContext* context,
        const SubscriptionRequest* request,
        grpc::ServerWriter<Message>* writer
    ) override;

    /**
     * @brief 发送推送通知
     * @param user_id 用户ID
     * @param title 标题
     * @param body 内容
     * @param type 通知类型
     * @param data 额外数据
     * @return 是否成功
     */
    bool SendPushNotification(
        int64_t user_id,
        const std::string& title,
        const std::string& body,
        PushNotificationType type,
        const std::map<std::string, std::string>& data = {}
    );

private:
    std::shared_ptr<im::db::MySQLConnection> mysql_conn_;
    std::shared_ptr<im::db::RedisClient> redis_client_;
    std::shared_ptr<im::kafka::KafkaProducer> kafka_producer_;
    PushNotificationService push_service_;
};

// 工具函数
std::string GetAuthToken(grpc::ServerContext* context);
bool ValidateToken(const std::string& token, int64_t& user_id);

} // namespace im

#endif // IM_SERVER_PUSH_NOTIFICATION_SERVICE_H 