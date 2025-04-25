#include "server/utils/datetime.h"
#include "server/utils/logger.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace im {
namespace utils {

const std::string DateTime::kDefaultFormat = "%Y-%m-%d %H:%M:%S";

int64_t DateTime::NowSeconds() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

int64_t DateTime::NowMilliseconds() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

int64_t DateTime::NowMicroseconds() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

std::string DateTime::NowString() {
    return FormatTimestamp(NowSeconds());
}

std::string DateTime::FormatTimestamp(int64_t timestamp) {
    return FormatTimestamp(timestamp, kDefaultFormat);
}

std::string DateTime::FormatTimestamp(int64_t timestamp, const std::string& format) {
    try {
        std::time_t time = static_cast<std::time_t>(timestamp);
        std::tm tm_time;
        
#ifdef _WIN32
        localtime_s(&tm_time, &time);
#else
        localtime_r(&time, &tm_time);
#endif

        std::stringstream ss;
        ss << std::put_time(&tm_time, format.c_str());
        return ss.str();
    } catch (const std::exception& e) {
        LOG_ERROR("Format timestamp error: {}", e.what());
        return "";
    }
}

int64_t DateTime::ParseDatetime(const std::string& datetime) {
    return ParseDatetime(datetime, kDefaultFormat);
}

int64_t DateTime::ParseDatetime(const std::string& datetime, const std::string& format) {
    try {
        std::tm tm_time = {};
        std::istringstream ss(datetime);
        ss >> std::get_time(&tm_time, format.c_str());
        
        if (ss.fail()) {
            LOG_ERROR("Parse datetime failed: {}", datetime);
            return 0;
        }
        
        std::time_t time = std::mktime(&tm_time);
        return static_cast<int64_t>(time);
    } catch (const std::exception& e) {
        LOG_ERROR("Parse datetime error: {}", e.what());
        return 0;
    }
}

int64_t DateTime::DiffSeconds(int64_t timestamp1, int64_t timestamp2) {
    return std::abs(timestamp1 - timestamp2);
}

int64_t DateTime::DiffMilliseconds(int64_t timestamp1, int64_t timestamp2) {
    return std::abs(timestamp1 - timestamp2);
}

int64_t DateTime::DiffMicroseconds(int64_t timestamp1, int64_t timestamp2) {
    return std::abs(timestamp1 - timestamp2);
}

int64_t DateTime::AddSeconds(int64_t timestamp, int64_t seconds) {
    return timestamp + seconds;
}

int64_t DateTime::AddMilliseconds(int64_t timestamp, int64_t milliseconds) {
    return timestamp + milliseconds;
}

int64_t DateTime::StartOfDay() {
    return StartOfDay(NowSeconds());
}

int64_t DateTime::StartOfDay(int64_t timestamp) {
    try {
        std::time_t time = static_cast<std::time_t>(timestamp);
        std::tm tm_time;
        
#ifdef _WIN32
        localtime_s(&tm_time, &time);
#else
        localtime_r(&time, &tm_time);
#endif

        tm_time.tm_hour = 0;
        tm_time.tm_min = 0;
        tm_time.tm_sec = 0;
        
        std::time_t day_start = std::mktime(&tm_time);
        return static_cast<int64_t>(day_start);
    } catch (const std::exception& e) {
        LOG_ERROR("Start of day error: {}", e.what());
        return timestamp;
    }
}

int64_t DateTime::EndOfDay() {
    return EndOfDay(NowSeconds());
}

int64_t DateTime::EndOfDay(int64_t timestamp) {
    try {
        std::time_t time = static_cast<std::time_t>(timestamp);
        std::tm tm_time;
        
#ifdef _WIN32
        localtime_s(&tm_time, &time);
#else
        localtime_r(&time, &tm_time);
#endif

        tm_time.tm_hour = 23;
        tm_time.tm_min = 59;
        tm_time.tm_sec = 59;
        
        std::time_t day_end = std::mktime(&tm_time);
        return static_cast<int64_t>(day_end);
    } catch (const std::exception& e) {
        LOG_ERROR("End of day error: {}", e.what());
        return timestamp;
    }
}

} // namespace utils
} // namespace im 