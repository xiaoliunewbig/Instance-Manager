#ifndef IM_SERVER_H
#define IM_SERVER_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>

// 修改包含路径为相对路径
#include "generated/service.grpc.pb.h"  // 不使用../../build路径

namespace im {
// 基本框架声明
class IMServer {
public:
    IMServer();
    ~IMServer();
    
    bool Initialize(const std::string& config_path);
    bool Run();
    void Stop();
    
    // 简化版服务器类，只保留基本方法
    struct WebSocketConnection {
        std::string session_id;
        std::function<void(const im::Message&)> message_callback;
    };
    
private:
    std::mutex websocket_mutex_;
    bool running_;
    std::string server_address_;
    int server_port_;
    bool use_ssl_;
    
    // 简化版
    std::unordered_map<int64_t, std::vector<WebSocketConnection>> websocket_connections_;
};
} // namespace im

#endif // IM_SERVER_H
