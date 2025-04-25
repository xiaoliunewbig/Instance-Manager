#ifndef IM_SERVER_H
#define IM_SERVER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include "generated/service.grpc.pb.h"
// 改用前向声明替代直接包含
//#include "include/server/message_service.h"
//#include "include/server/user_service.h"
//#include "include/server/file_service.h"
//#include "include/server/push_notification_service.h"
//#include "include/server/notification_service.h"
#include "../../include/server/utils/config.h"
#include "../../include/server/utils/logger.h"
#include "../../include/server/utils/datetime.h"
#include "../../include/server/kafka/kafka_producer.h"
#include "../../include/server/kafka/kafka_consumer.h"
#include "../../include/server/utils/security.h"
#include "../../include/server/websocket_handler.h"
#include "../../include/server/db/redis_client.h"
#include "../../include/server/db/mysql_connection.h"
#include "../../include/server/admin_service.h"
#include "../../include/server/relation_service.h"
//#include "include/server/group_service.h"       // 新增

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/server_builder.h>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include "../../include/server/push_notification_service.h"

namespace db {
class MySQLConnection;
class RedisClient;
}

namespace kafka {
class KafkaProducer;
class KafkaConsumer;
}


namespace im {
class MessageServiceImpl;
class UserServiceImpl;
//class GroupServiceImpl;
class FileServiceImpl;
class PushNotificationServiceImpl;
class NotificationServiceImpl;
class AdminServiceImpl;
class RelationServiceImpl;

/**
 * @brief IM服务器主类，负责管理所有服务实例和连接
 */

class IMServer {
public:
    /**
     * @brief 构造函数
     */
    IMServer();
    
    /**
     * @brief 析构函数
     */
    ~IMServer();
    
    /**
     * @brief 初始化服务器
     * @param config_path 配置文件路径
     * @return 成功返回true，失败返回false
     */
    bool Initialize(const std::string& config_path);
    
    /**
     * @brief 运行服务器
     * @return 成功返回true，失败返回false
     */
    bool Run();
    
    /**
     * @brief 停止服务器
     */
    void Stop();
    
    /**
     * @brief 向用户发送消息
     * @param user_id 用户ID
     * @param message 消息内容
     */
    void SendMessageToUser(int64_t user_id, const im::Message& message);
    
    /**
     * @brief 获取推送通知服务实例
     * @return 推送通知服务实例
     */
    im::PushNotificationServiceImpl* GetPushNotificationService() const;
    
    /**
     * @brief 获取消息服务实例
     * @return 消息服务实例
     */
    im::MessageServiceImpl* GetMessageService() const;
    
    /**
     * @brief 获取用户服务实例
     * @return 用户服务实例
     */
    im::UserServiceImpl* GetUserService() const;
    
    /**
     * @brief 获取组服务实例
     * @return 组服务实例
     */
    //im::GroupServiceImpl* GetGroupService() const;
    
    /**
     * @brief 获取文件服务实例
     * @return 文件服务实例
     */
    im::FileServiceImpl* GetFileService() const;

    /**
     *@brief 获取管理员实例
     *@return 管理员实例
     */
    im::AdminServiceImpl* GetAdminService() const;

    /**
     *@brief 获取关系实例
     *@return 关系实例
     */
     im::RelationServiceImpl* GetRelationService() const;

    /**
     * @brief 获取通知服务实例
     * @return 通知服务实例
     */
    im::NotificationServiceImpl* GetNotificationService() const;
    
    /**
     * @brief 获取MySQL连接
     * @return MySQL连接对象
     */
    std::shared_ptr<im::db::MySQLConnection> GetMySQLConnection() const;
    
    /**
     * @brief 获取Redis客户端
     * @return Redis客户端对象
     */
    std::shared_ptr<im::db::RedisClient> GetRedisClient() const;
    
    /**
     * @brief 获取Kafka生产者
     * @return Kafka生产者对象
     */
    std::shared_ptr<im::kafka::KafkaProducer> GetKafkaProducer() const;
    
