#include "server/kafka/kafka_consumer.h"
#include "server/utils/logger.h"
#include <librdkafka/rdkafkacpp.h>
#include <chrono>

namespace im {
namespace kafka {

KafkaConsumer::KafkaConsumer(
    const std::string& brokers,
    const std::string& group_id,
    const std::vector<std::string>& topics,
    MessageCallback callback
) : brokers_(brokers),
    group_id_(group_id),
    topics_(topics),
    callback_(callback),
    running_(false),
    initialized_(false) {
}

KafkaConsumer::~KafkaConsumer() {
    Stop();
}

bool KafkaConsumer::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return true;
    }
    
    try {
        // 创建配置对象
        std::string errstr;
        conf_.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
        
        // 设置Kafka服务器地址
        if (conf_->set("bootstrap.servers", brokers_, errstr) != RdKafka::Conf::CONF_OK) {
            last_error_ = "Failed to set bootstrap.servers: " + errstr;
            LogError(last_error_);
            return false;
        }
        
        // 设置消费者组ID
        if (conf_->set("group.id", group_id_, errstr) != RdKafka::Conf::CONF_OK) {
            last_error_ = "Failed to set group.id: " + errstr;
            LogError(last_error_);
            return false;
        }
        
        // 设置自动提交偏移量
        if (conf_->set("enable.auto.commit", "true", errstr) != RdKafka::Conf::CONF_OK) {
            last_error_ = "Failed to set enable.auto.commit: " + errstr;
            LogError(last_error_);
            return false;
        }
        
        // 设置自动提交间隔
        if (conf_->set("auto.commit.interval.ms", "5000", errstr) != RdKafka::Conf::CONF_OK) {
            last_error_ = "Failed to set auto.commit.interval.ms: " + errstr;
            LogError(last_error_);
            return false;
        }
        
        // 设置偏移量重置策略（消费者组第一次启动时）
        if (conf_->set("auto.offset.reset", "earliest", errstr) != RdKafka::Conf::CONF_OK) {
            last_error_ = "Failed to set auto.offset.reset: " + errstr;
            LogError(last_error_);
            return false;
        }
        
        // 创建消费者实例
        consumer_.reset(RdKafka::KafkaConsumer::create(conf_.get(), errstr));
        if (!consumer_) {
            last_error_ = "Failed to create Kafka consumer: " + errstr;
            LogError(last_error_);
            return false;
        }
        
        // 订阅主题
        RdKafka::ErrorCode err = consumer_->subscribe(topics_);
        if (err != RdKafka::ERR_NO_ERROR) {
            last_error_ = "Failed to subscribe to topics: " + RdKafka::err2str(err);
            LogError(last_error_);
            return false;
        }
        
        initialized_ = true;
        LogDebug("Kafka consumer initialized, brokers: " + brokers_ + ", group_id: " + group_id_);
        return true;
    } catch (const std::exception& e) {
        last_error_ = "Exception during Kafka consumer initialization: " + std::string(e.what());
        LogError(last_error_);
        return false;
    }
}

bool KafkaConsumer::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        return true;
    }
    
    if (!initialized_ && !Initialize()) {
        return false;
    }
    
    running_ = true;
    consume_thread_ = std::thread(&KafkaConsumer::ConsumeThreadFunc, this);
    
    LogDebug("Kafka consumer started");
    return true;
}

void KafkaConsumer::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!running_) {
            return;
        }
        
        running_ = false;
    }
    
    // 通知消费者线程停止
    if (consumer_) {
        consumer_->close();
    }
    
    // 等待消费线程结束
    if (consume_thread_.joinable()) {
        consume_thread_.join();
    }
    
    LogDebug("Kafka consumer stopped");
}

bool KafkaConsumer::IsRunning() const {
    return running_;
}

std::string KafkaConsumer::GetLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

bool KafkaConsumer::CommitOffsets() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!consumer_) {
        last_error_ = "Consumer not initialized";
        return false;
    }
    
    RdKafka::ErrorCode err = consumer_->commitSync();
    if (err != RdKafka::ERR_NO_ERROR) {
        last_error_ = "Failed to commit offsets: " + RdKafka::err2str(err);
        LogError(last_error_);
        return false;
    }
    
    return true;
}

void KafkaConsumer::ConsumeThreadFunc() {
    LogDebug("Kafka consumer thread started");
    
    while (running_) {
        try {
            // 轮询消息
            std::unique_ptr<RdKafka::Message> msg(consumer_->consume(1000));
            if (msg) {
                HandleMessage(msg.get());
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_);
            last_error_ = "Exception in consume thread: " + std::string(e.what());
            LogError(last_error_);
            
            // 添加短暂休眠，避免在异常情况下CPU使用率过高
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LogDebug("Kafka consumer thread stopped");
}

void KafkaConsumer::HandleMessage(RdKafka::Message* message) {
    switch (message->err()) {
        case RdKafka::ERR_NO_ERROR: {
            // 正常消息
            std::string topic = message->topic_name();
            int partition = message->partition();
            int64_t offset = message->offset();
            std::string key;
            std::string payload;
            
            // 提取消息键
            if (message->key()) {
                key = std::string(static_cast<const char*>(message->key_pointer()), message->key_len());
            }
            
            // 提取消息内容
            if (message->payload()) {
                payload = std::string(static_cast<const char*>(message->payload()), message->len());
            }
            
            // 调用回调函数处理消息
            if (callback_) {
                try {
                    bool result = callback_(topic, partition, offset, key, payload);
                    if (!result) {
                        LogError("Message callback returned false for topic: " + topic + 
                                ", partition: " + std::to_string(partition) + 
                                ", offset: " + std::to_string(offset));
                    }
                } catch (const std::exception& e) {
                    LogError("Exception in message callback: " + std::string(e.what()));
                }
            }
            break;
        }
        
        case RdKafka::ERR__PARTITION_EOF:
            // 分区末尾，不是错误
            LogDebug("Reached end of partition: " + std::to_string(message->partition()));
            break;
            
        case RdKafka::ERR__TIMED_OUT:
            // 消费超时，不是错误
            break;
            
        default:
            // 其他错误
            std::lock_guard<std::mutex> lock(mutex_);
            last_error_ = "Consume error: " + RdKafka::err2str(message->err());
            LogError(last_error_);
            break;
    }
}

void KafkaConsumer::LogDebug(const std::string& message) const {
    LOG_DEBUG("KafkaConsumer: {}", message);
}

void KafkaConsumer::LogError(const std::string& message) const {
    LOG_ERROR("KafkaConsumer: {}", message);
}

} // namespace kafka
} // namespace im 