#include "server/utils/config.h"
#include "server/utils/logger.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <regex> // 添加正则表达式头文件

namespace im {
namespace utils {

Config& Config::GetInstance() {
    static Config instance;
    return instance;
}

bool Config::Load(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        // 添加路径诊断日志
        LogDebug("Current working directory: " + std::filesystem::current_path().string());
        LogDebug("Attempting to load config from: " + std::filesystem::absolute(config_path).string());
        if (!std::filesystem::exists(config_path)) {
            LogError("Config file not found: " + config_path);
            return false;
        }

        // 读取文件内容到字符串
        std::ifstream file(config_path);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
        // 预处理移除注释（需要regex头文件）
        content = std::regex_replace(content, std::regex("//.*"), "");
        
        // 解析预处理后的内容
        config_ = nlohmann::json::parse(content);
        config_path_ = config_path;
        
        LogDebug("Config loaded successfully from: " + config_path);
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        // 增强错误输出显示具体行号
        LogError("JSON parse error at byte " + std::to_string(e.byte) + ": " + e.what());
        // 输出错误附近的配置内容（前200字节）
        std::ifstream file(config_path);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        size_t error_pos = std::min<size_t>(e.byte, content.size());
        LogError("Error context: " + content.substr(error_pos > 200 ? error_pos - 200 : 0, 400));
        return false;
    } catch (const std::exception& e) {
        LogError("Error loading config: " + std::string(e.what()));
        return false;
    }
}

bool Config::Reload() {
    if (config_path_.empty()) {
        LogError("Cannot reload config: No config file has been loaded");
        return false;
    }
    return Load(config_path_);
}

std::string Config::GetString(const std::string& key, const std::string& default_value) const {
    try {
        nlohmann::json value = GetValue(key);
        if (value.is_string()) {
            return value.get<std::string>();
        }
    } catch (const std::exception& e) {
        LogWarning("配置项 '" + key + "' 未找到，使用默认值: " + default_value);
    }
    return default_value;
}

int Config::GetInt(const std::string& key, int default_value) const {
    try {
        nlohmann::json value = GetValue(key);
        if (value.is_number_integer()) {
            return value.get<int>();
        }
    } catch (const std::exception& e) {
        LogDebug("GetInt failed for key '" + key + "': " + e.what());
    }
    return default_value;
}

bool Config::GetBool(const std::string& key, bool default_value) const {
    try {
        nlohmann::json value = GetValue(key);
        if (value.is_boolean()) {
            return value.get<bool>();
        }
    } catch (const std::exception& e) {
        LogDebug("GetBool failed for key '" + key + "': " + e.what());
    }
    return default_value;
}

double Config::GetDouble(const std::string& key, double default_value) const {
    try {
        nlohmann::json value = GetValue(key);
        if (value.is_number()) {
            return value.get<double>();
        }
    } catch (const std::exception& e) {
        LogDebug("GetDouble failed for key '" + key + "': " + e.what());
    }
    return default_value;
}

std::vector<std::string> Config::GetStringArray(const std::string& key) const {
    std::vector<std::string> result;
    try {
        nlohmann::json value = GetValue(key);
        if (value.is_array()) {
            for (const auto& item : value) {
                if (item.is_string()) {
                    result.push_back(item.get<std::string>());
                }
            }
        }
    } catch (const std::exception& e) {
        LogDebug("GetStringArray failed for key '" + key + "': " + e.what());
    }
    return result;
}

std::vector<int> Config::GetIntArray(const std::string& key) const {
    std::vector<int> result;
    try {
        nlohmann::json value = GetValue(key);
        if (value.is_array()) {
            for (const auto& item : value) {
                if (item.is_number_integer()) {
                    result.push_back(item.get<int>());
                }
            }
        }
    } catch (const std::exception& e) {
        LogDebug("GetIntArray failed for key '" + key + "': " + e.what());
    }
    return result;
}

nlohmann::json Config::GetObject(const std::string& key) const {
    try {
        return GetValue(key);
    } catch (const std::exception& e) {
        LogDebug("GetObject failed for key '" + key + "': " + e.what());
        return nlohmann::json::object();
    }
}

bool Config::HasKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto keys = ParseKey(key);
        nlohmann::json current = config_;
        
