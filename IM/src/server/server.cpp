#include "include/server/pch.h"
#include "pch.h"
#include "include/server/server.h"
#include "include/server/message_service.h"
#include "include/server/user_service.h"
#include "include/server/file_service.h"
#include "include/server/notification_service.h"
#include <algorithm>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include "include/server/websocket_handler.h"

using tcp = boost::asio::ip::tcp;

// namespace db {
//   class MySQLConnection;
//   class RedisClient;
// }

// namespace kafka {
//   class KafkaProducer;
// }

namespace im {

    IMServer::IMServer() 
        : running_(false),
          server_address_("0.0.0.0"),
          server_port_(50051),
          use_ssl_(false) {
        LOG_INFO("创建 IM 服务器实例");
    }
    
    IMServer::~IMServer() {
        Stop();
    }
    
    void IMServer::EnsureConfigDefaults() {
        auto& config = utils::Config::GetInstance();
        
        // 服务器配置
        config.GetOrCreate("server.address", "0.0.0.0");
        config.GetOrCreate("server.grpc_port", 50051);
        config.GetOrCreate("server.websocket_port", 8800);
        config.GetOrCreate("server.ssl.enabled", false);
        
        // 数据库配置
        config.GetOrCreate("database.mysql.max_allowed_packet", 1073741824);
        config.GetOrCreate("database.mysql.connect_timeout", 60);
        
        // 保存配置，以便在下次启动时已经有合适的默认值
        config.Save();
    }
    
    bool IMServer::Initialize(const std::string& config_path) {
        LOG_INFO("使用配置初始化 IM 服务器: {}", config_path);
        
        // 加载配置
        if (!LoadConfig(config_path)) {
            LOG_CRITICAL("配置加载失败");
            return false;
        }
        
        // 确保必要的配置项存在
        EnsureConfigDefaults();
        
        // 初始化数据库连接
        if (!InitializeDatabase()) {
            LOG_CRITICAL("无法初始化数据库连接");
            return false;
        }
        
        // 初始化Redis客户端
        if (!InitializeRedis()) {
            LOG_CRITICAL("无法初始化Redis客户端");
            return false;
        }
        
        // 初始化Kafka
        if (!InitializeKafka()) {
            LOG_CRITICAL("无法初始化Kafka");
            return false;
        }
        
        // 初始化各种服务
        if (!InitializeGrpcServices()) {
            LOG_CRITICAL("无法初始化gRPC服务");
            return false;
        }
        
        // 初始化WebSocket服务
        if (im::utils::Config::GetInstance().GetBool("websocket.enabled", true)) {
            if (!InitializeWebSocket()) {
                LOG_CRITICAL("无法初始化WebSocket服务");
                return false;
            }
        }
        
        LOG_INFO("IM服务器初始化成功");
        return true;
    }
    
    bool IMServer::Run() {
        std::lock_guard<std::mutex> lock(websocket_mutex_);
        if (running_) {
            return false;
        }
        running_ = true;
        LOG_INFO("启动gRPC服务器在端口 {}...", server_port_);
    
        // 创建gRPC服务器构建器
        grpc::ServerBuilder builder;
    
        // 绑定地址和端口
        builder.AddListeningPort(server_address_ + ":" + std::to_string(server_port_), grpc::InsecureServerCredentials());
    
        // 注册服务
        builder.RegisterService(admin_service_.get());
        builder.RegisterService(file_service_.get());
        builder.RegisterService(push_notification_service_.get());
        builder.RegisterService(relation_service_.get());
        builder.RegisterService(user_service_.get());
        builder.RegisterService(message_service_.get());
    
        // 构建并启动gRPC服务器
        grpc_server_ = builder.BuildAndStart();
        if (!grpc_server_) {
            LOG_CRITICAL("无法启动gRPC服务器");
            return false;
        }
    
        LOG_INFO("gRPC服务器已启动");
    
        // 启动gRPC服务器等待请求
        grpc_server_->Wait();
    
        return true;
    }
    
    void IMServer::Stop() {
        std::lock_guard<std::mutex> lock(websocket_mutex_);
        if (!running_) {
            return;
        }
        if (grpc_server_) {
            grpc_server_->Shutdown();
        }
        websocket_connections_.clear();
        message_service_.reset();
        user_service_.reset();
        file_service_.reset();
        push_notification_service_.reset();
        notification_service_.reset();
        running_ = false;
    }
    
    void IMServer::SendMessageToUser(int64_t user_id, const im::Message& message) {
        // std::lock_guard<std::mutex> lock(websocket_mutex_);
        // auto it = websocket_connections_.find(user_id);
        // if (it != websocket_connections_.end()) {
        //     // 发送消息逻辑
        //     for (auto& connection : it->second) {
        //         connection.message_callback(message);
        //     }
        // }
        // if (message_service_) {
        //     // 其他逻辑
        //     message_service_->SendMessage(user_id, message);
        // }
        grpc::ServerContext context;
        im::SendMessageRequest request;
        im::SendMessageResponse response;
        request.set_to_user_id(user_id);
        request.CopyFrom(message);
        if (redis_client_ && redis_client_->IsConnected()) {
            message_service_->SendMessage(&context, &request, &response);
        } else {
            LOG_WARN("Redis连接尚未建立，跳过此操作");
        }
    }
    
