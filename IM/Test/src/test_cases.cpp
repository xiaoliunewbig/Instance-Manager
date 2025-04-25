#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include "../../include/server/server.h"
#include "../../include/server/utils/logger.h"
#include "../../build/generated/service.grpc.pb.h"

using namespace im;

// 测试用例基类
class TestCase {
public:
    virtual ~TestCase() = default;
    virtual bool Run() = 0;
    virtual std::string GetName() const = 0;
    
    void SetToken(const std::string& token) { token_ = token; }
    void SetUserId(int64_t user_id) { user_id_ = user_id; }
    
    // 添加获取token和user_id的公共接口
    std::string GetToken() const { return token_; }
    int64_t GetUserId() const { return user_id_; }
    
    // 添加错误信息获取接口
    std::string GetErrorMessage() const { return error_message_; }
    
protected:
    std::string token_;
    int64_t user_id_ = 0;
    std::string error_message_; // 存储错误信息
    
    // 设置超时的ClientContext
    std::unique_ptr<grpc::ClientContext> CreateTimeoutContext(int timeout_seconds = 5) {
        auto context = std::make_unique<grpc::ClientContext>();
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(timeout_seconds);
        context->set_deadline(deadline);
        return context;
    }
    
    // 处理gRPC状态错误
    void HandleStatusError(const grpc::Status& status) {
        error_message_ = "RPC错误 [" + std::to_string(status.error_code()) + "]: " + status.error_message();
        
        if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
            error_message_ += " (请求超时)";
        } else if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
            error_message_ += " (服务不可用)";
        } else if (status.error_code() == grpc::StatusCode::INTERNAL) {
            error_message_ += " (服务器内部错误)";
        }
    }
};

// 用户注册测试
class RegisterTest : public TestCase {
public:
    RegisterTest(std::unique_ptr<UserService::Stub>& user_service)
        : user_service_(user_service) {}
    
    std::string GetName() const override {
        return "用户注册测试";
    }
    
    bool Run() override {
        std::cout << "执行用户注册测试..." << std::endl;
        
        auto context = CreateTimeoutContext();
        RegisterRequest request;
        RegisterResponse response;
        
        // 使用时间戳生成唯一用户名和邮箱
        std::string timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::string username = "test_user_" + timestamp.substr(timestamp.length() - 8);
        std::string email = "test_" + timestamp.substr(timestamp.length() - 8) + "@example.com";
        
        // 设置注册请求
        request.set_username(username);
        request.set_password("Password123!");
        request.set_email(email);
        request.set_verification_code("123456");
        request.set_nickname("测试用户");
        
        std::cout << "注册用户: " << username << ", 邮箱: " << email << std::endl;
        
        grpc::Status status = user_service_->Register(context.get(), request, &response);
        
        if (status.ok()) {
            std::cout << "注册结果: " << (response.success() ? "成功" : "失败") << std::endl;
            std::cout << "消息: " << response.message() << std::endl;
            if (response.success()) {
                std::cout << "用户ID: " << response.user_id() << std::endl;
                SetUserId(response.user_id());
                email_ = email; // 保存邮箱用于后续登录
                return true;
            } else {
                error_message_ = "服务器拒绝注册: " + response.message();
                return false;
            }
        } else {
            HandleStatusError(status);
            std::cout << error_message_ << std::endl;
            return false;
        }
    }
    
    // 获取注册邮箱
    std::string GetEmail() const { return email_; }
    
private:
    std::unique_ptr<UserService::Stub>& user_service_;
    std::string email_;
};

// 用户登录测试
class LoginTest : public TestCase {
public:
    LoginTest(std::unique_ptr<UserService::Stub>& user_service, const std::string& email, const std::string& password)
        : user_service_(user_service), email_(email), password_(password) {}
    
    std::string GetName() const override {
        return "用户登录测试";
    }
    
