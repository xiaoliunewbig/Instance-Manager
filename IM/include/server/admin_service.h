#ifndef IM_ADMIN_SERVICE_H
#define IM_ADMIN_SERVICE_H

#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "generated/service.grpc.pb.h"
#include "db/mysql_connection.h"
#include "db/redis_client.h"
#include "kafka/kafka_producer.h"

namespace im {

/**
 * @brief 管理员服务实现
 */
class AdminServiceImpl : public im::AdminService::Service {
public:
    /**
     * @brief 构造函数
     * @param mysql_conn MySQL连接
     * @param redis_client Redis客户端
     * @param kafka_producer Kafka生产者
     */
    AdminServiceImpl(
        std::shared_ptr<db::MySQLConnection> mysql_conn,
        std::shared_ptr<db::RedisClient> redis_client,
        std::shared_ptr<kafka::KafkaProducer> kafka_producer
    );

    /**
     * @brief 获取系统状态
     * @return 系统状态信息
     */
    std::string GetSystemStatus() const;

    /**
     * @brief 重启服务
     * @param service_name 服务名称
     * @return 成功返回true，失败返回false
     */
    bool RestartService(const std::string& service_name);

    // gRPC方法重写
    /**
     * @brief 处理获取系统状态请求
     */
    grpc::Status GetSystemStatus(
        grpc::ServerContext* context,
        const CommonRequest* request,
        CommonResponse* response
    ) override;

    /**
     * @brief 处理重启服务请求
     */
    grpc::Status RestartService(
        grpc::ServerContext* context,
        const RestartServiceRequest* request,
        CommonResponse* response
    ) override;

private:
    std::shared_ptr<db::MySQLConnection> mysql_conn_;
    std::shared_ptr<db::RedisClient> redis_client_;
    std::shared_ptr<kafka::KafkaProducer> kafka_producer_;

    /**
     * @brief 从请求上下文中获取认证令牌
     */
    std::string GetAuthToken(const grpc::ServerContext* context) const;

    /**
     * @brief 验证管理员令牌
     * @param token JWT令牌
     * @param user_id 输出用户ID
     * @return 如果是有效的管理员令牌则返回true
     */
    bool ValidateAdminToken(const std::string& token, int64_t& user_id) const;
};

} // namespace im

#endif // IM_ADMIN_SERVICE_H 