    im::PushNotificationServiceImpl* IMServer::GetPushNotificationService() const {
        return push_notification_service_.get();
    }
    
    im::MessageServiceImpl* IMServer::GetMessageService() const {
        return message_service_.get();
    }
    
    im::UserServiceImpl* IMServer::GetUserService() const {
        return user_service_.get();
    }
    
    im::FileServiceImpl* IMServer::GetFileService() const {
        return file_service_.get();
    }

    im::AdminServiceImpl* IMServer::GetAdminService() const {
        return admin_service_.get();
    }

    im::RelationServiceImpl* IMServer::GetRelationService() const {
        return relation_service_.get();
    }
    
    im::NotificationServiceImpl* IMServer::GetNotificationService() const {
        return notification_service_.get();
    }
    
    std::shared_ptr<im::db::MySQLConnection> IMServer::GetMySQLConnection() const {
        return mysql_conn_;
    }
    
    std::shared_ptr<im::db::RedisClient> IMServer::GetRedisClient() const {
        return redis_client_;
    }
    
    std::shared_ptr<im::kafka::KafkaProducer> IMServer::GetKafkaProducer() const {
        return kafka_producer_;
    }
    
    grpc::Server* IMServer::GetGrpcServer() const {
        return grpc_server_.get();
    }
    
    void IMServer::StopWebSocketServer() {
        LOG_INFO("停止WebSocket服务器");
        // 这里添加关闭WebSocket服务器的具体实现
        // 目前只是一个占位功能，实际应该根据WebSocket实现方式添加相应代码
    }
    
    bool IMServer::RegisterWebSocketConnection(int64_t user_id, const std::string& session_id, 
                                             std::function<void(const im::Message&)> message_callback) {
        std::lock_guard<std::mutex> lock(websocket_mutex_);
        
        WebSocketConnection connection;
        connection.session_id = session_id;
        connection.message_callback = message_callback;
        
        websocket_connections_[user_id].push_back(connection);
        
        LOG_INFO("已注册用户 {} 的WebSocket连接 (会话ID: {})", user_id, session_id);
        return true;
    }
    
    void IMServer::UnregisterWebSocketConnection(int64_t user_id, const std::string& session_id) {
        std::lock_guard<std::mutex> lock(websocket_mutex_);
        
        auto it = websocket_connections_.find(user_id);
        if (it != websocket_connections_.end()) {
            auto& connections = it->second;
            connections.erase(
                std::remove_if(connections.begin(), connections.end(),
                    [&session_id](const WebSocketConnection& conn) {
                        return conn.session_id == session_id;
                    }),
                connections.end()
            );
            
            if (connections.empty()) {
                websocket_connections_.erase(it);
            }
            
            LOG_INFO("已注销用户 {} 的WebSocket连接 (会话ID: {})", user_id, session_id);
        }
    }
    
    bool IMServer::LoadConfig(const std::string& config_path) {
        LOG_INFO("加载配置文件: {}", config_path);
        
        if (!im::utils::Config::GetInstance().Load(config_path)) {
            LOG_CRITICAL("无法加载配置文件: {}", config_path);
            return false;
        }
        
        // 读取服务器配置
        server_address_ = im::utils::Config::GetInstance().GetString("server.address", "0.0.0.0");
        server_port_ = im::utils::Config::GetInstance().GetInt("server.port", 50051);
        use_ssl_ = im::utils::Config::GetInstance().GetBool("server.ssl.enabled", false);
        
        if (use_ssl_) {
            cert_path_ = im::utils::Config::GetInstance().GetString("server.ssl.cert_path", "");
            key_path_ = im::utils::Config::GetInstance().GetString("server.ssl.key_path", "");
            
            if (cert_path_.empty() || key_path_.empty()) {
                LOG_ERROR("SSL已启用但证书或密钥路径为空");
                return false;
            }
        }
        
        // 读取数据库配置
        db_host_ = im::utils::Config::GetInstance().GetString("database.mysql.host", "localhost");
        db_port_ = im::utils::Config::GetInstance().GetInt("database.mysql.port", 3306);
        db_user_ = im::utils::Config::GetInstance().GetString("database.mysql.user", "root");
        db_password_ = im::utils::Config::GetInstance().GetString("database.mysql.password", "123456");
        db_name_ = im::utils::Config::GetInstance().GetString("database.mysql.database", "im_db");
        
        // 读取Redis配置
        redis_host_ = im::utils::Config::GetInstance().GetString("database.redis.host", "localhost");
        redis_port_ = im::utils::Config::GetInstance().GetInt("database.redis.port", 6379);
        redis_user_ = im::utils::Config::GetInstance().GetString("database.redis.user", "root");
        redis_password_ = im::utils::Config::GetInstance().GetString("database.redis.password", "123456");
        redis_db_ = im::utils::Config::GetInstance().GetInt("database.redis.db", 0);
        
        // 读取Kafka配置
        kafka_brokers_ = im::utils::Config::GetInstance().GetString("kafka.brokers", "localhost:9092");
        
        LOG_INFO("配置加载成功");
        return true;
    }
    