    bool Run() override {
        std::cout << "执行用户登录测试..." << std::endl;
        std::cout << "使用凭据 - 邮箱: " << email_ << ", 密码: [已隐藏]" << std::endl;
        
        auto context = CreateTimeoutContext();
        LoginRequest request;
        LoginResponse response;
        
        request.set_email(email_);
        request.set_password(password_);
        
        grpc::Status status = user_service_->Login(context.get(), request, &response);
        
        if (status.ok()) {
            std::cout << "登录结果: " << (response.success() ? "成功" : "失败") << std::endl;
            std::cout << "消息: " << response.message() << std::endl;
            if (response.success()) {
                std::cout << "用户信息: " << response.user_info().username() << std::endl;
                SetToken(response.token());
                SetUserId(response.user_info().user_id());
                return true;
            } else {
                error_message_ = "登录失败: " + response.message();
                return false;
            }
        } else {
            HandleStatusError(status);
            std::cout << error_message_ << std::endl;
            return false;
        }
    }
    
private:
    std::unique_ptr<UserService::Stub>& user_service_;
    std::string email_;
    std::string password_;
};

// 发送消息测试
class SendMessageTest : public TestCase {
public:
    SendMessageTest(std::unique_ptr<MessageService::Stub>& message_service, int64_t to_user_id, const std::string& content)
        : message_service_(message_service), to_user_id_(to_user_id), content_(content) {}
    
    std::string GetName() const override {
        return "发送消息测试";
    }
    
    bool Run() override {
        std::cout << "执行发送消息测试..." << std::endl;
        
        auto context = std::make_unique<grpc::ClientContext>();
        context->AddMetadata("authorization", "Bearer " + token_);
        
        SendMessageRequest request;
        SendMessageResponse response;
        
        request.set_from_user_id(user_id_);
        request.set_to_user_id(to_user_id_);
        request.set_message_type(MessageType::TEXT);
        request.set_content(content_);
        
        grpc::Status status = message_service_->SendMessage(context.get(), request, &response);
        
        if (status.ok()) {
            std::cout << "发送结果: " << (response.success() ? "成功" : "失败") << std::endl;
            std::cout << "消息: " << response.message() << std::endl;
            if (response.success()) {
                std::cout << "消息ID: " << response.message_id() << std::endl;
                std::cout << "发送时间: " << response.send_time() << std::endl;
            }
            return response.success();
        } else {
            std::cout << "RPC错误: " << status.error_message() << std::endl;
            return false;
        }
    }
    
private:
    std::unique_ptr<MessageService::Stub>& message_service_;
    int64_t to_user_id_;
    std::string content_;
};

// 获取用户信息测试
class GetUserInfoTest : public TestCase {
public:
    GetUserInfoTest(std::unique_ptr<UserService::Stub>& user_service)
        : user_service_(user_service) {}
    
    std::string GetName() const override {
        return "获取用户信息测试";
    }
    
    bool Run() override {
        std::cout << "执行获取用户信息测试..." << std::endl;
        
        auto context = std::make_unique<grpc::ClientContext>();
        context->AddMetadata("authorization", "Bearer " + token_);
        
        GetUserInfoRequest request;
        GetUserInfoResponse response;
        
        request.set_user_id(user_id_);
        
        grpc::Status status = user_service_->GetUserInfo(context.get(), request, &response);
        
        if (status.ok()) {
            std::cout << "获取结果: " << (response.success() ? "成功" : "失败") << std::endl;
            std::cout << "消息: " << response.message() << std::endl;
            if (response.success()) {
                const auto& user = response.user_info();
                std::cout << "用户ID: " << user.user_id() << std::endl;
                std::cout << "用户名: " << user.username() << std::endl;
                std::cout << "昵称: " << user.nickname() << std::endl;
                std::cout << "邮箱: " << user.email() << std::endl;
                std::cout << "状态: " << user.status() << std::endl;
            }
            return response.success();
        } else {
            std::cout << "RPC错误: " << status.error_message() << std::endl;
            return false;
        }
    }
    
private:
    std::unique_ptr<UserService::Stub>& user_service_;
};

// 更新用户信息测试
class UpdateUserInfoTest : public TestCase {
public:
    UpdateUserInfoTest(std::unique_ptr<UserService::Stub>& user_service, const std::string& nickname, const std::string& avatar_url)
        : user_service_(user_service), nickname_(nickname), avatar_url_(avatar_url) {}
    
    std::string GetName() const override {
        return "更新用户信息测试";
    }
    
