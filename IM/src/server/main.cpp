#include "include/server/pch.h"
#include "include/server/server.h"
#include "include/server/utils/logger.h"
#include "include/server/utils/config.h"
#include "include/server/utils/datetime.h"
#include "include/server/utils/error_code.h"
#include <boost/program_options.hpp>
#include <iostream>
#include <signal.h>
#include <thread>
#include <memory>
#include <chrono>
#include <filesystem>

// 全局服务器实例
std::unique_ptr<im::IMServer> g_server;
std::atomic<bool> g_running{true};

// 信号处理函数
void SignalHandler(int signal) {
    std::cout << "接收到信号: " << signal << ", 正在停止服务器..." << std::endl;
    g_running = false;
    if (g_server) {
        g_server->Stop();
    }
}

// 设置信号处理
void SetupSignalHandlers() {
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
#ifdef _WIN32
    signal(SIGBREAK, SignalHandler);
#else
    signal(SIGHUP, SignalHandler);
#endif
}

void PrintBanner() {
    std::cout << "\n"
              << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
              << "          即时通讯服务器 (IM Server)                \n"
              << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
              << "版本: 1.0.0                                        \n"
              << "\n";
}

// 获取当前时间戳（秒）
int64_t GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
}

// 确保目录存在
bool EnsureDirectoryExists(const std::string& path) {
    try {
        std::filesystem::path dir_path(path);
        auto parent_path = dir_path.parent_path();
        
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            std::filesystem::create_directories(parent_path);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "创建目录失败: " << e.what() << std::endl;
        return false;
    }
}

// 守护进程模式
bool DaemonProcess() {
#ifndef _WIN32
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "守护进程创建失败" << std::endl;
        return false;
    }
    
    if (pid > 0) {
        // 父进程退出
        exit(0);
    }
    
    // 子进程继续
    setsid();
    umask(0);
    
    // 关闭标准输入输出
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // 重定向标准输入输出到/dev/null
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    
    if (fd > 2) {
        close(fd);
    }
    
    return true;
#else
    std::cerr << "Windows不支持守护进程模式" << std::endl;
    return false;
#endif
}

