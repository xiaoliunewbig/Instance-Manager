#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <functional>
#include <hiredis/hiredis.h>

// 前向声明hiredis结构体
struct redisContext;
struct redisReply;

namespace im {
namespace db {

/**
 * @brief Redis客户端封装类
 */
class RedisClient {
public:
    /**
     * @brief 构造函数
     * @param host Redis服务器地址
     * @param port Redis服务器端口
     * @param user Redis用户名，可选
     * @param password Redis密码，可选
     */
    RedisClient(
        const std::string& host,
        int port,
        const std::string& user = "",
        const std::string& password = ""
    );

    /**
     * @brief 析构函数
     */
    ~RedisClient();

    /**
     * @brief 连接到Redis服务器
     * @return 成功返回true，失败返回false
     */
    bool Connect();

    /**
     * @brief 断开连接
     */
    void Disconnect();

    /**
     * @brief 检查是否已连接
     * @return 已连接返回true，否则返回false
     */
    bool IsConnected() const;

    /**
     * @brief 设置键值对
     * @param key 键
     * @param value 值
     * @param expire_seconds 过期时间（秒），0表示永不过期
     * @return 成功返回true，失败返回false
     */
    bool SetValue(const std::string& key, const std::string& value, int expire_seconds = 0);

    /**
     * @brief 获取键对应的值
     * @param key 键
     * @return 键对应的值，如果键不存在则返回空字符串
     */
    std::string GetValue(const std::string& key);

    /**
     * @brief 删除键
     * @param key 键
     * @return 成功返回true，失败返回false
     */
    bool DeleteKey(const std::string& key);

    /**
     * @brief 检查键是否存在
     * @param key 键
     * @return 存在返回true，不存在返回false
     */
    bool KeyExists(const std::string& key);

    /**
     * @brief 设置键的过期时间
     * @param key 键
     * @param expire_seconds 过期时间（秒）
     * @return 成功返回true，失败返回false
     */
    bool SetExpire(const std::string& key, int expire_seconds) {
        return Expire(key, expire_seconds);
    }

    /**
     * @brief 获取键的剩余过期时间
     * @param key 键
     * @return 剩余时间（秒），-1表示永不过期，-2表示键不存在
     */
    int GetTTL(const std::string& key);

    /**
     * @brief 原子递增键的值
     * @param key 键
     * @param increment
     * @return 递增后的值，失败返回-1
     */
    int64_t Increment(const std::string& key, int64_t increment = 1);

    /**
     * @brief 原子递减键的值
     * @param key 键
     * @param decrement 减量值
     * @return 递减后的值，失败返回-1
     */
    int64_t Decrement(const std::string& key, int64_t decrement = 1);

    /**
     * @brief 获取列表中的元素
     * @param key 列表的键
     * @param start 起始索引，0表示第一个元素
     * @param end 结束索引，-1表示最后一个元素
     * @return 元素列表
     */
    std::vector<std::string> GetList(const std::string& key, int start = 0, int end = -1);

    /**
     * @brief 向列表头部添加元素
     * @param key 列表的键
     * @param value 要添加的值
     * @return 添加后列表的长度，失败返回-1
     */
    int64_t PushFront(const std::string& key, const std::string& value);

    /**
     * @brief 向列表尾部添加元素
     * @param key 列表的键
     * @param value 要添加的值
     * @return 添加后列表的长度，失败返回-1
     */
    int64_t PushBack(const std::string& key, const std::string& value);

    /**
     * @brief 获取列表长度
     * @param key 列表的键
     * @return 列表长度，键不存在返回0
     */
    int64_t ListLength(const std::string& key);

    /**
     * @brief 获取列表中的元素（别名方法，同GetList）
     * @param key 列表的键
     * @param start 起始索引，0表示第一个元素
     * @param end 结束索引，-1表示最后一个元素
     * @return 元素列表
     */
    std::vector<std::string> ListRange(const std::string& key, int start = 0, int end = -1) {
        return GetList(key, start, end);
    }

    /**
     * @brief 修剪列表，只保留指定范围内的元素
     * @param key 列表的键
     * @param start 起始索引，0表示第一个元素
     * @param end 结束索引，-1表示最后一个元素
     * @return 是否成功
     */
    bool ListTrim(const std::string& key, int start, int end);

    /**
     * @brief 向列表尾部添加元素（别名方法，同PushBack）
     * @param key 列表的键
     * @param value 要添加的值
     * @return 添加后列表的长度，失败返回-1
     */
    int64_t ListPush(const std::string& key, const std::string& value) {
        return PushBack(key, value);
    }

    /**
     * @brief 设置哈希表中的字段
     * @param key 哈希表的键
     * @param field 字段名
     * @param value 字段值
     * @return 成功返回true，失败返回false
     */
    bool SetHashField(const std::string& key, const std::string& field, const std::string& value);

    /**
     * @brief 获取哈希表中的字段值
     * @param key 哈希表的键
     * @param field 字段名
     * @return 字段值，不存在返回空字符串
     */
    std::string GetHashField(const std::string& key, const std::string& field);

    /**
     * @brief 删除哈希表中的字段
     * @param key 哈希表的键
     * @param field 字段名
     * @return 成功返回true，失败返回false
     */
    bool DeleteHashField(const std::string& key, const std::string& field);

    /**
     * @brief 一次设置多个哈希表字段
     * @param key 哈希表的键
     * @param fields 字段名和值的映射
     * @param expire_seconds 过期时间（秒），0表示永不过期
     * @return 成功返回true，失败返回false
     */
    bool SetHashValues(
        const std::string& key,
        const std::map<std::string, std::string>& fields,
        int expire_seconds = 0
    );

    /**
     * @brief 获取哈希表中的所有字段和值
     * @param key 哈希表的键
     * @return 字段名和值的映射，键不存在返回空映射
     */
    std::map<std::string, std::string> GetHashAll(const std::string& key);

    /**
     * @brief 向集合中添加元素
     * @param key 集合的键
     * @param value 要添加的值
     * @return 成功添加的元素数量
     */
    int64_t SetAdd(const std::string& key, const std::string& value);

    /**
     * @brief 从集合中移除元素
     * @param key 集合的键
     * @param value 要移除的值
     * @return 成功移除的元素数量
     */
    int64_t SetRemove(const std::string& key, const std::string& value);

    /**
     * @brief 获取集合中的所有元素
     * @param key 集合的键
     * @return 元素列表
     */
    std::vector<std::string> SetMembers(const std::string& key);

    /**
     * @brief 在指定通道上发布消息
     * @param channel 通道名
     * @param message 消息内容
     * @return 成功返回true，失败返回false
     */
    bool Publish(const std::string& channel, const std::string& message);

    /**
     * @brief 设置键的过期时间
     * @param key 键
     * @param seconds 过期时间（秒）
     * @return 成功返回true，失败返回false
     */
    bool Expire(const std::string& key, int seconds);

    /**
     * @brief 异步连接到Redis服务器
     */
    void ConnectAsync();

private:
    // Redis连接参数
    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    
    // Redis连接上下文
    redisContext* redis_context_;
    
    // 线程安全锁
    mutable std::mutex mutex_;
    
    // 辅助方法
    redisReply* ExecuteCommand(const char* format, ...);
    void FreeReply(redisReply* reply);
};

} // namespace db
} // namespace im 