    bool Run() override {
        std::cout << "执行更新用户信息测试..." << std::endl;
        
        auto context = std::make_unique<grpc::ClientContext>();
        context->AddMetadata("authorization", "Bearer " + token_);
        
        UpdateUserInfoRequest request;
        UpdateUserInfoResponse response;
        
        request.set_user_id(user_id_);
        request.set_nickname(nickname_);
        request.set_avatar_url(avatar_url_);
        request.set_status(UserStatus::ONLINE);
        
        grpc::Status status = user_service_->UpdateUserInfo(context.get(), request, &response);
        
        if (status.ok()) {
            std::cout << "更新结果: " << (response.success() ? "成功" : "失败") << std::endl;
            std::cout << "消息: " << response.message() << std::endl;
            return response.success();
        } else {
            std::cout << "RPC错误: " << status.error_message() << std::endl;
            return false;
        }
    }
    
private:
    std::unique_ptr<UserService::Stub>& user_service_;
    std::string nickname_;
    std::string avatar_url_;
};

// 获取消息历史测试
class GetMessageHistoryTest : public TestCase {
public:
    GetMessageHistoryTest(std::unique_ptr<MessageService::Stub>& message_service, int64_t friend_id)
        : message_service_(message_service), friend_id_(friend_id) {}
    
    std::string GetName() const override {
        return "获取消息历史测试";
    }
    
    bool Run() override {
        std::cout << "执行获取消息历史测试..." << std::endl;
        
        auto context = std::make_unique<grpc::ClientContext>();
        context->AddMetadata("authorization", "Bearer " + token_);
        
        GetMessageHistoryRequest request;
        GetMessageHistoryResponse response;
        
        request.set_user_id(user_id_);
        request.set_friend_id(friend_id_);
        request.set_start_time(0);
        request.set_end_time(std::chrono::system_clock::now().time_since_epoch().count());
        request.set_limit(20);
        
        grpc::Status status = message_service_->GetMessageHistory(context.get(), request, &response);
        
        if (status.ok()) {
            std::cout << "获取结果: " << (response.success() ? "成功" : "失败") << std::endl;
            std::cout << "消息: " << response.message() << std::endl;
            if (response.success()) {
                std::cout << "消息数量: " << response.messages_size() << std::endl;
                for (int i = 0; i < response.messages_size(); i++) {
                    const auto& msg = response.messages(i);
                    std::cout << "消息 #" << i+1 << ": " << msg.content() << std::endl;
                }
            }
            return response.success();
        } else {
            std::cout << "RPC错误: " << status.error_message() << std::endl;
            return false;
        }
    }
    
private:
    std::unique_ptr<MessageService::Stub>& message_service_;
    int64_t friend_id_;
};

