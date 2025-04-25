#ifndef IM_LOGGER_H
#define IM_LOGGER_H

#include <string>
#include <memory>
#include <spdlog/spdlog.h>

namespace im {
namespace utils {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL,
    OFF
};

/**
 * @brief 日志工具类
 */
class Logger {
public:
    /**
     * @brief 初始化日志系统
     * @param level 日志级别
     * @param log_file 日志文件路径，为空时仅输出到控制台
     */
    static void Initialize(const std::string& level, const std::string& log_file = "");

    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    static void SetLevel(LogLevel level);

    /**
     * @brief 设置日志级别
     * @param level 日志级别字符串
     */
    static void SetLevel(const std::string& level);

    /**
     * @brief 获取当前日志级别
     * @return 日志级别
     */
    static LogLevel GetLevel();

    /**
     * @brief 获取日志记录器
     * @return 日志记录器指针
     */
    static std::shared_ptr<spdlog::logger> GetLogger();

    /**
     * @brief 获取服务器日志记录器
     * @return 服务器日志记录器指针
     */
    static std::shared_ptr<spdlog::logger> GetServerLogger();

    /**
     * @brief 记录日志到数据库
     * @param level 日志级别
     * @param message 日志消息
     * @param source 日志来源
     */
    static void LogToDatabase(LogLevel level, const std::string& message, const std::string& source = "server");

private:
    /**
     * @brief 将字符串转换为日志级别枚举
     * @param level 日志级别字符串
     * @return 日志级别枚举
     */
    static LogLevel StringToLogLevel(const std::string& level);

    /**
     * @brief 将日志级别枚举转换为spdlog级别
     * @param level 日志级别枚举
     * @return spdlog日志级别
     */
    static spdlog::level::level_enum ToSpdLogLevel(LogLevel level);

    // 日志记录器
    static std::shared_ptr<spdlog::logger> logger_;
    // 数据库日志记录器
    static std::shared_ptr<spdlog::logger> server_logger_;
    // 当前日志级别
    static LogLevel current_level_;
    // 是否已初始化
    static bool initialized_;
};

} // namespace utils
} // namespace im

// 日志宏定义
#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(::im::utils::Logger::GetLogger(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(::im::utils::Logger::GetLogger(), __VA_ARGS__)
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(::im::utils::Logger::GetLogger(), __VA_ARGS__)
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(::im::utils::Logger::GetLogger(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(::im::utils::Logger::GetLogger(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(::im::utils::Logger::GetLogger(), __VA_ARGS__)

// 服务器日志宏定义
#define SERVER_LOG_INFO(...) SPDLOG_LOGGER_INFO(::im::utils::Logger::GetServerLogger(), __VA_ARGS__)
#define SERVER_LOG_WARN(...) SPDLOG_LOGGER_WARN(::im::utils::Logger::GetServerLogger(), __VA_ARGS__)
#define SERVER_LOG_ERROR(...) SPDLOG_LOGGER_ERROR(::im::utils::Logger::GetServerLogger(), __VA_ARGS__)

#endif // IM_LOGGER_H 