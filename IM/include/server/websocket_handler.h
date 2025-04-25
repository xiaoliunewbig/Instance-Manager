#ifndef IM_WEBSOCKET_HANDLER_H
#define IM_WEBSOCKET_HANDLER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "db/redis_client.h"

namespace im {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// 前向声明
class WebSocketHandler;

/**
 * @brief WebSocket会话类，处理单个WebSocket连接
 */
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    /**
     * @brief 构造函数
     * @param socket TCP套接字
     */
    explicit WebSocketSession(tcp::socket socket, const std::string& path);

    /**
     * @brief 开始会话
     */
    void Start();

    /**
     * @brief 发送消息
     * @param message 消息内容
     */
    void Send(const std::string& message);

    /**
     * @brief 关闭会话
     */
    void Close();

    /**
     * @brief 获取会话ID
     * @return 会话ID
     */
    const std::string& GetID() const;

    /**
     * @brief 获取用户ID
     * @return 用户ID
     */
    int64_t GetUserID() const;

    /**
     * @brief 设置用户ID
     * @param user_id 用户ID
     */
    void SetUserID(int64_t user_id);

    /**
     * @brief 判断会话是否过期
     * @param now 当前时间戳
     * @return 过期返回true，否则返回false
     */
    bool IsExpired(int64_t now) const;

    /**
     * @brief 更新活跃时间
     */
    void UpdateActiveTime();

    /**
     * @brief 设置WebSocket处理器引用
     * @param handler WebSocket处理器
     */
    void SetHandler(std::shared_ptr<WebSocketHandler> handler);

    /**
     * @brief 判断会话是否已认证
     * @return 已认证返回true，否则返回false
     */
    bool IsAuthorized() const {
        return authorized_;
    }

    /**
     * @brief 获取最后活跃时间
     * @return 最后活跃时间戳
     */
    int64_t GetLastActiveTime() const {
        return last_active_time_;
    }

private:
    /**
     * @brief 执行握手
     */
    void DoAccept();

    /**
     * @brief 读取消息
     */
    void DoRead();

    /**
     * @brief 写入消息
     * @param message 消息内容
     */
    void DoWrite(const std::string& message);

    /**
     * @brief 处理接收到的消息
     * @param message 消息内容
     */
    void OnMessage(const std::string& message);

    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    std::string session_id_;
    std::string path_;  // 添加 path_ 成员变量
    int64_t user_id_;
    int64_t create_time_;
    int64_t last_active_time_;
    bool authorized_;
    std::weak_ptr<WebSocketHandler> handler_;
};

/**
 * @brief WebSocket处理器类，管理所有WebSocket连接
 */
class WebSocketHandler : public std::enable_shared_from_this<WebSocketHandler> {
public:
    /**
     * @brief 构造函数
     * @param redis_client Redis客户端
     */
    explicit WebSocketHandler(std::shared_ptr<db::RedisClient> redis_client);

    /**
     * @brief 析构函数
     */
    ~WebSocketHandler();

    /**
     * @brief 处理新连接
     * @param socket TCP套接字
     */
    void HandleNewConnection(tcp::socket socket);

    /**
     * @brief 向指定用户发送消息
     * @param user_id 用户ID
     * @param message 消息内容
     * @return 成功返回true，失败返回false
     */
    bool SendToUser(int64_t user_id, const std::string& message);

    /**
     * @brief 向所有用户广播消息
     * @param message 消息内容
     */
    void Broadcast(const std::string& message);

    /**
     * @brief 管理连接，清理过期会话
     */
    void ManageSessions();

    /**
     * @brief 添加用户会话
     * @param user_id 用户ID
     * @param session_id 会话ID
     */
    void AddUserSession(int64_t user_id, const std::string& session_id);

    /**
     * @brief 获取用户会话数量
     * @param user_id 用户ID
     * @return 用户的会话数量
     */
    size_t GetUserSessionCount(int64_t user_id) const;

    /**
     * @brief 获取所有在线用户ID
     * @return 在线用户ID列表
     */
    std::vector<int64_t> GetOnlineUsers() const;

    /**
     * @brief 判断用户是否在线
     * @param user_id 用户ID
     * @return 在线返回true，离线返回false
     */
    bool IsUserOnline(int64_t user_id) const;

    /**
     * @brief 获取符合特定条件的会话
     * @param filter 过滤函数，接受WebSocketSession指针，返回bool
     * @return 符合条件的会话列表
     */
    std::vector<std::shared_ptr<WebSocketSession>> GetSessions(
        std::function<bool(const std::shared_ptr<WebSocketSession>&)> filter = nullptr) const;