int main(int argc, char** argv) {
    try {
        // 处理命令行参数
        namespace po = boost::program_options;
        po::options_description desc("允许的选项");
        desc.add_options()
            ("help,h", "显示帮助信息")
            ("config,c", po::value<std::string>()->default_value("conf/server.jsonc"), "配置文件路径")
            ("port,p", po::value<int>(), "gRPC服务器端口")
            ("ws-port,w", po::value<int>(), "WebSocket服务器端口")
            ("daemon,d", "以守护进程模式运行")
            ("log-level,l", po::value<std::string>(), "日志级别: trace, debug, info, warn, error, critical")
            ("log-file,f", po::value<std::string>(), "日志文件路径")
            ("env,e", po::value<std::string>(), "运行环境: dev, prod")
            ("version,v", "显示版本信息");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            PrintBanner();
            std::cout << desc << std::endl;
            return 0;
        }

        if (vm.count("version")) {
            std::cout << "IM Server 版本 1.0.0" << std::endl;
            return 0;
        }

        // 获取配置文件路径
        std::string config_path = vm["config"].as<std::string>();

        // 设置信号处理器
        SetupSignalHandlers();

        // 尝试加载配置文件
        auto& config = im::utils::Config::GetInstance();
        if (!config.Load(config_path)) {
            std::cerr << "无法加载配置文件: " << config_path << std::endl;
            return 1;
        }
        
        // 确定日志文件路径
        std::string log_file = vm.count("log-file") ? 
            vm["log-file"].as<std::string>() : 
            config.GetString("logging.file.path", "logs/im_server.log");
        
        // 确保日志目录存在
        if (!EnsureDirectoryExists(log_file)) {
            std::cerr << "无法创建日志目录" << std::endl;
            return 1;
        }

        // 守护进程模式
        if (vm.count("daemon")) {
            if (!DaemonProcess()) {
                return 1;
            }
        } else {
            // 只有在非守护进程模式下才打印横幅
            PrintBanner();
        }

        // 设置运行环境
        if (vm.count("env")) {
            std::string env = vm["env"].as<std::string>();
            config.Set("server.env", env);
            std::cout << "设置运行环境: " << env << std::endl;
        }

        // 从配置文件和命令行选项确定日志设置
        std::string log_level = vm.count("log-level") ? 
            vm["log-level"].as<std::string>() : 
            config.GetString("logging.level", "info");
        
        // 初始化日志系统
        spdlog::level::level_enum level;
        
        if (log_level == "trace") level = spdlog::level::trace;
        else if (log_level == "debug") level = spdlog::level::debug;
        else if (log_level == "info") level = spdlog::level::info;
        else if (log_level == "warn") level = spdlog::level::warn;
        else if (log_level == "error") level = spdlog::level::err;
        else if (log_level == "critical") level = spdlog::level::critical;
        else level = spdlog::level::info;
        
        // 正确调用Logger::Initialize，参考测试文件中的调用方式
        im::utils::Logger::Initialize(log_level, log_file);
        
        // 记录启动信息
        LOG_INFO("IM服务器启动中，PID: {}", getpid());
        LOG_INFO("使用配置文件: {}", config_path);
        LOG_INFO("运行环境: {}", config.GetString("server.env", "dev"));
        
        // 从命令行覆盖配置
        if (vm.count("port")) {
            int port = vm["port"].as<int>();
            config.Set("server.port", port);
            LOG_INFO("从命令行设置gRPC端口: {}", port);
        }
        
        if (vm.count("ws-port")) {
            int ws_port = vm["ws-port"].as<int>();
            config.Set("websocket.port", ws_port);
            LOG_INFO("从命令行设置WebSocket端口: {}", ws_port);
        }
        
        // 显示重要配置信息
        LOG_INFO("服务器配置:");
        LOG_INFO("gRPC端口: {}", config.GetInt("server.port", 50051));
        LOG_INFO("WebSocket端口: {}", config.GetInt("websocket.port", 8080));
        LOG_INFO("WebSocket启用状态: {}", config.GetBool("websocket.enabled", true) ? "启用" : "禁用");
        LOG_INFO("数据库: {}:{}", 
            config.GetString("database.mysql.host", "localhost"),
            config.GetInt("database.mysql.port", 3306));
        LOG_INFO("Redis: {}:{}", 
            config.GetString("database.redis.host", "localhost"),
            config.GetInt("database.redis.port", 6379));
        
        if (config.GetBool("kafka.enabled", false)) {
            LOG_INFO("Kafka: {}", config.GetString("kafka.brokers", "localhost:9092"));
        } else {
            LOG_INFO("Kafka: 未启用");
        }
        
        // 创建IM服务器实例
        g_server = std::make_unique<im::IMServer>();
        
        // 初始化服务器
        LOG_INFO("正在初始化服务器...");
        if (!g_server->Initialize(config_path)) {
            LOG_CRITICAL("服务器初始化失败");
            return 1;
        }
        
        LOG_INFO("服务器初始化成功，开始运行...");
        
        // 创建服务器运行线程
        std::thread server_thread([&]() {
            if (!g_server->Run()) {
                LOG_CRITICAL("服务器运行失败");
                g_running = false;
            }
        });
        
        // 主线程监控和状态报告
        int64_t last_status_time = GetCurrentTimestamp();
        int64_t status_interval = config.GetInt("server.status_report_interval", 300); // 默认5分钟报告一次状态
        
        while (g_running) {
            // 每秒检查一次状态
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 定期报告服务器状态
            int64_t current_time = GetCurrentTimestamp();
            if (current_time - last_status_time > status_interval) {
                // 服务器状态监控
                LOG_INFO("服务器状态报告:");
                LOG_INFO("运行时间: {} 秒", current_time - last_status_time);
                
                // 这里可以添加更多状态信息，例如连接数、内存使用等
                // TODO: 添加更多状态监控信息
                
                last_status_time = current_time;
            }
        }
        
        LOG_INFO("正在关闭IM服务器...");
        
        // 等待服务器线程结束
        if (server_thread.joinable()) {
            LOG_INFO("等待服务器线程结束...");
            server_thread.join();
        }
        
        LOG_INFO("IM服务器已安全关闭");
        
        if (!vm.count("daemon")) {
            std::cout << "IM服务器已安全关闭" << std::endl;
        }
    }
    catch (const std::exception& e) {
        if (g_server) {
            g_server->Stop();
        }
        
        LOG_CRITICAL("服务器异常: {}", e.what());
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}