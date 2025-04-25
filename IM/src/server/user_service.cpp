#include "server/user_service.h"
#include "server/utils/logger.h"
#include "server/utils/security.h"
#include "server/utils/datetime.h"
#include "server/utils/config.h"
#include "server/utils/string_util.h"
#include <regex>
#include <random>
#include <chrono>
#include <nlohmann/json.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/algorithm/string.hpp>
#include <openssl/sha.h>
#include <openssl/hmac.h>

namespace im {

using json = nlohmann::json;

UserServiceImpl::UserServiceImpl(
    std::shared_ptr<db::MySQLConnection> mysql_conn,
    std::shared_ptr<db::RedisClient> redis_client,
    std::shared_ptr<kafka::KafkaProducer> kafka_producer
) : mysql_conn_(mysql_conn),
    redis_client_(redis_client),
    kafka_producer_(kafka_producer) {
    LOG_INFO("UserServiceImpl initialized");
}

grpc::Status UserServiceImpl::Register(
    grpc::ServerContext* context,
    const RegisterRequest* request,
    RegisterResponse* response
) {
    // 验证参数
    const std::string& username = request->username();
    const std::string& password = request->password();
    const std::string& email = request->email();
    const std::string& verification_code = request->verification_code();
    
    // 验证用户名
    if (username.empty() || username.length() < 3 || username.length() > 20) {
        response->set_success(false);
        response->set_message("用户名长度应为3-20个字符");
        return grpc::Status::OK;
    }
    
    // 验证密码强度
    if (!ValidatePassword(password)) {
        response->set_success(false);
        response->set_message("密码至少包含8个字符，且必须包含字母和数字");
        return grpc::Status::OK;
    }
    
    // 验证邮箱格式
    if (!ValidateEmail(email)) {
        response->set_success(false);
        response->set_message("邮箱格式不正确");
        return grpc::Status::OK;
    }
    
    // 检查用户名是否已存在
    try {
        std::string sql = "SELECT COUNT(*) as count FROM users WHERE username = ?";
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {username});
        
        if (!results.empty() && std::stoi(results[0]["count"]) > 0) {
            response->set_success(false);
            response->set_message("用户名已存在");
            return grpc::Status::OK;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("查询用户名失败: {}", e.what());
        response->set_success(false);
        response->set_message("服务器内部错误");
        return grpc::Status::OK;
    }
    
    // 检查邮箱是否已存在
    try {
        std::string sql = "SELECT COUNT(*) as count FROM users WHERE email = ?";
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {email});
        
        if (!results.empty() && std::stoi(results[0]["count"]) > 0) {
            response->set_success(false);
            response->set_message("邮箱已被注册");
            return grpc::Status::OK;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("查询邮箱失败: {}", e.what());
        response->set_success(false);
        response->set_message("服务器内部错误");
        return grpc::Status::OK;
    }
    
    // 验证验证码 (除非是内部注册，无需验证码)
    bool need_verification = utils::Config::GetInstance().GetBool("user.email_verification", true);
    if (need_verification && !ValidateVerificationCode(email, verification_code)) {
        response->set_success(false);
        response->set_message("验证码错误或已过期");
        return grpc::Status::OK;
    }
    
    // 生成盐值和密码哈希
    std::string salt = utils::Security::GenerateSalt();
    std::string hashed_password = utils::Security::HashPassword(password, salt);
    
    // 注册用户
    int64_t user_id = 0;
    try {
        // 开启事务
        mysql_conn_->BeginTransaction();
        
        // 插入用户记录
        std::string sql = "INSERT INTO users (username, email, password_hash, salt, role, status, created_at, updated_at) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        
        int64_t now = utils::DateTime::NowSeconds();
        int role = 0; // 普通用户
        int status = 1; // 正常状态
        
        user_id = mysql_conn_->ExecuteInsert(sql, {
            username,
            email,
            hashed_password,
            salt,
            std::to_string(role),
            std::to_string(status),
            std::to_string(now),
            std::to_string(now)
        });
        
        if (user_id == 0) {
            mysql_conn_->RollbackTransaction();
            response->set_success(false);
            response->set_message("创建用户失败");
            return grpc::Status::OK;
        }
        
        // 创建用户配置表
        sql = "INSERT INTO user_settings (user_id, notification_enabled, created_at, updated_at) "
             "VALUES (?, ?, ?, ?)";
        
        bool success = mysql_conn_->ExecuteUpdate(sql, {
            std::to_string(user_id),
            "1",  // 默认开启通知
            std::to_string(now),
            std::to_string(now)
        });
        
        if (!success) {
            mysql_conn_->RollbackTransaction();
            response->set_success(false);
            response->set_message("创建用户设置失败");
            return grpc::Status::OK;
        }
        
        // 提交事务
        mysql_conn_->CommitTransaction();
    } catch (const std::exception& e) {
        // 回滚事务
        mysql_conn_->RollbackTransaction();
        LOG_ERROR("注册用户失败: {}", e.what());
        response->set_success(false);
        response->set_message("服务器内部错误");
        return grpc::Status::OK;
    }
    
    // 发送注册事件到Kafka
    std::string client_ip = GetClientIP(context);
    SendRegistrationEvent(user_id, username, email, client_ip);
    
    // 生成登录令牌
    std::string token = GenerateToken(user_id, false);
    
    // 设置响应
    response->set_success(true);
    response->set_message("注册成功");
    response->set_user_id(user_id);
    
    LOG_INFO("用户注册成功: id={}, username={}, email={}", user_id, username, email);
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::Login(
    grpc::ServerContext* context,
    const LoginRequest* request,
    LoginResponse* response
) {
    // 获取参数
    std::string email = request->email();
    std::string password = request->password();
    
    // 查询用户信息
    int64_t user_id = 0;
    std::string username;
    std::string db_email;
    std::string password_hash;
    std::string salt;
    int role = 0;
    int status = 0;
    
    try {
        std::string sql = "SELECT user_id, username, email, password_hash, salt, role, status "
                        "FROM users WHERE email = ?";
        
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {email});
        
        if (results.empty()) {
            response->set_success(false);
            response->set_message("邮箱或密码错误");
            return grpc::Status::OK;
        }
        
        auto& user = results[0];
        user_id = std::stoll(user["user_id"]);
        username = user["username"];
        db_email = user["email"];
        password_hash = user["password_hash"];
        salt = user["salt"];
        role = std::stoi(user["role"]);
        status = std::stoi(user["status"]);
    } catch (const std::exception& e) {
        LOG_ERROR("查询用户信息失败: {}", e.what());
        response->set_success(false);
        response->set_message("服务器内部错误");
        return grpc::Status::OK;
    }
    
    // 检查用户状态
    if (status != 1) {
        response->set_success(false);
        response->set_message("账号已被禁用");
        return grpc::Status::OK;
    }
    
    // 验证密码
    if (!utils::Security::VerifyPassword(password, password_hash, salt)) {
        response->set_success(false);
        response->set_message("邮箱或密码错误");
        return grpc::Status::OK;
    }
    
    // 生成登录令牌
    bool is_admin = (role == 1);
    std::string token = GenerateToken(user_id, is_admin);
    
    // 记录用户在线状态到Redis
    std::string online_key = "user:" + std::to_string(user_id) + ":online";
    redis_client_->SetValue(online_key, "1", 3600);  // 1小时过期
    
    // 更新最后登录时间
    try {
        std::string sql = "UPDATE users SET last_login_at = ?, updated_at = ? WHERE user_id = ?";
        int64_t now = utils::DateTime::NowSeconds();
        mysql_conn_->ExecuteUpdate(sql, {
            std::to_string(now),
            std::to_string(now),
            std::to_string(user_id)
        });
    } catch (const std::exception& e) {
        LOG_ERROR("更新用户登录时间失败: {}", e.what());
    }
    
    // 记录登录日志
    std::string client_ip = GetClientIP(context);
    try {
        std::string sql = "INSERT INTO login_logs (user_id, ip_address, login_time, status) "
                        "VALUES (?, ?, ?, ?)";
        int64_t now = utils::DateTime::NowSeconds();
        mysql_conn_->ExecuteInsert(sql, {
            std::to_string(user_id),
            client_ip,
            std::to_string(now),
            "1"  // 登录成功
        });
    } catch (const std::exception& e) {
        LOG_ERROR("记录登录日志失败: {}", e.what());
    }
    
    // 发送登录事件到Kafka
    nlohmann::json event;
    event["event_type"] = "user_login";
    event["user_id"] = user_id;
    event["username"] = username;
    event["ip_address"] = client_ip;
    event["timestamp"] = utils::DateTime::NowSeconds();
    
    kafka_producer_->SendMessage("im_events", event.dump(), std::to_string(user_id));
    
    // 设置响应
    response->set_success(true);
    response->set_message("登录成功");
    response->set_token(token);
    
    // 填充用户信息
    UserInfo* user_info = response->mutable_user_info();
    user_info->set_user_id(user_id);
    user_info->set_username(username);
    user_info->set_email(db_email);
    user_info->set_status(status == 1 ? ONLINE : OFFLINE);
    
    LOG_INFO("用户登录成功: id={}, username={}", user_id, username);
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::SendVerificationCode(
    grpc::ServerContext* context,
    const SendVerificationCodeRequest* request,
    SendVerificationCodeResponse* response
) {
    // 获取参数
    std::string email = request->email();
    
    // 验证邮箱格式
    if (!ValidateEmail(email)) {
        response->set_success(false);
        response->set_message("邮箱格式不正确");
        return grpc::Status::OK;
    }
    
    // 检查发送频率，防止滥用
    std::string rate_limit_key = "email_verify_rate:" + email;
    if (redis_client_->KeyExists(rate_limit_key)) {
        response->set_success(false);
        response->set_message("验证码已发送，请稍后再试");
        return grpc::Status::OK;
    }
    
    // 生成6位数字验证码
    std::string code = GenerateVerificationCode();
    
    // 存储验证码到Redis，有效期10分钟
    std::string verify_code_key = "email_verify_code:" + email;
    redis_client_->SetValue(verify_code_key, code, 600);
    
    // 设置频率限制，60秒内不能重复发送
    redis_client_->SetValue(rate_limit_key, "1", 60);
    
    // 发送验证码邮件（模拟发送，实际项目中应集成邮件服务）
    bool sent = SendVerificationEmail(email, code);
    
    // 设置响应
    response->set_success(sent);
    response->set_message(sent ? "验证码已发送" : "发送验证码失败");
    
    // 在开发环境中返回验证码（方便测试）
    if (utils::Config::GetInstance().GetString("env", "prod") == "dev") {
        LOG_DEBUG("开发环境验证码: {} -> {}", email, code);
    }
    
    return grpc::Status::OK;
}

// 发送验证码邮件（模拟实现）
bool UserServiceImpl::SendVerificationEmail(const std::string& email, const std::string& code) {
    // 实际项目中，这里应该调用邮件服务发送验证码
    // 例如使用SMTP服务或第三方邮件API
    LOG_INFO("模拟发送验证码邮件: email={}, code={}", email, code);
    
    // 这里仅作为示例，总是返回成功
    return true;
}

grpc::Status UserServiceImpl::GetUserInfo(
    grpc::ServerContext* context,
    const GetUserInfoRequest* request,
    GetUserInfoResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t auth_user_id;
    if (!ValidateToken(token, auth_user_id)) {
        response->set_success(false);
        response->set_message("未授权");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid token");
    }
    
    // 获取目标用户ID
    int64_t user_id = request->user_id();
    
    try {
        // 查询用户信息
        std::string sql = "SELECT user_id, username, email, nickname, avatar, role, status, created_at, updated_at, "
                         "last_login_at, gender, bio, notification_enabled "
                         "FROM users WHERE user_id = ?";
        
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {std::to_string(user_id)});
        
        if (results.empty()) {
            response->set_success(false);
            response->set_message("用户不存在");
            return grpc::Status::OK;
        }
        
        auto& user = results[0];
        UserInfo* user_info = response->mutable_user_info();
        
        user_info->set_user_id(std::stoll(user["user_id"]));
        user_info->set_username(user["username"]);
        
        // 如果是查询其他用户，出于隐私考虑，不返回邮箱
        if (user_id == auth_user_id) {
            user_info->set_email(user["email"]);
        }
        
        if (user.find("nickname") != user.end() && !user["nickname"].empty()) {
            user_info->set_nickname(user["nickname"]);
        }
        
        if (user.find("avatar") != user.end() && !user["avatar"].empty()) {
            user_info->set_avatar_url(user["avatar"]);
        }
        
        // 设置用户状态
        int status_int = std::stoi(user["status"]);
        if (status_int == 0) {
            user_info->set_status(UserStatus::OFFLINE);
        } else if (status_int == 1) {
            user_info->set_status(UserStatus::ONLINE);
        } else if (status_int == 2) {
            user_info->set_status(UserStatus::AWAY);
        } else if (status_int == 3) {
            user_info->set_status(UserStatus::BUSY);
        } else {
            user_info->set_status(UserStatus::OFFLINE); // 默认
        }
        
        // 其他字段 (gender, bio, role, created_at, last_login_at 等) 在当前 proto 定义中不存在
        // 如果需要这些信息，应该扩展 proto 定义
        
        response->set_success(true);
        response->set_message("成功获取用户信息");
    } catch (const std::exception& e) {
        LOG_ERROR("获取用户信息失败: {}", e.what());
        response->set_success(false);
        response->set_message("服务器内部错误");
    }
    
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::UpdateUserInfo(
    grpc::ServerContext* context,
    const UpdateUserInfoRequest* request,
    UpdateUserInfoResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t auth_user_id;
    if (!ValidateToken(token, auth_user_id)) {
        response->set_success(false);
        response->set_message("未授权");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid token");
    }
    
    // 确保用户只能修改自己的信息
    int64_t user_id = request->user_id();
    if (user_id != auth_user_id) {
        response->set_success(false);
        response->set_message("无权修改其他用户的信息");
        return grpc::Status::OK;
    }
    
    try {
        // 构建SQL语句和参数
        std::string sql = "UPDATE users SET updated_at = ?";
        std::vector<std::string> params;
        
        int64_t now = utils::DateTime::NowSeconds();
        params.push_back(std::to_string(now));
        
        // 添加要更新的字段
        if (!request->nickname().empty()) {
            sql += ", nickname = ?";
            params.push_back(request->nickname());
        }
        
        if (!request->avatar_url().empty()) {
            sql += ", avatar = ?";
            params.push_back(request->avatar_url());
        }
        
        // 添加WHERE条件
        sql += " WHERE user_id = ?";
        params.push_back(std::to_string(user_id));
        
        // 执行更新
        bool success = mysql_conn_->ExecuteUpdate(sql, params);
        if (!success) {
            response->set_success(false);
            response->set_message("更新失败");
            return grpc::Status::OK;
        }
        
        // 更新状态（单独处理，因为可能需要通知其他用户）
        if (request->status() != UserStatus::OFFLINE) {
            try {
                // 更新用户状态
                std::string status_sql = "UPDATE users SET status = ? WHERE user_id = ?";
                std::vector<std::string> status_params = {
                    std::to_string(static_cast<int>(request->status())),
                    std::to_string(user_id)
                };
                
                mysql_conn_->ExecuteUpdate(status_sql, status_params);
                
                // 设置在线状态到Redis（用于即时通讯）
                std::string online_key = "user:" + std::to_string(user_id) + ":online";
                redis_client_->SetValue(online_key, std::to_string(static_cast<int>(request->status())), 86400);
                
                // TODO: 通知好友状态变更
            } catch (const std::exception& e) {
                LOG_ERROR("更新用户状态失败: {}", e.what());
            }
        }
        
        response->set_success(true);
        response->set_message("用户信息已更新");
    } catch (const std::exception& e) {
        LOG_ERROR("更新用户信息失败: {}", e.what());
        response->set_success(false);
        response->set_message("服务器内部错误");
    }
    
    return grpc::Status::OK;
}

grpc::Status UserServiceImpl::GetPendingApprovals(
    grpc::ServerContext* context,
    grpc::ServerReader<UserInfo>* reader,
    CommonResponse* response
) {
    // 验证授权和管理员权限
    std::string token = GetAuthToken(context);
    int64_t user_id;
    if (!ValidateToken(token, user_id) || !IsAdmin(user_id)) {
        response->set_success(false);
        response->set_message("无权限执行此操作");
        return grpc::Status::OK;
    }
    
    try {
        // 查询待审批用户
        std::string sql = "SELECT user_id, username, email, nickname, avatar, gender, bio, role, status, created_at "
                         "FROM users WHERE status = 0 ORDER BY created_at DESC";
        
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql);
        
        // 处理上传的用户列表
        UserInfo request;
        while (reader->Read(&request)) {
            // 处理每个请求，可以在这里添加代码查询指定用户的状态等
        }
        
        response->set_success(true);
        response->set_message("成功获取待审批用户列表");
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        LOG_ERROR("获取待审批用户列表失败: {}", e.what());
        response->set_success(false);
        response->set_message("服务器内部错误");
        return grpc::Status::OK;
    }
}

grpc::Status UserServiceImpl::ApproveUser(
    grpc::ServerContext* context,
    const UserInfo* request,
    CommonResponse* response
) {
    // 验证授权和管理员权限
    std::string token = GetAuthToken(context);
    int64_t admin_id;
    if (!ValidateToken(token, admin_id) || !IsAdmin(admin_id)) {
        response->set_success(false);
        response->set_message("无权限执行此操作");
        return grpc::Status::OK;
    }
    
    int64_t user_id = request->user_id();
    int new_status = request->status();
    
    try {
        // 更新用户状态
        std::string sql = "UPDATE users SET status = ?, updated_at = ? WHERE user_id = ?";
        int64_t now = utils::DateTime::NowSeconds();
        
        bool result = mysql_conn_->ExecuteUpdate(sql, {
            std::to_string(new_status),
            std::to_string(now),
            std::to_string(user_id)
        });
        
        if (!result) {
            response->set_success(false);
            response->set_message("更新用户状态失败");
            return grpc::Status::OK;
        }
        
        // 记录审批日志
        sql = "INSERT INTO approval_logs (user_id, admin_id, old_status, new_status, approval_time) "
             "VALUES (?, ?, ?, ?, ?)";
        
        mysql_conn_->ExecuteInsert(sql, {
            std::to_string(user_id),
            std::to_string(admin_id),
            "0",  // 假设原状态为待审批
            std::to_string(new_status),
            std::to_string(now)
        });
        
        response->set_success(true);
        response->set_message("用户状态已更新");
        
        LOG_INFO("管理员 {} 将用户 {} 的状态更新为 {}", admin_id, user_id, new_status);
    } catch (const std::exception& e) {
        LOG_ERROR("审批用户失败: {}", e.what());
        response->set_success(false);
        response->set_message("服务器内部错误");
    }
    
    return grpc::Status::OK;
}

bool UserServiceImpl::ValidateEmail(const std::string& email) const {
    std::regex email_regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    return std::regex_match(email, email_regex);
}

bool UserServiceImpl::ValidatePassword(const std::string& password) const {
    // 密码至少8个字符，包含字母和数字
    bool has_letter = false;
    bool has_digit = false;
    
    if (password.length() < 8) {
        return false;
    }
    
    for (char c : password) {
        if (std::isalpha(c)) {
            has_letter = true;
        } else if (std::isdigit(c)) {
            has_digit = true;
        }
        
        if (has_letter && has_digit) {
            return true;
        }
    }
    
    return false;
}

bool UserServiceImpl::ValidateVerificationCode(const std::string& email, const std::string& code) {
    if (code.empty()) {
        return false;
    }
    
    std::string code_key = "verification_code:" + email;
    std::string stored_code = redis_client_->GetValue(code_key);
    
    if (stored_code.empty()) {
        return false;  // 验证码不存在或已过期
    }
    
    bool result = (stored_code == code);
    
    // 验证成功后删除验证码，防止重复使用
    if (result) {
        redis_client_->DeleteKey(code_key);
    }
    
    return result;
}

std::string UserServiceImpl::GenerateVerificationCode() const {
    return utils::Security::GenerateVerificationCode();
}

std::string UserServiceImpl::GenerateToken(int64_t user_id, bool is_admin) const {
    // 创建JWT载荷
    std::map<std::string, std::string> payload;
    payload["user_id"] = std::to_string(user_id);
    payload["is_admin"] = is_admin ? "1" : "0";
    
    // 从配置获取JWT密钥和过期时间
    std::string secret = utils::Config::GetInstance().GetString("security.jwt_secret", "your_jwt_secret");
    int expire_seconds = utils::Config::GetInstance().GetInt("security.jwt_expire_seconds", 86400);
    
    // 生成JWT令牌
    return utils::Security::GenerateJWT(payload, secret, expire_seconds);
}

bool UserServiceImpl::ValidateToken(const std::string& token, int64_t& user_id) const {
    if (token.empty()) {
        return false;
    }
    
    std::map<std::string, std::string> payload;
    std::string secret = utils::Config::GetInstance().GetString("security.jwt_secret", "your_jwt_secret");
    
    if (!utils::Security::VerifyJWT(token, secret, payload)) {
        return false;
    }
    
    auto it = payload.find("user_id");
    if (it == payload.end()) {
        return false;
    }
    
    try {
        user_id = std::stoll(it->second);
        
        // 可以在这里添加额外的验证，例如检查用户是否存在、是否被禁用等
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Token中的user_id无效: {}", e.what());
        return false;
    }
}

void UserServiceImpl::SendRegistrationEvent(
    int64_t user_id,
    const std::string& username,
    const std::string& email,
    const std::string& client_ip
) {
    nlohmann::json event;
    event["event_type"] = "user_register";
    event["user_id"] = user_id;
    event["username"] = username;
    event["email"] = email;
    event["ip_address"] = client_ip;
    event["timestamp"] = utils::DateTime::NowSeconds();
    
    kafka_producer_->SendMessage("im_events", event.dump(), std::to_string(user_id));
}

std::string UserServiceImpl::GetClientIP(const grpc::ServerContext* context) const {
    auto peer_identity = context->peer();
    // 解析peer字符串，格式类似于"ipv4:127.0.0.1:12345"
    std::vector<std::string> parts = utils::StringUtil::Split(peer_identity, ':');
    
    if (parts.size() >= 2) {
        return parts[1];
    }
    
    return "unknown";
}

std::string UserServiceImpl::GetAuthToken(const grpc::ServerContext* context) const {
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

bool UserServiceImpl::IsAdmin(int64_t user_id) const {
    try {
        std::string sql = "SELECT role FROM users WHERE user_id = ?";
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {std::to_string(user_id)});
        
        if (!results.empty()) {
            int role = std::stoi(results[0]["role"]);
            return role == 1;  // 假设1表示管理员角色
        }
    } catch (const std::exception& e) {
        LOG_ERROR("验证管理员权限失败: {}", e.what());
    }
    
    return false;
}

} // namespace im 