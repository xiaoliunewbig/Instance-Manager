#include "server/utils/logger.h"
#include <iostream>
#include <algorithm>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>

namespace im {
namespace utils {

// 静态成员初始化
std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;
std::shared_ptr<spdlog::logger> Logger::server_logger_ = nullptr;
LogLevel Logger::current_level_ = LogLevel::INFO;
bool Logger::initialized_ = false;

void Logger::Initialize(const std::string& level, const std::string& log_file) {
    if (initialized_) {
        return;
    }
    
    try {
        // 设置日志格式
        std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [thread %t] %v";
        
        // 创建控制台和文件日志接收器
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        
        if (!log_file.empty()) {
            // 创建每天切换的日志文件，最大5个文件
            auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
                log_file, 0, 0, false, 5);
            sinks.push_back(file_sink);
        }
        
        // 创建主日志记录器
        logger_ = std::make_shared<spdlog::logger>("IM", sinks.begin(), sinks.end());
        
        // 创建服务器日志记录器（用于数据库日志）
        auto server_log_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        server_logger_ = std::make_shared<spdlog::logger>("ServerLog", server_log_sink);
        
        // 设置日志级别
        SetLevel(level);
        
        // 注册日志记录器
        spdlog::register_logger(logger_);
        spdlog::register_logger(server_logger_);
        
        // 设置默认日志记录器
        spdlog::set_default_logger(logger_);
        
        // 设置日志格式
        logger_->set_pattern(pattern);
        server_logger_->set_pattern(pattern);
        
        // 设置刷新策略：每条日志都刷新
        logger_->flush_on(spdlog::level::trace);
        server_logger_->flush_on(spdlog::level::info);
        
        initialized_ = true;
        
        LOG_INFO("日志系统初始化成功，级别: {}", level);
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "日志初始化失败: " << ex.what() << std::endl;
    }
}

void Logger::SetLevel(LogLevel level) {
    current_level_ = level;
    
    if (logger_) {
        logger_->set_level(ToSpdLogLevel(level));
    }
    
    if (server_logger_) {
        server_logger_->set_level(ToSpdLogLevel(level));
    }
}

void Logger::SetLevel(const std::string& level) {
    SetLevel(StringToLogLevel(level));
}

LogLevel Logger::GetLevel() {
    return current_level_;
}

std::shared_ptr<spdlog::logger> Logger::GetLogger() {
    if (!initialized_) {
        Initialize("info");
    }
    return logger_;
}

std::shared_ptr<spdlog::logger> Logger::GetServerLogger() {
    if (!initialized_) {
        Initialize("info");
    }
    return server_logger_;
}

void Logger::LogToDatabase(LogLevel level, const std::string& message, const std::string& source) {
    // 这里实际项目中应该将日志写入数据库
    // 此处简化为使用专门的日志记录器输出
    switch (level) {
        case LogLevel::TRACE:
        case LogLevel::DEBUG:
            SERVER_LOG_INFO("[{}] {}", source, message);
            break;
        case LogLevel::INFO:
            SERVER_LOG_INFO("[{}] {}", source, message);
            break;
        case LogLevel::WARNING:
            SERVER_LOG_WARN("[{}] {}", source, message);
            break;
        case LogLevel::ERROR:
        case LogLevel::CRITICAL:
            SERVER_LOG_ERROR("[{}] {}", source, message);
            break;
        default:
            break;
    }
}

LogLevel Logger::StringToLogLevel(const std::string& level) {
    std::string level_lower = level;
    std::transform(level_lower.begin(), level_lower.end(), level_lower.begin(), ::tolower);
    
    if (level_lower == "trace") return LogLevel::TRACE;
    if (level_lower == "debug") return LogLevel::DEBUG;
    if (level_lower == "info") return LogLevel::INFO;
    if (level_lower == "warning" || level_lower == "warn") return LogLevel::WARNING;
    if (level_lower == "error") return LogLevel::ERROR;
    if (level_lower == "critical" || level_lower == "fatal") return LogLevel::CRITICAL;
    if (level_lower == "off") return LogLevel::OFF;
    
    // 默认级别
    return LogLevel::INFO;
}

spdlog::level::level_enum Logger::ToSpdLogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return spdlog::level::trace;
        case LogLevel::DEBUG: return spdlog::level::debug;
        case LogLevel::INFO: return spdlog::level::info;
        case LogLevel::WARNING: return spdlog::level::warn;
        case LogLevel::ERROR: return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        case LogLevel::OFF: return spdlog::level::off;
        default: return spdlog::level::info;
    }
}

} // namespace utils
} // namespace im 