        for (const auto& k : keys) {
            if (!current.is_object() || !current.contains(k)) {
                return false;
            }
            current = current[k];
        }
        return true;
    } catch (const std::exception& e) {
        LogDebug("HasKey failed for key '" + key + "': " + e.what());
        return false;
    }
}

nlohmann::json Config::GetAllConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void Config::Set(const std::string& key, const nlohmann::json& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto keys = ParseKey(key);
        if (keys.empty()) {
            LogError("Invalid key: " + key);
            return;
        }
        
        nlohmann::json* current = &config_;
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            const std::string& k = keys[i];
            if (!current->contains(k) || !(*current)[k].is_object()) {
                (*current)[k] = nlohmann::json::object();
            }
            current = &(*current)[k];
        }
        
        (*current)[keys.back()] = value;
        LogDebug("Set key '" + key + "' to a new value");
    } catch (const std::exception& e) {
        LogError("Set failed for key '" + key + "': " + e.what());
    }
}

bool Config::Save(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = file_path.empty() ? config_path_ : file_path;
    
    if (path.empty()) {
        LogError("Cannot save config: No file path specified");
        return false;
    }
    
    try {
        std::ofstream file(path);
        if (!file.is_open()) {
            LogError("Failed to open file for writing: " + path);
            return false;
        }
        
        file << std::setw(4) << config_;
        if (!file) {
            LogError("Failed to write to file: " + path);
            return false;
        }
        
        LogDebug("Config saved successfully to: " + path);
        return true;
    } catch (const std::exception& e) {
        LogError("Error saving config: " + std::string(e.what()));
        return false;
    }
}

nlohmann::json Config::GetValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto keys = ParseKey(key);
    nlohmann::json current = config_;
    
    for (const auto& k : keys) {
        if (!current.is_object() || !current.contains(k)) {
            throw std::runtime_error("Key not found: " + key);
        }
        current = current[k];
    }
    
    return current;
}

std::vector<std::string> Config::ParseKey(const std::string& key) const {
    std::vector<std::string> result;
    std::stringstream ss(key);
    std::string item;
    
    while (std::getline(ss, item, '.')) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    
    return result;
}

void Config::LogDebug(const std::string& message) const {
    // 临时添加标准输出
    std::cout << "[DEBUG] " << message << std::endl;
    LOG_DEBUG("{}", message);
}

void Config::LogWarning(const std::string& message) const {
    LOG_WARN("[Config] {}", message); // 添加模块标识
}

void Config::LogError(const std::string& message) const {
    // 临时添加标准错误输出
    std::cerr << "[ERROR] " << message << std::endl;
    LOG_ERROR("{}", message);
}

nlohmann::json Config::GetOrCreate(const std::string& key, const nlohmann::json& default_value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 使用已有的ParseKey方法
    std::vector<std::string> parts = ParseKey(key);
    nlohmann::json* current = &config_;
    
    bool was_modified = false;
    
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string& part = parts[i];
        
        if (i == parts.size() - 1) {
            // 最后一部分，检查是否存在
            if (!current->contains(part)) {
                (*current)[part] = default_value;
                LogWarning("配置项 '" + key + "' 未找到，自动创建并使用默认值");
                was_modified = true;
            }
            return (*current)[part];
        } else {
            // 中间部分，如果不存在则创建对象
            if (!current->contains(part) || !(*current)[part].is_object()) {
                (*current)[part] = nlohmann::json::object();
                was_modified = true;
            }
            current = &(*current)[part];
        }
    }
    
    // 如果需要跟踪修改状态，可以添加一个成员变量
    // if (was_modified) {
    //     modified_ = true;
    // }
    
    // 不应该到达这里
    return default_value;
}

} // namespace utils
} // namespace im