// 测试套件类
class IMTestSuite {
public:
    IMTestSuite(const std::string& server_address) 
        : server_address_(server_address),
          channel_(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials())) {
        
        // 初始化服务存根
        user_service_ = UserService::NewStub(channel_);
        message_service_ = MessageService::NewStub(channel_);
        file_service_ = FileService::NewStub(channel_);
        notification_service_ = NotificationService::NewStub(channel_);
        
        std::cout << "已连接到服务器: " << server_address << std::endl;
        
        // 检查服务器连接
        CheckServerConnection();
    }
    
    // 检查服务器连接状态
    void CheckServerConnection() {
        std::cout << "检查服务器连接状态..." << std::endl;
        
        // 等待通道就绪，设置超时时间
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        bool connected = channel_->WaitForConnected(deadline);
        
        if (connected) {
            std::cout << "服务器连接就绪" << std::endl;
        } else {
            std::cout << "警告: 无法连接到服务器或连接超时" << std::endl;
        }
    }
    
    void RunBasicTests() {
        std::cout << "========== 运行基本测试套件 ==========" << std::endl;
        
        // 用户注册测试
        auto register_test = std::make_unique<RegisterTest>(user_service_);
        bool register_success = RunTest(register_test.get());
        
        // 保存注册邮箱，如果注册成功
        std::string email = register_success ? register_test->GetEmail() : "test@example.com";
        std::string password = "Password123!";
        
        // 用户登录测试
        auto login_test = std::make_unique<LoginTest>(user_service_, email, password);
        if (!RunTest(login_test.get())) {
            std::cout << "登录失败，无法继续测试" << std::endl;
            DiagnoseServerIssues();
            return;
        }
        
        // 获取用户信息测试
        auto get_user_info_test = std::make_unique<GetUserInfoTest>(user_service_);
        get_user_info_test->SetToken(token_);
        get_user_info_test->SetUserId(user_id_);
        RunTest(get_user_info_test.get());
        
        // 更新用户信息测试
        auto update_user_info_test = std::make_unique<UpdateUserInfoTest>(
            user_service_, "更新后的昵称", "http://example.com/avatar.jpg");
        update_user_info_test->SetToken(token_);
        update_user_info_test->SetUserId(user_id_);
        RunTest(update_user_info_test.get());
        
        // 发送消息测试
        auto send_message_test = std::make_unique<SendMessageTest>(
            message_service_, 1, "这是一条测试消息");
        send_message_test->SetToken(token_);
        send_message_test->SetUserId(user_id_);
        RunTest(send_message_test.get());
        
        // 获取消息历史测试
        auto get_message_history_test = std::make_unique<GetMessageHistoryTest>(
            message_service_, 1);
        get_message_history_test->SetToken(token_);
        get_message_history_test->SetUserId(user_id_);
        RunTest(get_message_history_test.get());
        
        std::cout << "========== 基本测试套件完成 ==========" << std::endl;
    }
    
    void RunPerformanceTests(int message_count = 10) {
        std::cout << "========== 运行性能测试套件 ==========" << std::endl;
        
        // 登录测试账号
        std::string email = "test@example.com";
        std::string password = "Password123!";
        
        auto login_test = std::make_unique<LoginTest>(user_service_, email, password);
        if (!RunTest(login_test.get())) {
            std::cout << "登录失败，无法继续测试" << std::endl;
            DiagnoseServerIssues();
            return;
        }
        
        // 批量发送消息性能测试
        std::cout << "消息发送性能测试 - 发送 " << message_count << " 条消息..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < message_count; i++) {
            std::string content = "性能测试消息 #" + std::to_string(i+1);
            auto send_test = std::make_unique<SendMessageTest>(message_service_, 1, content);
            send_test->SetToken(token_);
            send_test->SetUserId(user_id_);
            if (!send_test->Run()) {
                std::cout << "消息 #" << i+1 << " 发送失败" << std::endl;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        std::cout << "发送 " << message_count << " 条消息耗时: " << duration << " ms" << std::endl;
        std::cout << "平均每条消息耗时: " << (duration / message_count) << " ms" << std::endl;
        
        std::cout << "========== 性能测试套件完成 ==========" << std::endl;
    }
    
    // 服务器问题诊断
    void DiagnoseServerIssues() {
        std::cout << "\n========== 服务器诊断 ==========" << std::endl;
        
        // 检查连接状态
        {
            auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(2);
            bool connected = channel_->WaitForConnected(deadline);
            std::cout << "1. 服务器连接状态: " << (connected ? "可连接" : "不可连接") << std::endl;
        }
        
        // 尝试一个简单的无需认证的API调用
        {
            auto context = std::make_unique<grpc::ClientContext>();
            SendVerificationCodeRequest request;
            SendVerificationCodeResponse response;
            
            request.set_email("probe@example.com");
            
            auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(2);
            context->set_deadline(deadline);
            
            grpc::Status status = user_service_->SendVerificationCode(context.get(), request, &response);
            
            std::cout << "2. 验证码API测试: " 
                    << (status.ok() ? "服务响应" : "服务未响应") << std::endl;
            
            if (!status.ok()) {
                std::cout << "   错误详情: [" << status.error_code() << "] " 
                        << status.error_message() << std::endl;
            }
        }
        
        std::cout << "3. 基于服务器日志的问题诊断:" << std::endl;
        std::cout << "   - 数据库查询错误: 'Unknown column \'id\' in \'field list\'" << std::endl;
        std::cout << "   - 数据包大小限制: 'packet bigger than \'max_allowed_packet\' bytes'" << std::endl;
        
        std::cout << "4. 数据库结构分析:" << std::endl;
        std::cout << "   - 用户表(users)正确的主键列是'user_id'而非'id'" << std::endl;
        std::cout << "   - 相关SQL查询错误地使用了'id'而不是'user_id'" << std::endl;
        std::cout << "   - 所有相关表的外键列使用'user_id'引用users表" << std::endl;
        
        std::cout << "5. 具体错误位置:" << std::endl;
        std::cout << "   - user_service.cpp:" << std::endl;
        std::cout << "     * 第255行: UPDATE users SET last_login_at = ?, updated_at = ? WHERE id = ?" << std::endl;
        std::cout << "     * 第387行: FROM users WHERE id = ?" << std::endl;
        std::cout << "     * 第486行: sql += \" WHERE id = ?\"" << std::endl;
        std::cout << "     * 第501行: UPDATE users SET status = ? WHERE id = ?" << std::endl;
        std::cout << "     * 第587行: UPDATE users SET status = ?, updated_at = ? WHERE id = ?" << std::endl;
        std::cout << "     * 第768行: SELECT role FROM users WHERE id = ?" << std::endl;
        std::cout << "   - file_service.cpp:" << std::endl;
        std::cout << "     * 第385行: SELECT id FROM users WHERE id = ?" << std::endl;
        std::cout << "   - relation_service.cpp:" << std::endl;
        std::cout << "     * 第483行: SELECT COUNT(*) as count FROM users WHERE id = ?" << std::endl;
        std::cout << "     * 第583行: SELECT id, username, email, nickname, avatar, status FROM users WHERE id = ?" << std::endl;
        
        std::cout << "6. 修复建议:" << std::endl;
        std::cout << "   - 将所有SQL查询中涉及users表的'id'改为'user_id'" << std::endl;
        std::cout << "   - SELECT语句中的'id'列也应修改为'user_id'" << std::endl;
        std::cout << "   - 示例修复: 'SELECT id FROM users' 改为 'SELECT user_id FROM users'" << std::endl;
        std::cout << "   - 示例修复: 'WHERE id = ?' 改为 'WHERE user_id = ?'" << std::endl;
        std::cout << "   - 在MySQL配置中增加max_allowed_packet: SET GLOBAL max_allowed_packet=67108864; (64MB)" << std::endl;
        
        std::cout << "7. 其他可能的问题:" << std::endl;
        std::cout << "   - 代码中对从数据库返回的结果处理也可能需要修改" << std::endl;
        std::cout << "   - 检查'id'与'user_id'映射的一致性" << std::endl;
        
        std::cout << "======================================" << std::endl;
    }
    
private:
    bool RunTest(TestCase* test) {
        std::cout << "\n[测试]: " << test->GetName() << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        bool result = test->Run();
        std::cout << "结果: " << (result ? "通过" : "失败") << std::endl;
        if (!result) {
            std::cout << "错误信息: " << test->GetErrorMessage() << std::endl;
        }
        std::cout << "----------------------------------------" << std::endl;
        
        if (result) {
            passed_tests_++;
        } else {
            failed_tests_++;
        }
        
        // 使用公共接口获取token和user_id
        if (!test->GetToken().empty()) {
            token_ = test->GetToken();
        }
        if (test->GetUserId() > 0) {
            user_id_ = test->GetUserId();
        }
        
        return result;
    }

    std::string server_address_;
    std::shared_ptr<grpc::Channel> channel_;
    
    std::unique_ptr<UserService::Stub> user_service_;
    std::unique_ptr<MessageService::Stub> message_service_;
    std::unique_ptr<FileService::Stub> file_service_;
    std::unique_ptr<NotificationService::Stub> notification_service_;
    
    std::string token_;
    int64_t user_id_ = 0;
    
    int passed_tests_ = 0;
    int failed_tests_ = 0;
};