    bool IMServer::InitializeDatabase() {
        LOG_INFO("初始化MySQL连接...");
        
        try {
            mysql_conn_ = std::make_shared<im::db::MySQLConnection>(
                db_host_, db_port_, db_user_, db_password_, db_name_);
            
            if (!mysql_conn_->Connect()) {
                LOG_CRITICAL("无法连接到MySQL: {}:{}", db_host_, db_port_);
                return false;
            }
            
            LOG_INFO("MySQL连接成功");
            return true;
        } catch (const std::exception& e) {
            LOG_CRITICAL("初始化MySQL连接失败: {}", e.what());
            return false;
        }
    }
    
    bool IMServer::InitializeRedis() {
        LOG_INFO("初始化Redis客户端...");
        
        try {
            redis_client_ = std::make_shared<im::db::RedisClient>(
                redis_host_, redis_port_, redis_user_, redis_password_);
            
            std::cout << "正在连接Redis服务器: " << redis_host_ << ":" << redis_port_ << std::endl;
            
            // 使用异步连接，避免阻塞主线程
            redis_client_->ConnectAsync();
            
            // 不阻塞地继续初始化其他组件
            LOG_INFO("Redis客户端初始化成功，连接将在后台建立");
            return true;
        } catch (const std::exception& e) {
            LOG_CRITICAL("初始化Redis客户端失败: {}", e.what());
            return false;
        }
    }
    
    bool IMServer::InitializeKafka() {
        LOG_INFO("初始化Kafka...");
        
        try {
            kafka_producer_ = std::make_shared<im::kafka::KafkaProducer>(
                kafka_brokers_, "im_messages", [](const std::string& topic, const std::string& payload, bool success){
                    if (!success) {
                        LOG_WARN("发送到Kafka主题 {} 失败", topic);
                    }
                });
            
            LOG_INFO("Kafka生产者初始化成功");
            return true;
        } catch (const std::exception& e) {
            LOG_CRITICAL("初始化Kafka失败: {}", e.what());
            return false;
        }
    }
    
    bool IMServer::InitializeGrpcServices() {
        LOG_INFO("初始化gRPC服务...");
    
        try {
            // 创建服务实例
            message_service_ = std::make_unique<im::MessageServiceImpl>(
                mysql_conn_,
                redis_client_,
                kafka_producer_,
                this
            );
    
            user_service_ = std::make_unique<im::UserServiceImpl>(
                mysql_conn_, redis_client_, kafka_producer_
            );
    
            file_service_ = std::make_unique<im::FileServiceImpl>(
                mysql_conn_, redis_client_, kafka_producer_
            );
    
            push_notification_service_ = std::make_unique<im::PushNotificationServiceImpl>(
                mysql_conn_, redis_client_, kafka_producer_
            );
    
            relation_service_ = std::make_unique<im::RelationServiceImpl>(
                mysql_conn_, redis_client_, kafka_producer_
            );
    
            admin_service_ = std::make_unique<im::AdminServiceImpl>(
                mysql_conn_, redis_client_, kafka_producer_
            );
    
            LOG_INFO("gRPC服务初始化成功");
            return true;
        } catch (const std::exception& e) {
            LOG_CRITICAL("初始化gRPC服务失败: {}", e.what());
            return false;
        }
    }

    bool IMServer::InitializeWebSocket() {
        LOG_INFO("初始化WebSocket服务...");
        
        try {
            // 创建WebSocket处理器
            websocket_handler_ = std::make_shared<WebSocketHandler>(redis_client_);
            
            // 获取WebSocket端口
            int websocket_port = im::utils::Config::GetInstance().GetInt("server.websocket_port", 8081);
            
            // 创建WebSocket监听器
            auto const address = boost::asio::ip::make_address(server_address_);
            websocket_acceptor_ = std::make_unique<tcp::acceptor>(io_context_, 
                tcp::endpoint(address, websocket_port));
            
            // 开始接受连接
            do_accept();
            
            // 启动IO上下文（在单独的线程中运行）
            websocket_thread_ = std::thread([this]() {
                try {
                    LOG_INFO("WebSocket IO线程启动");
                    io_context_.run();
                    LOG_INFO("WebSocket IO线程退出");
                } catch (const std::exception& e) {
                    LOG_CRITICAL("WebSocket线程异常: {}", e.what());
                }
            });
            
            LOG_INFO("WebSocket服务初始化成功，监听端口: {}", websocket_port);
            return true;
        } catch (const std::exception& e) {
            LOG_CRITICAL("初始化WebSocket服务失败: {}", e.what());
            return false;
        }
    }

    void IMServer::do_accept() {
        websocket_acceptor_->async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    LOG_INFO("接收到新的WebSocket连接");
                    websocket_handler_->HandleNewConnection(std::move(socket));
                } else {
                    LOG_ERROR("WebSocket接受连接失败: {}", ec.message());
                }
                
                // 继续接受下一个连接
                do_accept();
            });
    }
}

