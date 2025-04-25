#include "server/utils/string_util.h"
#include "server/utils/logger.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <random>
#include <iomanip>
#include <iostream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace im {
namespace utils {

std::string StringUtil::ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string StringUtil::ToUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string StringUtil::Trim(const std::string& str) {
    return RTrim(LTrim(str));
}

std::string StringUtil::LTrim(const std::string& str) {
    std::string result = str;
    result.erase(result.begin(), 
                 std::find_if(result.begin(), result.end(), 
                              [](unsigned char ch) { return !std::isspace(ch); }));
    return result;
}

std::string StringUtil::RTrim(const std::string& str) {
    std::string result = str;
    result.erase(std::find_if(result.rbegin(), result.rend(), 
                             [](unsigned char ch) { return !std::isspace(ch); }).base(), 
                result.end());
    return result;
}

std::vector<std::string> StringUtil::Split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    
    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    
    return result;
}

std::vector<std::string> StringUtil::Split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    
    result.push_back(str.substr(start));
    return result;
}

std::string StringUtil::Join(const std::vector<std::string>& strs, const std::string& delimiter) {
    std::stringstream ss;
    for (size_t i = 0; i < strs.size(); ++i) {
        if (i > 0) {
            ss << delimiter;
        }
        ss << strs[i];
    }
    return ss.str();
}

std::string StringUtil::Replace(const std::string& str, const std::string& oldStr, const std::string& newStr) {
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find(oldStr, pos)) != std::string::npos) {
        result.replace(pos, oldStr.length(), newStr);
        pos += newStr.length();
    }
    return result;
}

bool StringUtil::StartsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

bool StringUtil::EndsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && 
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool StringUtil::Contains(const std::string& str, const std::string& substr) {
    return str.find(substr) != std::string::npos;
}

int StringUtil::ToInt(const std::string& str, int defaultValue) {
    try {
        return std::stoi(str);
    } catch (const std::exception& e) {
        LOG_DEBUG("Failed to convert string to int: {}, error: {}", str, e.what());
        return defaultValue;
    }
}

int64_t StringUtil::ToInt64(const std::string& str, int64_t defaultValue) {
    try {
        return std::stoll(str);
    } catch (const std::exception& e) {
        LOG_DEBUG("Failed to convert string to int64: {}, error: {}", str, e.what());
        return defaultValue;
    }
}

double StringUtil::ToDouble(const std::string& str, double defaultValue) {
    try {
        return std::stod(str);
    } catch (const std::exception& e) {
        LOG_DEBUG("Failed to convert string to double: {}, error: {}", str, e.what());
        return defaultValue;
    }
}

bool StringUtil::ToBool(const std::string& str, bool defaultValue) {
    std::string lower = ToLower(str);
    if (lower == "true" || lower == "yes" || lower == "1" || lower == "on") {
        return true;
    } else if (lower == "false" || lower == "no" || lower == "0" || lower == "off") {
        return false;
    }
    return defaultValue;
}

std::string StringUtil::BinToHex(const unsigned char* data, size_t length) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

std::vector<unsigned char> StringUtil::HexToBin(const std::string& hex) {
    std::vector<unsigned char> result;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte = hex.substr(i, 2);
        try {
            result.push_back(static_cast<unsigned char>(std::stoi(byte, nullptr, 16)));
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to convert hex to bin: {}, error: {}", hex, e.what());
            return std::vector<unsigned char>();
        }
    }
    return result;
}

std::string StringUtil::RandomString(size_t length) {
    static const char charset[] =
        "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, sizeof(charset) - 2);
    
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[distribution(generator)];
    }
    
    return result;
}

std::string StringUtil::GenUUID() {
    boost::uuids::random_generator gen;
    boost::uuids::uuid uuid = gen();
    return boost::uuids::to_string(uuid);
}

std::string StringUtil::UrlEncode(const std::string& url) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : url) {
        // 保留字符：字母、数字、'-'、'_'、'.'、'~'
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // 其他字符都需要转义
        escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
    }

    return escaped.str();
}

std::string StringUtil::UrlDecode(const std::string& url) {
    std::string result;
    result.reserve(url.size());

    for (size_t i = 0; i < url.size(); ++i) {
        if (url[i] == '%') {
            if (i + 2 < url.size()) {
                int value;
                std::istringstream is(url.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                } else {
                    result += '%';
                }
            } else {
                result += '%';
            }
        } else if (url[i] == '+') {
            result += ' ';
        } else {
            result += url[i];
        }
    }

    return result;
}

} // namespace utils
} // namespace im 