#ifndef IM_KAFKA_CONSUMER_H
#define IM_KAFKA_CONSUMER_H

#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <vector>
#include <atomic>
#include <thread>

// 前向声明，避免引入librdkafka头文件
namespace RdKafka {
    class KafkaConsumer;
    class Conf;
    class Message;
}

namespace im {
namespace kafka {

/**
 * @brief Kafka消息消费者类
 */
class KafkaConsumer {
public:
    /**
     * @brief 消息处理回调函数类型
     * @param topic 主题
     * @param partition 分区ID
     * @param offset 消息偏移量
     * @param key 消息键
     * @param payload 消息内容
     * @return 处理成功返回true，失败返回false
     */
    using MessageCallback = std::function<bool(
        const std::string& topic,
        int partition,
        int64_t offset,
        const std::string& key,
        const std::string& payload
    )>;

    /**
     * @brief 构造函数
     * @param brokers Kafka服务器地址列表（逗号分隔）
     * @param group_id 消费者组ID
     * @param topics 要订阅的主题列表
     * @param callback 消息处理回调函数
     */
    KafkaConsumer(
        const std::string& brokers,
        const std::string& group_id,
        const std::vector<std::string>& topics,
        MessageCallback callback
    );

    /**
     * @brief 析构函数
     */
    ~KafkaConsumer();

    /**
     * @brief 初始化消费者
     * @return 成功返回true，失败返回false
     */
    bool Initialize();

    /**
     * @brief 启动消费
     * @return 成功返回true，失败返回false
     */
    bool Start();

    /**
     * @brief 停止消费
     */
    void Stop();

    /**
     * @brief 是否正在运行
     * @return 正在运行返回true，否则返回false
     */
    bool IsRunning() const;

    /**
     * @brief 获取上次错误信息
     * @return 错误信息
     */
    std::string GetLastError() const;

    /**
     * @brief 手动提交偏移量
     * @return 成功返回true，失败返回false
     */
    bool CommitOffsets();

private:
    /**
     * @brief 消费线程入口函数
     */
    void ConsumeThreadFunc();

    /**
     * @brief 处理消息
     * @param message 消息对象
     */
    void HandleMessage(RdKafka::Message* message);

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
    std::string group_id_;
    std::vector<std::string> topics_;
    MessageCallback callback_;
    
    std::unique_ptr<RdKafka::Conf> conf_;
    std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
    
    std::atomic<bool> running_;
    std::thread consume_thread_;
    
    mutable std::mutex mutex_;
    std::string last_error_;
    bool initialized_;
};

} // namespace kafka
} // namespace im

#endif // IM_KAFKA_CONSUMER_H 