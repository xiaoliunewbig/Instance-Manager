#ifndef IM_USER_SERVICE_H
#define IM_USER_SERVICE_H

#include <memory>
#include <string>
#include <vector>
#include <grpcpp/grpcpp.h>
#include "../../build/generated/service.grpc.pb.h"
#include "db/mysql_connection.h"
#include "db/redis_client.h"
#include "kafka/kafka_producer.h"

namespace im {

/**
 * @brief 用户服务实现类，处理用户注册、登录等功能
 */
class UserServiceImpl final : public UserService::Service {
public:
    /**
     * @brief 构造函数
     * @param mysql_conn MySQL连接
     * @param redis_client Redis客户端
     * @param kafka_producer Kafka生产者
     */
    UserServiceImpl(
        std::shared_ptr<im::db::MySQLConnection> mysql_conn,
        std::shared_ptr<im::db::RedisClient> redis_client,
        std::shared_ptr<im::kafka::KafkaProducer> kafka_producer
    );

    /**
     * @brief 析构函数
     */
    ~UserServiceImpl() override = default;

    /**
     * @brief 用户注册
     * @param context RPC上下文
     * @param request 注册请求
     * @param response 注册响应
     * @return RPC状态
     */
    grpc::Status Register(
        grpc::ServerContext* context,
        const RegisterRequest* request,
        RegisterResponse* response
    ) override;

    /**
     * @brief 用户登录
     * @param context RPC上下文
     * @param request 登录请求
     * @param response 登录响应
     * @return RPC状态
     */
    grpc::Status Login(
        grpc::ServerContext* context,
        const LoginRequest* request,
        LoginResponse* response
    ) override;

    /**
     * @brief 发送验证码
     * @param context RPC上下文
     * @param request 发送验证码请求
     * @param response 发送验证码响应
     * @return RPC状态
     */
    grpc::Status SendVerificationCode(
        grpc::ServerContext* context,
        const SendVerificationCodeRequest* request,
        SendVerificationCodeResponse* response
    ) override;

    /**
     * @brief 获取用户信息
     * @param context RPC上下文
     * @param request 获取用户信息请求
     * @param response 获取用户信息响应
     * @return RPC状态
     */
    grpc::Status GetUserInfo(
        grpc::ServerContext* context,
        const GetUserInfoRequest* request,
        GetUserInfoResponse* response
    ) override;

    /**
     * @brief 更新用户信息
     * @param context RPC上下文
     * @param request 更新用户信息请求
     * @param response 更新用户信息响应
     * @return RPC状态
     */
    grpc::Status UpdateUserInfo(
        grpc::ServerContext* context,
        const UpdateUserInfoRequest* request,
        UpdateUserInfoResponse* response
    ) override;

    /**
     * @brief 获取待审批用户列表
     */
    grpc::Status GetPendingApprovals(
        grpc::ServerContext* context,
        grpc::ServerReader<UserInfo>* reader,
        CommonResponse* response
    ) override;

    /**
     * @brief 审批用户注册
     * @param context RPC上下文
     * @param request 用户信息
     * @param response 通用响应
     * @return RPC状态
     */
    grpc::Status ApproveUser(
        grpc::ServerContext* context,
        const UserInfo* request,
        CommonResponse* response
    ) override;

private:
    /**
     * @brief 验证邮箱格式
     * @param email 邮箱
     * @return 有效返回true，无效返回false
     */
    bool ValidateEmail(const std::string& email) const;

    /**
     * @brief 验证密码强度
     * @param password 密码
     * @return 有效返回true，无效返回false
     */
    bool ValidatePassword(const std::string& password) const;

    /**
     * @brief 验证验证码
     * @param email 邮箱
     * @param code 验证码
     * @return 有效返回true，无效返回false
     */
    bool ValidateVerificationCode(const std::string& email, const std::string& code);

    /**
     * @brief 生成验证码
     * @return 6位数字验证码
     */
    std::string GenerateVerificationCode() const;

    /**
     * @brief 发送验证码邮件（模拟实现）
     * @param email 目标邮箱
     * @param code 验证码
     * @return 发送成功返回true，失败返回false
     */
    bool SendVerificationEmail(const std::string& email, const std::string& code);

    /**
     * @brief 生成JWT令牌
     * @param user_id 用户ID
     * @param is_admin 是否是管理员
     * @return JWT令牌
     */
    std::string GenerateToken(int64_t user_id, bool is_admin) const;

    /**
     * @brief 验证JWT令牌
     * @param token JWT令牌
     * @param user_id 输出参数，成功时返回用户ID
     * @return 有效返回true，无效返回false
     */
    bool ValidateToken(const std::string& token, int64_t& user_id) const;

    /**
     * @brief 发送注册事件到Kafka
     * @param user_id 用户ID
     * @param username 用户名
     * @param email 邮箱
     * @param client_ip 客户端IP
     */
    void SendRegistrationEvent(
        int64_t user_id,
        const std::string& username,
        const std::string& email,
        const std::string& client_ip
    );

    /**
     * @brief 从请求元数据中获取客户端IP
     * @param context RPC上下文
     * @return 客户端IP
     */
    std::string GetClientIP(const grpc::ServerContext* context) const;

    /**
     * @brief 从请求元数据中获取授权令牌
     * @param context RPC上下文
     * @return 授权令牌
     */
    std::string GetAuthToken(const grpc::ServerContext* context) const;

    /**
     * @brief 检查是否是管理员
     * @param user_id 用户ID
     * @return 是管理员返回true，否则返回false
     */
    bool IsAdmin(int64_t user_id) const;

    std::shared_ptr<db::MySQLConnection> mysql_conn_;
    std::shared_ptr<db::RedisClient> redis_client_;
    std::shared_ptr<im::kafka::KafkaProducer> kafka_producer_;
};

} // namespace im

#endif // IM_USER_SERVICE_H