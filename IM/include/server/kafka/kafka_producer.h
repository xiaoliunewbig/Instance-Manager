#pragma once

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include <librdkafka/rdkafkacpp.h>

// 前向声明，避免引入librdkafka头文件
namespace RdKafka {
    class Producer;
    class Topic;
    class Conf;
}

namespace im {
namespace kafka {

/**
 * @brief Kafka消息生产者类
 */
class KafkaProducer : public RdKafka::DeliveryReportCb {
public:
    /**
     * @brief 交付回调函数类型
     */
    using DeliveryCallback = std::function<void(const std::string&, const std::string&, bool)>;

    /**
     * @brief 构造函数
     * @param brokers Kafka服务器地址列表（逗号分隔）
     * @param client_id 客户端ID
     * @param callback 消息发送回调函数
     */
    KafkaProducer(
        const std::string& brokers,
        const std::string& client_id,
        DeliveryCallback callback = nullptr
    );
    /**
     * @brief 判断生产者是否有效
     * @return 有效返回true，无效返回false
     */
    bool IsValid() const { return producer_ != nullptr; }

    /**
     * @brief 析构函数
     */
    ~KafkaProducer();

    /**
     * @brief 初始化生产者
     * @return 成功返回true，失败返回false
     */
    bool Initialize();

    /**
     * @brief 发送消息
     * @param topic 主题名称
     * @param payload 消息内容
     * @param key 可选的消息键（用于分区分配）
     * @return 成功返回true，失败返回false
     */
    bool SendMessage(
        const std::string& topic,
        const std::string& payload,
        const std::string& key = ""
    );

    /**
     * @brief 刷新未发送的消息
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功返回true，失败返回false
     */
    bool Flush(int timeout_ms = 1000);

    /**
     * @brief 获取上次错误信息
     * @return 错误信息
     */
    std::string GetLastError() const;
    
    // 实现 RdKafka::DeliveryReportCb 接口
    virtual void dr_cb(RdKafka::Message& message);

private:
    /**
     * @brief 获取或创建主题对象
     * @param topic 主题名称
     * @return 主题对象指针
     */
    RdKafka::Topic* GetTopic(const std::string& topic);

    /**
     * @brief 记录日志，调试用
     * @param message 日志消息
     */
    void LogDebug(const std::string& message) const;

    /**
     * @brief 记录错误日志
     * @param message 错误信息
     */
    void LogError(const std::string& message) const;

    std::string brokers_;
    std::string client_id_;
    DeliveryCallback callback_;
    
    std::unique_ptr<RdKafka::Conf> conf_;
    std::unique_ptr<RdKafka::Producer> producer_;
    std::unordered_map<std::string, std::unique_ptr<RdKafka::Topic>> topics_;
    
    mutable std::mutex mutex_;
    std::string last_error_;
    bool initialized_;
};

} // namespace kafka
} // namespace im

