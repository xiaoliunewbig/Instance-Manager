#ifndef IM_DATETIME_H
#define IM_DATETIME_H

#include <string>
#include <chrono>
#include <ctime>

namespace im {
namespace utils {

/**
 * @brief 日期时间工具类，提供日期时间相关操作
 */
class DateTime {
public:
    /**
     * @brief 获取当前时间戳（秒）
     * @return 当前时间戳
     */
    static int64_t NowSeconds();

    /**
     * @brief 获取当前时间戳（毫秒）
     * @return 当前时间戳
     */
    static int64_t NowMilliseconds();

    /**
     * @brief 获取当前时间戳（微秒）
     * @return 当前时间戳
     */
    static int64_t NowMicroseconds();

    /**
     * @brief 获取当前日期时间字符串，格式：yyyy-MM-dd HH:mm:ss
     * @return 日期时间字符串
     */
    static std::string NowString();

    /**
     * @brief 获取指定时间戳的日期时间字符串，格式：yyyy-MM-dd HH:mm:ss
     * @param timestamp 时间戳（秒）
     * @return 日期时间字符串
     */
    static std::string FormatTimestamp(int64_t timestamp);

    /**
     * @brief 获取指定时间戳的日期时间字符串
     * @param timestamp 时间戳（秒）
     * @param format 格式，例如：%Y-%m-%d %H:%M:%S
     * @return 日期时间字符串
     */
    static std::string FormatTimestamp(int64_t timestamp, const std::string& format);

    /**
     * @brief 解析日期时间字符串为时间戳（秒）
     * @param datetime 日期时间字符串，格式：yyyy-MM-dd HH:mm:ss
     * @return 时间戳
     */
    static int64_t ParseDatetime(const std::string& datetime);

    /**
     * @brief 解析日期时间字符串为时间戳（秒）
     * @param datetime 日期时间字符串
     * @param format 格式，例如：%Y-%m-%d %H:%M:%S
     * @return 时间戳
     */
    static int64_t ParseDatetime(const std::string& datetime, const std::string& format);

    /**
     * @brief 获取两个时间戳的间隔（秒）
     * @param timestamp1 时间戳1（秒）
     * @param timestamp2 时间戳2（秒）
     * @return 间隔（秒）
     */
    static int64_t DiffSeconds(int64_t timestamp1, int64_t timestamp2);

    /**
     * @brief 获取两个时间戳的间隔（毫秒）
     * @param timestamp1 时间戳1（毫秒）
     * @param timestamp2 时间戳2（毫秒）
     * @return 间隔（毫秒）
     */
    static int64_t DiffMilliseconds(int64_t timestamp1, int64_t timestamp2);

    /**
     * @brief 获取两个时间戳的间隔（微秒）
     * @param timestamp1 时间戳1（微秒）
     * @param timestamp2 时间戳2（微秒）
     * @return 间隔（微秒）
     */
    static int64_t DiffMicroseconds(int64_t timestamp1, int64_t timestamp2);

    /**
     * @brief 时间戳（秒）加上指定秒数
     * @param timestamp 时间戳（秒）
     * @param seconds 秒数
     * @return 新的时间戳
     */
    static int64_t AddSeconds(int64_t timestamp, int64_t seconds);

    /**
     * @brief 时间戳（毫秒）加上指定毫秒数
     * @param timestamp 时间戳（毫秒）
     * @param milliseconds 毫秒数
     * @return 新的时间戳
     */
    static int64_t AddMilliseconds(int64_t timestamp, int64_t milliseconds);

    /**
     * @brief 获取当天开始时间戳（秒）
     * @return 当天开始时间戳
     */
    static int64_t StartOfDay();

    /**
     * @brief 获取指定时间戳当天开始时间戳（秒）
     * @param timestamp 时间戳（秒）
     * @return 当天开始时间戳
     */
    static int64_t StartOfDay(int64_t timestamp);

    /**
     * @brief 获取当天结束时间戳（秒）
     * @return 当天结束时间戳
     */
    static int64_t EndOfDay();

    /**
     * @brief 获取指定时间戳当天结束时间戳（秒）
     * @param timestamp 时间戳（秒）
     * @return 当天结束时间戳
     */
    static int64_t EndOfDay(int64_t timestamp);

private:
    /**
     * @brief 默认日期时间格式
     */
    static const std::string kDefaultFormat;
};

} // namespace utils
} // namespace im

#endif // IM_DATETIME_H 