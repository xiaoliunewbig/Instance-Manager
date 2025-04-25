#include "include/server/kafka/kafka_producer.h"
#include "include/server/utils/logger.h"
#include "/usr/include/librdkafka/rdkafkacpp.h"
#include <iostream>

namespace im {
namespace kafka {

// 消息交付回调函数
void KafkaProducer::dr_cb(RdKafka::Message& message) {
    if (callback_) {
        std::string topic = message.topic_name();
        std::string payload;
        if (message.payload()) {
            payload = std::string(static_cast<const char*>(message.payload()), message.len());
        }
        
        callback_(topic, payload, message.err() == RdKafka::ERR_NO_ERROR);
    }
}

KafkaProducer::KafkaProducer(
    const std::string& brokers,
    const std::string& client_id,
    DeliveryCallback callback)
    : brokers_(brokers), client_id_(client_id), callback_(callback), initialized_(false) {
}

KafkaProducer::~KafkaProducer() {
    if (producer_) {
        Flush();
        producer_.reset();
    }
    
    topics_.clear();
    conf_.reset();
}

bool KafkaProducer::Initialize() {
    std::string errstr;
    
    // 创建配置对象
    conf_ = std::unique_ptr<RdKafka::Conf>(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (!conf_) {
        last_error_ = "Failed to create Kafka configuration";
        LOG_ERROR(last_error_);
        return false;
    }
    
    // 设置broker列表
    if (conf_->set("bootstrap.servers", brokers_, errstr) != RdKafka::Conf::CONF_OK) {
        last_error_ = "Failed to set Kafka brokers: " + errstr;
        LOG_ERROR(last_error_);
        return false;
    }
    
    // 设置客户端ID
    if (!client_id_.empty()) {
        if (conf_->set("client.id", client_id_, errstr) != RdKafka::Conf::CONF_OK) {
            last_error_ = "Failed to set client ID: " + errstr;
            LOG_ERROR(last_error_);
            return false;
        }
    }
    
    // 设置交付报告回调函数
    if (conf_->set("dr_cb", this, errstr) != RdKafka::Conf::CONF_OK) {
        last_error_ = "Failed to set delivery report callback: " + errstr;
        LOG_ERROR(last_error_);
        return false;
    }
    
    // 创建生产者
    producer_ = std::unique_ptr<RdKafka::Producer>(RdKafka::Producer::create(conf_.get(), errstr));
    if (!producer_) {
        last_error_ = "Failed to create Kafka producer: " + errstr;
        LOG_ERROR(last_error_);
        return false;
    }
    
    initialized_ = true;
    LOG_INFO("Kafka producer initialized successfully: brokers={}, client_id={}", 
        brokers_, client_id_);
    return true;
}

bool KafkaProducer::SendMessage(
    const std::string& topic,
    const std::string& payload,
    const std::string& key) {
    
    if (!initialized_ || !producer_) {
        last_error_ = "Kafka producer not initialized";
        LOG_ERROR(last_error_);
        return false;
    }
    
    RdKafka::Topic* topic_ptr = GetTopic(topic);
    if (!topic_ptr) {
        return false;
    }
    
    // 发送消息
    RdKafka::ErrorCode err = producer_->produce(
        topic_ptr,
        RdKafka::Topic::PARTITION_UA,
        RdKafka::Producer::RK_MSG_COPY,
        const_cast<char*>(payload.c_str()),
        payload.size(),
        key.empty() ? nullptr : key.c_str(),
        key.empty() ? 0 : key.size(),
        nullptr  // 使用nullptr代替cb_copy
    );
    
    if (err != RdKafka::ERR_NO_ERROR) {
        last_error_ = "Failed to produce message: " + RdKafka::err2str(err);
        LOG_ERROR(last_error_ + ", topic: {}", topic);
        return false;
    }
    
    // 轮询事件
    producer_->poll(0);
    
    LOG_DEBUG("Message sent to topic {}, payload size: {}", topic, payload.size());
    return true;
}

bool KafkaProducer::Flush(int timeout_ms) {
    if (!initialized_ || !producer_) {
        last_error_ = "Kafka producer not initialized";
        LOG_ERROR(last_error_);
        return false;
    }
    
    LOG_DEBUG("Flushing Kafka producer...");
    RdKafka::ErrorCode err = producer_->flush(timeout_ms);
    if (err != RdKafka::ERR_NO_ERROR) {
        last_error_ = "Failed to flush messages: " + RdKafka::err2str(err);
        LOG_ERROR(last_error_);
        return false;
    }
    
    return true;
}

std::string KafkaProducer::GetLastError() const {
    return last_error_;
}

RdKafka::Topic* KafkaProducer::GetTopic(const std::string& topic) {
    if (!initialized_ || !producer_) {
        last_error_ = "Kafka producer not initialized";
        LOG_ERROR(last_error_);
        return nullptr;
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = topics_.find(topic);
        if (it != topics_.end()) {
            return it->second.get();
        }
    }
    
    // 创建主题配置
    std::string errstr;
    RdKafka::Conf* tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);
    if (!tconf) {
        last_error_ = "Failed to create topic configuration";
        LOG_ERROR(last_error_);
        return nullptr;
    }
    
    // 创建主题
    std::unique_ptr<RdKafka::Topic> topic_ptr(
        RdKafka::Topic::create(producer_.get(), topic, tconf, errstr));
    delete tconf;
    
    if (!topic_ptr) {
        last_error_ = "Failed to create topic " + topic + ": " + errstr;
        LOG_ERROR(last_error_);
        return nullptr;
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        RdKafka::Topic* result = topic_ptr.get();
        topics_[topic] = std::move(topic_ptr);
        return result;
    }
}

void KafkaProducer::LogDebug(const std::string& message) const {
    LOG_DEBUG("{}", message);
}

void KafkaProducer::LogError(const std::string& message) const {
    LOG_ERROR("{}", message);
}

} // namespace kafka
} // namespace im 