    /**
     * @brief 获取会话总数
     * @return 会话总数
     */
    size_t GetSessionCount() const;

    /**
     * @brief 获取用户的好友ID列表
     * @param user_id 用户ID
     * @return 好友ID列表
     */
    std::vector<int64_t> GetUserFriends(int64_t user_id) const;

    /**
     * @brief 通知用户状态变更
     * @param user_id 状态发生变更的用户ID
     * @param status 新状态
     */
    void NotifyUserStatusChange(int64_t user_id, const std::string& status);

    /**
     * @brief 获取Redis客户端
     * @return Redis客户端指针
     */
    std::shared_ptr<db::RedisClient> GetRedisClient() const {
        return redis_client_;
    }

    /**
     * @brief 向用户的好友发送消息
     * @param user_id 用户ID
     * @param message 要发送的消息内容
     * @param exclude_user_id 要排除的用户ID，默认为0（不排除任何用户）
     * @return 成功发送消息的用户数量
     */
    int SendToUserFriends(int64_t user_id, const std::string& message, int64_t exclude_user_id = 0);

    /**
     * @brief 获取特定用户的所有会话
     * @param user_id 用户ID
     * @return 该用户的所有会话指针列表
     */
    std::vector<std::shared_ptr<WebSocketSession>> GetUserSessions(int64_t user_id) const;

    /**
     * @brief 处理僵尸会话（长时间不活跃但未过期的会话）
     * @param inactive_threshold_seconds 不活跃的时间阈值（秒）
     * @return 处理的僵尸会话数量
     */
    int HandleZombieSessions(int64_t inactive_threshold_seconds);

    /**
     * @brief 获取指定ID的会话
     * @param session_id 会话ID
     * @return 会话指针，如果不存在则返回nullptr
     */
    std::shared_ptr<WebSocketSession> GetSession(const std::string& session_id) const;

    /**
     * @brief 向群组发送消息
     * @param group_id 群组ID
     * @param message 消息内容
     * @param exclude_user_id 要排除的用户ID，默认为0（不排除任何用户）
     * @return 成功发送消息的用户数量
     */
    int SendToGroup(int64_t group_id, const std::string& message, int64_t exclude_user_id = 0);

    /**
     * @brief 获取用户所在的群组ID列表
     * @param user_id 用户ID
     * @return 群组ID列表
     */
    std::vector<int64_t> GetUserGroups(int64_t user_id) const;

    /**
     * @brief 获取群组的成员ID列表
     * @param group_id 群组ID
     * @return 成员ID列表
     */
    std::vector<int64_t> GetGroupMembers(int64_t group_id) const;

    /**
     * @brief 通知群组成员用户状态变更
     * @param user_id 状态发生变更的用户ID
     * @param status 新状态
     * @param group_id 群组ID，如果指定则只通知该群组成员，否则通知所有相关群组
     */
    void NotifyGroupUserStatusChange(int64_t user_id, const std::string& status, int64_t group_id = 0);

    /**
     * @brief 向多个用户批量发送消息
     * @param user_ids 用户ID列表
     * @param message 要发送的消息内容
     * @return 成功发送消息的用户数量
     */
    int SendToUsers(const std::vector<int64_t>& user_ids, const std::string& message);

    /**
     * @brief 记录用户最后在线时间
     * @param user_id 用户ID
     * @param timestamp 时间戳，默认为当前时间
     */
    void UpdateUserLastSeen(int64_t user_id, int64_t timestamp = 0);

    /**
     * @brief 获取用户最后在线时间
     * @param user_id 用户ID
     * @return 用户最后在线时间戳，未找到则返回0
     */
    int64_t GetUserLastSeen(int64_t user_id) const;

    /**
     * @brief 发送消息确认
     * @param to_user_id 接收确认的用户ID
     * @param message_id 原始消息ID
     * @param status 确认状态 ("delivered"已送达, "read"已读)
     * @return 成功返回true，失败返回false
     */
    bool SendMessageAcknowledgement(int64_t to_user_id, int64_t message_id, const std::string& status);

protected:
    std::shared_ptr<db::RedisClient> redis_client_;

private:
    /**
     * @brief 移除会话
     * @param session_id 会话ID
     */
    void RemoveSession(const std::string& session_id);

    mutable std::mutex sessions_mutex_;
    std::unordered_map<std::string, std::shared_ptr<WebSocketSession>> sessions_;
    std::unordered_map<int64_t, std::vector<std::string>> user_sessions_;
    
    // 清理线程相关成员
    std::thread cleanup_thread_;
    std::atomic<bool> running_;
};

} // namespace im

#endif // IM_WEBSOCKET_HANDLER_H 