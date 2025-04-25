#ifndef IM_NOTIFICATION_SERVICE_H
#define IM_NOTIFICATION_SERVICE_H

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "../../build/generated/service.grpc.pb.h"
#include "db/mysql_connection.h"
#include "db/redis_client.h"
#include "kafka/kafka_producer.h"
#include "kafka/kafka_consumer.h"

// 前向声明
class IMServer;

namespace im {

// 前向声明
class MessageServiceImpl;
class UserServiceImpl;

// 确保枚举中有NOTIFICATION成员
enum class NotificationMessageType {
    TEXT = 0,
    IMAGE = 1,
    VIDEO = 2,
    AUDIO = 3,
    
    FILE_TYPE = 4,
    LOCATION = 5,
    CONTACT = 6,
    SYSTEM = 7,
    NOTIFICATION = 8
};

// 通知类型枚举
enum class NotificationType {
    MESSAGE = 0,
    FRIEND_REQUEST = 1,
    FRIEND_ACCEPTED = 2,
    GROUP_INVITATION = 3,
    FILE_TRANSFER = 4,
    SYSTEM = 5
};

// 通知结构体
struct Notification {
    int64_t notification_id;
    int64_t sender_id;
    NotificationType type;
    std::string content;
    int64_t created_time;
    std::string extra_info;
};

/**
 * @brief 通知服务实现类，处理系统通知和推送
 */
class NotificationServiceImpl final : public NotificationService::Service {
public:
    /**
     * @brief 构造函数
     * @param mysql_conn MySQL连接
     * @param redis_client Redis客户端
     * @param kafka_producer Kafka生产者
     * @param message_service 消息服务实例
     * @param user_service 用户服务实例
     * @param server 服务器实例，可选
     */
    NotificationServiceImpl(
        std::shared_ptr<im::db::MySQLConnection> mysql_conn,
        std::shared_ptr<im::db::RedisClient> redis_client,
        std::shared_ptr<im::kafka::KafkaProducer> kafka_producer,
        im::MessageServiceImpl* message_service, // 确保类型正确
        im::UserServiceImpl* user_service,
        im::IMServer* server
    );

    /**
     * @brief 析构函数
     */
    ~NotificationServiceImpl();

    /**
     * @brief 订阅通知流
     * @param context RPC上下文
     * @param request 订阅请求
     * @param writer 服务器写入流
     * @return RPC状态
     */
    grpc::Status SubscribeNotifications(
        grpc::ServerContext* context,
        const SubscriptionRequest* request,
        grpc::ServerWriter<Message>* writer
    ) override;

    /**
     * @brief 发送通知
     * @param user_id 接收通知的用户ID
     * @param notification 通知内容
     */
    void SendNotification(int64_t user_id, const Notification& notification);

    /**
     * @brief 广播通知
     * @param message 通知消息
     */
    void BroadcastNotification(const Message& message);

    /**
     * @brief 将通知存储到数据库
     * @param user_id 用户ID
     * @param notification 通知
     * @return 成功返回通知ID，失败返回-1
     */
    int64_t StoreNotification(int64_t user_id, const Notification& notification);

    /**
     * @brief 检查用户是否在线
     * @param user_id 用户ID
     * @return 用户在线返回true，否则返回false
     */
    bool IsUserOnline(int64_t user_id) const;

    /**
     * @brief 将消息转换为通知
     * @param message 消息对象
     * @return 通知对象
     */
    Notification ConvertToNotification(const Message& message);

    /**
     * @brief 将通知转换为消息
     * @param notification 通知对象
     * @return 消息对象
     */
    Message ConvertToMessage(const Notification& notification);

    /**
     * @brief 设置消息服务
     * @param service 消息服务实例
     */
    void SetMessageService(MessageServiceImpl* service) { message_service_ = service; }

    /**
     * @brief 设置用户服务
     * @param service 用户服务实例
     */
    void SetUserService(UserServiceImpl* service) { user_service_ = service; }

    /**
     * @brief 验证令牌
     * @param context gRPC上下文
     * @param user_id 输出参数，验证成功后存储用户ID
     * @return 验证是否成功
     */
    bool ValidateToken(grpc::ServerContext* context, int64_t& user_id);

    /**
     * @brief 获取认证令牌
     * @param context gRPC上下文
     * @return 认证令牌
     */
    std::string GetAuthToken(grpc::ServerContext* context);

private:
    // 回调类，用于处理gRPC异步通知
    class NotificationDoneCallback;

    /**
     * @brief Kafka消费线程函数
     */
    void KafkaConsumerThread();

    /**
     * @brief 处理Kafka事件
     * @param event_json 事件JSON
     */
    void HandleKafkaEvent(const std::string& event_json);

    /**
     * @brief 添加活跃通知流
     * @param user_id 用户ID
     * @param writer 消息流
     */
    void AddActiveStream(int64_t user_id, grpc::ServerWriter<Message>* writer);

    /**
     * @brief 移除活跃通知流
     * @param user_id 用户ID
     * @param writer 消息流
     */
    void RemoveActiveStream(int64_t user_id, grpc::ServerWriter<Message>* writer);

    std::shared_ptr<im::db::MySQLConnection> mysql_conn_;
    std::shared_ptr<im::db::RedisClient> redis_client_;
    std::shared_ptr<im::kafka::KafkaProducer> kafka_producer_;
    std::unique_ptr<im::kafka::KafkaConsumer> kafka_consumer_;

    // 用户活跃通知流
    std::mutex streams_mutex_;
    std::unordered_map<int64_t, std::vector<grpc::ServerWriter<Message>*>> active_streams_;

    // 其他服务引用
    im::MessageServiceImpl* message_service_;
    im::UserServiceImpl* user_service_;
    im::IMServer* server_;  // 服务器实例指针

    // Kafka消费线程
    std::thread kafka_consumer_thread_;
    std::atomic<bool> running_;
};

} // namespace im

#endif // IM_NOTIFICATION_SERVICE_H