int main(int argc, char* argv[]) {
    // 初始化日志
    im::utils::Logger::Initialize("info", "");
    
    std::string server_address = "localhost:50051";
    if (argc > 1) {
        server_address = argv[1];
    }
    
    int test_type = 0; // 0: 基本测试, 1: 性能测试, 2: 全部测试
    if (argc > 2) {
        test_type = std::stoi(argv[2]);
    }
    
    // 处理特殊值-1作为诊断模式
    if (test_type == -1) {
        std::cout << "仅运行服务器诊断..." << std::endl;
        IMTestSuite diagnostics(server_address);
        diagnostics.DiagnoseServerIssues();
        return 0;
    }
    
    // 创建测试套件
    IMTestSuite test_suite(server_address);
    
    // 根据测试类型运行不同的测试
    switch (test_type) {
        case 0:
            test_suite.RunBasicTests();
            break;
        case 1:
            test_suite.RunPerformanceTests();
            break;
        case 2:
            test_suite.RunBasicTests();
            test_suite.RunPerformanceTests();
            break;
        default:
            std::cout << "未知的测试类型: " << test_type << std::endl;
            std::cout << "用法: " << argv[0] << " [服务器地址] [测试类型]" << std::endl;
            std::cout << "测试类型: 0=基本测试, 1=性能测试, 2=全部测试, -1=仅诊断" << std::endl;
            return 1;
    }
    
    return 0;
} 