    /**
     * @brief 注册WebSocket连接
     * @param user_id 用户ID
     * @param session_id 会话ID
     * @param message_callback 消息回调函数
     * @return 成功返回true，失败返回false
     */
    bool RegisterWebSocketConnection(int64_t user_id, const std::string& session_id, 
                                  std::function<void(const im::Message&)> message_callback);
    
    /**
     * @brief 注销WebSocket连接
     * @param user_id 用户ID
     * @param session_id 会话ID
     */
    void UnregisterWebSocketConnection(int64_t user_id, const std::string& session_id);
    
    /**
     * @brief 获取gRPC服务器实例
     * @return gRPC服务器实例
     */
    grpc::Server* GetGrpcServer() const;
    
    /**
     * @brief 停止WebSocket服务器
     */
    void StopWebSocketServer();
    
    /**
     * @brief 确保所有必要的配置项存在
     */
    void EnsureConfigDefaults();
    
private:
    /**
     * @brief 加载配置
     * @param config_path 配置文件路径
     * @return 成功返回true，失败返回false
     */
    bool LoadConfig(const std::string& config_path);
    
    /**
     * @brief 初始化数据库连接
     * @return 成功返回true，失败返回false
     */
    bool InitializeDatabase();
    
    /**
     * @brief 初始化Redis连接
     * @return 成功返回true，失败返回false
     */
    bool InitializeRedis();
    
    /**
     * @brief 初始化Kafka
     * @return 成功返回true，失败返回false
     */
    bool InitializeKafka();
    
    /**
     * @brief 初始化gRPC服务
     * @return 成功返回true，失败返回false
     */
    bool InitializeGrpcServices();
    
    /**
     * @brief 初始化WebSocket服务
     * @return 成功返回true，失败返回false
     */
    bool InitializeWebSocket();
    
    // 服务器状态
    bool running_;
    
    // 配置相关
    std::string server_address_;
    int server_port_;
    bool use_ssl_;
    std::string cert_path_;
    std::string key_path_;
    
    // 数据库相关
    std::string db_host_;
    int db_port_;
    std::string db_user_;
    std::string db_password_;
    std::string db_name_;
    std::shared_ptr<im::db::MySQLConnection> mysql_conn_;
    
    // Redis相关
    std::string redis_host_;
    int redis_port_;
    std::string redis_user_;
    std::string redis_password_;
    int redis_db_;
    std::shared_ptr<im::db::RedisClient> redis_client_;
    
    // Kafka相关
    std::string kafka_brokers_;
    std::shared_ptr<im::kafka::KafkaProducer> kafka_producer_;
    std::shared_ptr<im::kafka::KafkaConsumer> kafka_consumer_;
    
    // gRPC相关
    std::unique_ptr<grpc::Server> grpc_server_;
    
    // 服务实例
    std::unique_ptr<im::AdminServiceImpl>admin_service_;
    std::unique_ptr<im::RelationServiceImpl>relation_service_;
    std::unique_ptr<im::MessageServiceImpl> message_service_;
    std::unique_ptr<im::UserServiceImpl> user_service_;
    //std::unique_ptr<im::GroupServiceImpl> group_service_;
    std::unique_ptr<im::FileServiceImpl> file_service_;
    std::unique_ptr<im::PushNotificationServiceImpl> push_notification_service_;
    std::unique_ptr<im::NotificationServiceImpl> notification_service_;
    
    // WebSocket相关
    struct WebSocketConnection {
        std::string session_id;
        std::function<void(const im::Message&)> message_callback;
    };
    std::mutex websocket_mutex_;
    std::unordered_map<int64_t, std::vector<WebSocketConnection>> websocket_connections_;
    
    // WebSocket相关
    boost::asio::io_context io_context_;
    std::unique_ptr<tcp::acceptor> websocket_acceptor_;
    std::shared_ptr<WebSocketHandler> websocket_handler_;
    std::thread websocket_thread_;
    
    void do_accept();
};
}   // namespace im

#endif // IM_SERVER_H