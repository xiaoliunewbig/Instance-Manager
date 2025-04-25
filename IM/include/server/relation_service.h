#ifndef IM_RELATION_SERVICE_H
#define IM_RELATION_SERVICE_H

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include "../../build/generated/service.grpc.pb.h"
#include "db/mysql_connection.h"
#include "db/redis_client.h"
#include "kafka/kafka_producer.h"

namespace im {

/**
 * @brief 关系服务实现类，处理好友关系管理等功能
 */
class RelationServiceImpl final : public RelationService::Service {
public:
    /**
     * @brief 构造函数
     * @param mysql_conn MySQL连接
     * @param redis_client Redis客户端
     * @param kafka_producer Kafka生产者
     */
    RelationServiceImpl(
        std::shared_ptr<db::MySQLConnection> mysql_conn,
        std::shared_ptr<db::RedisClient> redis_client,
        std::shared_ptr<kafka::KafkaProducer> kafka_producer
    );

    /**
     * @brief 析构函数
     */
    ~RelationServiceImpl() override = default;

    /**
     * @brief 添加好友请求
     * @param context RPC上下文
     * @param request 添加好友请求
     * @param response 添加好友响应
     * @return RPC状态
     */
    grpc::Status AddFriend(
        grpc::ServerContext* context,
        const AddFriendRequest* request,
        AddFriendResponse* response
    ) override;

    /**
     * @brief 处理好友请求
     * @param context RPC上下文
     * @param request 处理好友请求
     * @param response 处理好友请求响应
     * @return RPC状态
     */
    grpc::Status HandleFriendRequest(
        grpc::ServerContext* context,
        const HandleFriendRequestRequest* request,
        HandleFriendRequestResponse* response
    ) override;

    /**
     * @brief 获取好友列表
     * @param context RPC上下文
     * @param request 获取好友列表请求
     * @param response 获取好友列表响应
     * @return RPC状态
     */
    grpc::Status GetFriends(
        grpc::ServerContext* context,
        const GetFriendsRequest* request,
        GetFriendsResponse* response
    ) override;

    /**
     * @brief 获取待处理的好友请求
     * @param context RPC上下文
     * @param request 获取待处理的好友请求
     * @param response 获取待处理的好友请求响应
     * @return RPC状态
     */
    grpc::Status GetPendingFriendRequests(
        grpc::ServerContext* context,
        const GetPendingFriendRequestsRequest* request,
        GetPendingFriendRequestsResponse* response
    ) override;

    /**
     * @brief 删除好友
     * @param context RPC上下文
     * @param request 好友关系
     * @param response 通用响应
     * @return RPC状态
     */
    grpc::Status DeleteFriend(
        grpc::ServerContext* context,
        const FriendRelation* request,
        CommonResponse* response
    ) override;

private:
    /**
     * @brief 从请求元数据中获取授权令牌
     * @param context RPC上下文
     * @return 授权令牌
     */
    std::string GetAuthToken(const grpc::ServerContext* context) const;

    /**
     * @brief 验证JWT令牌
     * @param token JWT令牌
     * @param user_id 输出参数，成功时返回用户ID
     * @return 有效返回true，无效返回false
     */
    bool ValidateToken(const std::string& token, int64_t& user_id) const;

    /**
     * @brief 检查用户是否存在
     * @param user_id 用户ID
     * @return 存在返回true，不存在返回false
     */
    bool CheckUserExists(int64_t user_id) const;

    /**
     * @brief 检查是否已经是好友
     * @param user_id 用户ID
     * @param friend_id 好友ID
     * @return 是好友返回true，不是好友返回false
     */
    bool CheckIfAlreadyFriends(int64_t user_id, int64_t friend_id) const;
    
    /**
     * @brief 检查是否已经发送过好友请求
     * @param from_user_id 发送者ID
     * @param to_user_id 接收者ID
     * @return 存在未处理的请求返回true，否则返回false
     */
    bool CheckPendingRequest(int64_t from_user_id, int64_t to_user_id) const;

    /**
     * @brief 创建好友关系
     * @param user_id 用户ID
     * @param friend_id 好友ID
     * @return 成功返回true，失败返回false
     */
    bool CreateFriendRelation(int64_t user_id, int64_t friend_id);

    /**
     * @brief 发送好友关系变更事件到Kafka
     * @param event_type 事件类型
     * @param user_id 用户ID
     * @param friend_id 好友ID
     */
    void SendRelationEvent(const std::string& event_type, int64_t user_id, int64_t friend_id);

    /**
     * @brief 获取用户信息
     * @param user_id 用户ID
     * @param user_info 输出参数，返回用户信息
     * @return 成功返回true，失败返回false
     */
    bool GetUserInfoById(int64_t user_id, UserInfo* user_info);

    std::shared_ptr<db::MySQLConnection> mysql_conn_;
    std::shared_ptr<db::RedisClient> redis_client_;
    std::shared_ptr<kafka::KafkaProducer> kafka_producer_;
};

} // namespace im

#endif // IM_RELATION_SERVICE_H 