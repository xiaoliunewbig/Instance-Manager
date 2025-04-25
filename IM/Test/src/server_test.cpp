#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include "../../include/server/server.h"
#include "../../include/server/utils/logger.h"
#include "../../build/generated/service.grpc.pb.h"

using namespace im;

// 测试客户端类
class IMTestClient {
public:
    IMTestClient(const std::string& server_address) 
        : server_address_(server_address),
          channel_(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials())) {
        
        // 初始化各服务存根
        user_service_ = UserService::NewStub(channel_);
        message_service_ = MessageService::NewStub(channel_);
        file_service_ = FileService::NewStub(channel_);
        notification_service_ = NotificationService::NewStub(channel_);
        
        std::cout << "已连接到服务器: " << server_address << std::endl;
        
        // 检查服务器连接状态
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

    // 测试用户注册
    bool TestRegister() {
        std::cout << "测试用户注册..." << std::endl;
        
        grpc::ClientContext context;
        // 设置5秒超时
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        context.set_deadline(deadline);
        
        RegisterRequest request;
        RegisterResponse response;
        
        // 使用时间戳创建唯一用户名和邮箱
        std::string timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::string username = "test_user_" + timestamp.substr(timestamp.length() - 8);
        std::string email = "test_" + timestamp.substr(timestamp.length() - 8) + "@example.com";
        
        // 设置注册请求
        request.set_username(username);
        request.set_password("Password123!");
        request.set_email(email);
        request.set_verification_code("123456"); // 模拟验证码
        request.set_nickname("测试用户");
        
        std::cout << "注册用户: " << username << ", 邮箱: " << email << std::endl;
        
        // 发送请求
        grpc::Status status = user_service_->Register(&context, request, &response);
        
        if (status.ok()) {
            std::cout << "注册结果: " << (response.success() ? "成功" : "失败") << std::endl;
            std::cout << "消息: " << response.message() << std::endl;
            if (response.success()) {
                std::cout << "用户ID: " << response.user_id() << std::endl;
                user_id_ = response.user_id();
                // 保存成功注册的凭据
                registered_email_ = email;
                registered_password_ = "Password123!";
            }
            return response.success();
        } else {
            std::cout << "RPC错误: [" << status.error_code() << "] " << status.error_message() << std::endl;
            if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
                std::cout << "请求超时，服务器响应时间过长" << std::endl;
            } else if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                std::cout << "服务器不可用，请检查服务器是否启动" << std::endl;
            }
            return false;
        }
    }
    
    // 测试用户登录
    bool TestLogin() {
        std::cout << "测试用户登录..." << std::endl;
        
        // 如果有注册成功的凭据，使用它们；否则使用默认凭据
        std::string email = !registered_email_.empty() ? registered_email_ : "test@example.com";
        std::string password = !registered_password_.empty() ? registered_password_ : "Password123!";
        
        std::cout << "使用凭据 - 邮箱: " << email << ", 密码: [已隐藏]" << std::endl;
        
        grpc::ClientContext context;
        // 设置5秒超时
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        context.set_deadline(deadline);
        
        LoginRequest request;
        LoginResponse response;
        
        // 设置登录请求
        request.set_email(email);
        request.set_password(password);
        
        // 发送请求
        grpc::Status status = user_service_->Login(&context, request, &response);
        
        if (status.ok()) {
            std::cout << "登录结果: " << (response.success() ? "成功" : "失败") << std::endl;
            std::cout << "消息: " << response.message() << std::endl;
            if (response.success()) {
                std::cout << "用户信息: " << response.user_info().username() << std::endl;
                token_ = response.token();
                user_id_ = response.user_info().user_id();
            }
            return response.success();
        } else {
            std::cout << "RPC错误: [" << status.error_code() << "] " << status.error_message() << std::endl;
            if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
                std::cout << "请求超时，服务器响应时间过长" << std::endl;
            } else if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
                std::cout << "服务器不可用，请检查服务器是否启动" << std::endl;
            }
            return false;
        }
    }
    
    // 测试发送消息
    bool TestSendMessage() {
        std::cout << "测试发送消息..." << std::endl;
        
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer " + token_);
        
        SendMessageRequest request;
        SendMessageResponse response;
        
        // 设置消息请求
        request.set_from_user_id(user_id_);
        request.set_to_user_id(1); // 发给管理员用户（假设ID为1）
        request.set_message_type(MessageType::TEXT);
        request.set_content("这是一条测试消息");
        
        // 发送请求
        grpc::Status status = message_service_->SendMessage(&context, request, &response);
        
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
    
    // 测试获取消息历史
    bool TestGetMessageHistory() {
        std::cout << "测试获取消息历史..." << std::endl;
        
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer " + token_);
        
        GetMessageHistoryRequest request;
        GetMessageHistoryResponse response;
        
        // 设置请求参数
        request.set_user_id(user_id_);
        request.set_friend_id(1); // 获取与用户1的聊天记录
        request.set_start_time(0); // 从最早时间开始
        request.set_end_time(std::chrono::system_clock::now().time_since_epoch().count()); // 到当前时间
        request.set_limit(10); // 最多10条记录
        
        // 发送请求
        grpc::Status status = message_service_->GetMessageHistory(&context, request, &response);
        
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
    
    // 测试获取用户信息
    bool TestGetUserInfo() {
        std::cout << "测试获取用户信息..." << std::endl;
        
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer " + token_);
        
        GetUserInfoRequest request;
        GetUserInfoResponse response;
        
        // 设置请求参数
        request.set_user_id(user_id_);
        
        // 发送请求
        grpc::Status status = user_service_->GetUserInfo(&context, request, &response);
        
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
    
    // 测试消息流
    void TestMessageStream() {
        std::cout << "测试消息流..." << std::endl;
        
        grpc::ClientContext context;
        context.AddMetadata("authorization", "Bearer " + token_);
        
        std::shared_ptr<grpc::ClientReaderWriter<Message, Message>> stream = 
            message_service_->MessageStream(&context);
        
        // 启动读取线程
        std::thread read_thread([this, &stream]() {
            Message received_message;
            while (stream->Read(&received_message)) {
                std::cout << "收到消息: " << received_message.content() << std::endl;
            }
        });
        
        // 发送一些消息
        for (int i = 0; i < 5; i++) {
            Message send_message;
            send_message.set_from_user_id(user_id_);
            send_message.set_to_user_id(1); // 发给管理员
            send_message.set_message_type(MessageType::TEXT);
            send_message.set_content("流消息测试 #" + std::to_string(i+1));
            send_message.set_send_time(std::chrono::system_clock::now().time_since_epoch().count());
            
            if (!stream->Write(send_message)) {
                std::cout << "消息流已关闭" << std::endl;
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // 关闭写入流
        stream->WritesDone();
        
        // 等待读取线程结束
        read_thread.join();
        
        grpc::Status status = stream->Finish();
        if (!status.ok()) {
            std::cout << "消息流RPC错误: " << status.error_message() << std::endl;
        }
    }
    
    // 运行所有测试
    void RunAllTests() {
        std::cout << "================ 开始测试IM系统 ================" << std::endl;
        
        // 测试注册
        bool register_result = TestRegister();
        if (!register_result) {
            std::cout << "注册失败，尝试登录..." << std::endl;
        }
        
        // 测试登录
        bool login_result = TestLogin();
        if (!login_result) {
            std::cout << "登录失败，无法继续测试" << std::endl;
            
            // 尝试诊断服务器问题
            DiagnoseServerIssues();
            return;
        }
        
        // 测试获取用户信息
        TestGetUserInfo();
        
        // 测试发送消息
        TestSendMessage();
        
        // 测试获取消息历史
        TestGetMessageHistory();
        
        // 测试消息流
        TestMessageStream();
        
        std::cout << "================ 测试完成 ================" << std::endl;
    }
    
    // 诊断服务器问题
    void DiagnoseServerIssues() {
        std::cout << "\n================ 服务器诊断 ================" << std::endl;
        
        // 检查连接状态
        {
            auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(2);
            bool connected = channel_->WaitForConnected(deadline);
            std::cout << "1. 服务器连接状态: " << (connected ? "可连接" : "不可连接") << std::endl;
        }
        
        // 尝试一个简单的无需认证的API调用来检查服务是否在线
        {
            grpc::ClientContext context;
            SendVerificationCodeRequest request;
            SendVerificationCodeResponse response;
            
            request.set_email("probe@example.com");
            
            auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(2);
            context.set_deadline(deadline);
            
            grpc::Status status = user_service_->SendVerificationCode(&context, request, &response);
            
            std::cout << "2. 验证码API测试: " 
                    << (status.ok() ? "服务响应" : "服务未响应") << std::endl;
            
            if (!status.ok()) {
                std::cout << "   错误详情: [" << status.error_code() << "] " 
                        << status.error_message() << std::endl;
            }
        }
        
        std::cout << "3. 可能的问题:" << std::endl;
        std::cout << "   - 数据库连接问题（检查服务器日志）" << std::endl;
        std::cout << "   - 数据表结构不匹配（检查'id'列是否存在）" << std::endl;
        std::cout << "   - MySQL 'max_allowed_packet'配置过小" << std::endl;
        std::cout << "   - 服务器内部错误（检查服务器日志）" << std::endl;
        
        std::cout << "4. 建议操作:" << std::endl;
        std::cout << "   - 检查服务器日志获取更详细错误信息" << std::endl;
        std::cout << "   - 验证数据库表结构" << std::endl;
        std::cout << "   - 增加MySQL max_allowed_packet大小" << std::endl;
        std::cout << "   - 确保用户表中使用'user_id'而非'id'列" << std::endl;
        std::cout << "===============================================" << std::endl;
    }

private:
    std::string server_address_;
    std::shared_ptr<grpc::Channel> channel_;
    
    std::unique_ptr<UserService::Stub> user_service_;
    std::unique_ptr<MessageService::Stub> message_service_;
    std::unique_ptr<FileService::Stub> file_service_;
    std::unique_ptr<NotificationService::Stub> notification_service_;
    
    std::string token_; // 认证令牌
    int64_t user_id_ = 0; // 用户ID
    
    // 存储成功注册的凭据
    std::string registered_email_;
    std::string registered_password_;
};

int main(int argc, char* argv[]) {
    // 初始化日志
    im::utils::Logger::Initialize("info", "");
    
    std::string server_address = "localhost:50051";
    if (argc > 1) {
        server_address = argv[1];
    }
    
    // 创建测试客户端并运行测试
    IMTestClient client(server_address);
    client.RunAllTests();
    
    return 0;
} 