#include "server/admin_service.h"
#include "server/utils/logger.h"
#include "server/utils/security.h"
#include "server/utils/config.h"

namespace im {

AdminServiceImpl::AdminServiceImpl(
    std::shared_ptr<db::MySQLConnection> mysql_conn,
    std::shared_ptr<db::RedisClient> redis_client,
    std::shared_ptr<kafka::KafkaProducer> kafka_producer
) : mysql_conn_(mysql_conn),
    redis_client_(redis_client),
    kafka_producer_(kafka_producer) {
    LOG_INFO("AdminService初始化");
}

std::string AdminServiceImpl::GetSystemStatus() const {
    // 简单实现，返回一些基本状态信息
    // 实际应用中应该收集更多系统信息
    return "系统运行正常";
}

bool AdminServiceImpl::RestartService(const std::string& service_name) {
    LOG_INFO("尝试重启服务: {}", service_name);
    // 实际应用中应该实现服务重启逻辑
    return true;
}

// gRPC方法实现
grpc::Status AdminServiceImpl::GetSystemStatus(
    grpc::ServerContext* context,
    const CommonRequest* request,
    CommonResponse* response
) {
    // 验证请求是否来自管理员
    std::string token = GetAuthToken(context);
    int64_t user_id;
    if (!ValidateAdminToken(token, user_id)) {
        response->set_success(false);
        response->set_message("仅管理员可访问该接口");
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Unauthorized");
    }

    if (request->user_id() != user_id) {
        response->set_success(false);
        response->set_message("用户ID不匹配");
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "User ID mismatch");
    }

    // 获取系统状态
    std::string status = GetSystemStatus();
    response->set_success(true);
    response->set_message(status);
    return grpc::Status::OK;
}

grpc::Status AdminServiceImpl::RestartService(
    grpc::ServerContext* context,
    const RestartServiceRequest* request,
    CommonResponse* response
) {
    // 验证请求是否来自管理员
    std::string token = GetAuthToken(context);
    int64_t user_id;
    if (!ValidateAdminToken(token, user_id)) {
        response->set_success(false);
        response->set_message("仅管理员可访问该接口");
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Unauthorized");
    }

    if (request->admin_id() != user_id) {
        response->set_success(false);
        response->set_message("管理员ID不匹配");
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Admin ID mismatch");
    }

    // 重启服务
    bool success = RestartService(request->service_name());
    response->set_success(success);
    response->set_message(success ? "服务重启成功" : "服务重启失败");
    return grpc::Status::OK;
}

// 辅助方法
std::string AdminServiceImpl::GetAuthToken(const grpc::ServerContext* context) const {
    auto metadata = context->client_metadata();
    auto it = metadata.find("authorization");
    if (it != metadata.end()) {
        std::string auth_header(it->second.data(), it->second.size());
        if (auth_header.substr(0, 7) == "Bearer ") {
            return auth_header.substr(7);
        }
    }
    return "";
}

bool AdminServiceImpl::ValidateAdminToken(const std::string& token, int64_t& user_id) const {
    std::map<std::string, std::string> payload;
    // 从配置中获取JWT密钥，而不是硬编码
    std::string jwt_secret = utils::Config::GetInstance().GetString("security.jwt_secret", "your_jwt_secret");
    
    if (!utils::Security::VerifyJWT(token, jwt_secret, payload)) {
        return false;
    }
    
    auto it_user_id = payload.find("user_id");
    auto it_role = payload.find("role");
    
    if (it_user_id == payload.end() || it_role == payload.end()) {
        return false;
    }
    
    try {
        user_id = std::stoll(it_user_id->second);
        int role = std::stoi(it_role->second);
        
        // 检查是否为管理员角色
        return role == 1; // 假设角色1为管理员
    } catch (const std::exception& e) {
        LOG_ERROR("Invalid token payload: {}", e.what());
        return false;
    }
}

} // namespace im 