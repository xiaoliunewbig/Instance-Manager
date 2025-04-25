#ifndef IM_MESSAGE_SERVICE_H
#define IM_MESSAGE_SERVICE_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include "../../build/generated/service.grpc.pb.h"
#include "include/server/db/mysql_connection.h"
#include "include/server/db/redis_client.h"
#include "include/server/kafka/kafka_producer.h"

// 前向声明
namespace im {
class IMServer;
}

namespace im {

/**
 * @brief 消息服务实现类，处理消息发送、接收等功能
 */
class MessageServiceImpl final : public MessageService::Service {
public:
    /**
     * @brief 构造函数
     * @param mysql_conn MySQL连接
     * @param redis_client Redis客户端
     * @param kafka_producer Kafka生产者
     * @param server 服务器实例指针
     */
    MessageServiceImpl(
        std::shared_ptr<im::db::MySQLConnection> mysql_conn,
        std::shared_ptr<im::db::RedisClient> redis_client,
        std::shared_ptr<im::kafka::KafkaProducer> kafka_producer,
        im::IMServer* server
    );

    /**
     * @brief 析构函数
     */
    ~MessageServiceImpl() override = default;

    /**
     * @brief 发送消息
     * @param context RPC上下文
     * @param request 发送消息请求
     * @param response 发送消息响应
     * @return RPC状态
     */
    grpc::Status SendMessage(
        grpc::ServerContext* context,
        const SendMessageRequest* request,
        SendMessageResponse* response
    ) override;

    /**
     * @brief 获取消息历史
     * @param context RPC上下文
     * @param request 获取消息历史请求
     * @param response 获取消息历史响应
     * @return RPC状态
     */
    grpc::Status GetMessageHistory(
        grpc::ServerContext* context,
        const GetMessageHistoryRequest* request,
        GetMessageHistoryResponse* response
    ) override;

    /**
     * @brief 获取离线消息
     * @param context RPC上下文
     * @param request 获取离线消息请求
     * @param response 获取离线消息响应
     * @return RPC状态
     */
    grpc::Status GetOfflineMessages(
        grpc::ServerContext* context,
        const GetOfflineMessagesRequest* request,
        GetOfflineMessagesResponse* response
    ) override;

    /**
     * @brief 标记消息为已读
     * @param context RPC上下文
     * @param request 标记消息为已读请求
     * @param response 标记消息为已读响应
     * @return RPC状态
     */
    grpc::Status MarkMessageRead(
        grpc::ServerContext* context,
        const MarkMessageReadRequest* request,
        MarkMessageReadResponse* response
    ) override;

    /**
     * @brief 消息流
     * @param context RPC上下文
     * @param stream 双向消息流
     * @return RPC状态
     */
    grpc::Status MessageStream(
        grpc::ServerContext* context,
        grpc::ServerReaderWriter<Message, Message>* stream
    ) override;

    /**
     * @brief 通知用户有新消息
     * @param user_id 用户ID
     * @param message 消息
     */
    void NotifyNewMessage(int64_t user_id, const Message& message);

    /**
     * @brief 添加活跃消息流
     * @param user_id 用户ID
     * @param stream 消息流
     */
    void AddActiveStream(int64_t user_id, grpc::ServerReaderWriter<Message, Message>* stream);

    /**
     * @brief 移除活跃消息流
     * @param user_id 用户ID
     * @param stream 消息流
     */
    void RemoveActiveStream(int64_t user_id, grpc::ServerReaderWriter<Message, Message>* stream);

    /**
     * @brief 存储消息到数据库
     * @param message 要存储的消息
     * @return 存储后的消息ID，失败返回0
     */
    int64_t StoreMessage(const Message& message);

    /**
     * @brief 存储离线消息
     * @param user_id 用户ID
     * @param message 消息内容
     */
    void StoreOfflineMessage(int64_t user_id, const Message& message);

    /**
     * @brief 获取认证令牌
     * @param context gRPC上下文
     * @return 认证令牌
     */
    std::string GetAuthToken(grpc::ServerContext* context);

    /**
     * @brief 验证令牌
     * @param token 认证令牌
     * @param user_id 输出参数，验证成功后存储用户ID
     * @return 验证是否成功
     */
    bool ValidateToken(const std::string& token, int64_t& user_id);

    /**
     * @brief 检查用户是否在线
     * @param user_id 用户ID
     * @return 在线返回true，离线返回false
     */
    bool IsUserOnline(int64_t user_id) const;

    /**
     * @brief 缓存消息到Redis
     * @param chat_type 聊天类型（personal或group）
     * @param chat_id 聊天ID
     * @param message 消息
     */
    void CacheMessage(const std::string& chat_type, int64_t chat_id, const Message& message);

    /**
     * @brief 发送消息到Kafka
     * @param message 消息
     * @param chat_type 聊天类型（personal或group）
     */
    void SendMessageToKafka(const Message& message, const std::string& chat_type);

    /**
     * @brief 调试日志
     */
    void LogDebug(const std::string& message) const;

    /**
     * @brief 错误日志
     */
    void LogError(const std::string& message) const;

private:
    std::shared_ptr<im::db::MySQLConnection> mysql_conn_;
    std::shared_ptr<im::db::RedisClient> redis_client_;
    std::shared_ptr<im::kafka::KafkaProducer> kafka_producer_;
    im::IMServer* server_;

    // 用户活跃消息流
    std::mutex streams_mutex_;
    std::unordered_map<int64_t, std::vector<grpc::ServerReaderWriter<Message, Message>*>> active_streams_;
};

} // namespace im

#endif // IM_MESSAGE_SERVICE_H
 