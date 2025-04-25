#ifndef IM_CONFIG_H
#define IM_CONFIG_H

#include <string>
#include <memory>
#include <unordered_map>
#include <any>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

namespace im {
namespace utils {

/**
 * @brief 配置管理类，负责读取和解析配置文件
 */
class Config {
public:
    /**
     * @brief 获取单例实例
     * @return 配置类单例
     */
    static Config& GetInstance();

    /**
     * @brief 加载配置文件
     * @param config_path 配置文件路径
     * @return 成功返回true，失败返回false
     */
    bool Load(const std::string& config_path);

    /**
     * @brief 重新加载配置文件
     * @return 成功返回true，失败返回false
     */
    bool Reload();

    /**
     * @brief 获取字符串配置项
     * @param key 配置项键
     * @param default_value 默认值
     * @return 配置项值
     */
    std::string GetString(const std::string& key, const std::string& default_value = "") const;

    /**
     * @brief 获取整数配置项
     * @param key 配置项键
     * @param default_value 默认值
     * @return 配置项值
     */
    int GetInt(const std::string& key, int default_value = 0) const;

    /**
     * @brief 获取布尔配置项
     * @param key 配置项键
     * @param default_value 默认值
     * @return 配置项值
     */
    bool GetBool(const std::string& key, bool default_value = false) const;

    /**
     * @brief 获取浮点数配置项
     * @param key 配置项键
     * @param default_value 默认值
     * @return 配置项值
     */
    double GetDouble(const std::string& key, double default_value = 0.0) const;

    /**
     * @brief 获取字符串数组配置项
     * @param key 配置项键
     * @return 配置项值
     */
    std::vector<std::string> GetStringArray(const std::string& key) const;

    /**
     * @brief 获取整数数组配置项
     * @param key 配置项键
     * @return 配置项值
     */
    std::vector<int> GetIntArray(const std::string& key) const;

    /**
     * @brief 获取JSON对象配置项
     * @param key 配置项键
     * @return 配置项值
     */
    nlohmann::json GetObject(const std::string& key) const;

    /**
     * @brief 检查配置项是否存在
     * @param key 配置项键
     * @return 存在返回true，不存在返回false
     */
    bool HasKey(const std::string& key) const;

    /**
     * @brief 获取所有配置项
     * @return 所有配置项
     */
    nlohmann::json GetAllConfig() const;

    /**
     * @brief 设置配置项
     * @param key 配置项键
     * @param value 配置项值
     */
    void Set(const std::string& key, const nlohmann::json& value);

    /**
     * @brief 保存配置到文件
     * @param file_path 文件路径，为空时使用上次加载的配置文件路径
     * @return 成功返回true，失败返回false
     */
    bool Save(const std::string& file_path = "");

    /**
     * @brief 获取配置项值，如果不存在则创建
     * @param key 配置项键
     * @param default_value 默认值
     * @return 配置项值
     */
    nlohmann::json GetOrCreate(const std::string& key, const nlohmann::json& default_value);

private:
    /**
     * @brief 构造函数
     */
    Config() = default;

    /**
     * @brief 析构函数
     */
    ~Config() = default;

    /**
     * @brief 禁止拷贝构造
     */
    Config(const Config&) = delete;

    /**
     * @brief 禁止赋值操作
     */
    Config& operator=(const Config&) = delete;

    /**
     * @brief 获取配置项，通过点分隔键名
     * @param key 配置项键
     * @return 配置项值
     */
    nlohmann::json GetValue(const std::string& key) const;

    /**
     * @brief 解析点分隔键名
     * @param key 配置项键
     * @return 键名数组
     */
    std::vector<std::string> ParseKey(const std::string& key) const;

    /**
     * @brief 记录日志，调试用
     * @param message 日志消息
     */
    void LogDebug(const std::string& message) const;

    /**
     * @brief 记录警告日志
     * @param message 警告信息
     */
    void LogWarning(const std::string& message) const;

    /**
     * @brief 记录错误日志
     * @param message 错误信息
     */
    void LogError(const std::string& message) const;

    mutable std::mutex mutex_;
    std::string config_path_;
    nlohmann::json config_;
};

} // namespace utils
} // namespace im

#endif // IM_CONFIG_H