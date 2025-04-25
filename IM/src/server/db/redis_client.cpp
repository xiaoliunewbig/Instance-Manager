#include "include/server/db/redis_client.h"
#include <iostream>
#include <cstdarg>
#include <hiredis/hiredis.h>
#include <thread>

namespace im {
namespace db {

RedisClient::RedisClient(const std::string& host, int port, const std::string& username, const std::string& password)
    : host_(host), port_(port), user_(username), password_(password),
      redis_context_(nullptr), mutex_() {
    // 初始化
}

RedisClient::~RedisClient() {
    Disconnect();
}

bool RedisClient::Connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (IsConnected()) {
        return true;
    }
    
    std::cout << "尝试连接Redis: " << host_ << ":" << port_ << std::endl;
    
    // 创建Redis上下文
    redis_context_ = redisConnect(host_.c_str(), port_);
    if (redis_context_ == nullptr || redis_context_->err) {
        if (redis_context_) {
            std::string error = redis_context_->errstr;
            redisFree(redis_context_);
            redis_context_ = nullptr;
            std::cout << "Redis连接错误: " << error << std::endl;
        } else {
            std::cout << "Redis上下文分配失败" << std::endl;
        }
        return false;
    }
    
    std::cout << "Redis连接成功，准备进行认证" << std::endl;
    
    // 如果需要认证
    if (!password_.empty()) {
        redisReply* reply = nullptr;
        
        if (!user_.empty()) {
            // Redis 6.0+ 支持用户名+密码认证
            reply = (redisReply*)redisCommand(redis_context_, "AUTH %s %s", 
                                           user_.c_str(), password_.c_str());
        } else {
            // 仅密码认证
            reply = (redisReply*)redisCommand(redis_context_, "AUTH %s", password_.c_str());
        }
        
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            std::string error = reply ? reply->str : "认证失败";
            freeReplyObject(reply);
            redisFree(redis_context_);
            redis_context_ = nullptr;
            std::cerr << "Redis认证错误: " << error << std::endl;
            return false;
        }
        
        freeReplyObject(reply);
    }
    
    return true;
}

bool RedisClient::IsConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return redis_context_ != nullptr && !redis_context_->err;
}

bool RedisClient::KeyExists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return false;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "EXISTS %s", key.c_str());
    
    bool exists = false;
    if (reply != nullptr && reply->type == REDIS_REPLY_INTEGER) {
        exists = (reply->integer == 1);
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return exists;
}

bool RedisClient::SetValue(const std::string& key, const std::string& value, int expire_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return false;
    }
    
    redisReply* reply = nullptr;
    
    if (expire_seconds > 0) {
        reply = (redisReply*)redisCommand(redis_context_, "SETEX %s %d %s", 
                                        key.c_str(), expire_seconds, value.c_str());
    } else {
        reply = (redisReply*)redisCommand(redis_context_, "SET %s %s", 
                                        key.c_str(), value.c_str());
    }
    
    bool success = (reply != nullptr && reply->type != REDIS_REPLY_ERROR);
    if (reply) {
        freeReplyObject(reply);
    }
    
    return success;
}

std::string RedisClient::GetValue(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string value;
    
    if (!Connect()) {
        return value;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "GET %s", key.c_str());
    
    if (reply != nullptr && reply->type == REDIS_REPLY_STRING) {
        value.assign(reply->str, reply->len);
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return value;
}

bool RedisClient::DeleteKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return false;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "DEL %s", key.c_str());
    
    bool success = (reply != nullptr && reply->type != REDIS_REPLY_ERROR);
    if (reply) {
        freeReplyObject(reply);
    }
    
    return success;
}

int64_t RedisClient::PushFront(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return -1;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "LPUSH %s %s", 
                                      key.c_str(), value.c_str());
    
    int64_t count = -1;
    if (reply != nullptr && reply->type == REDIS_REPLY_INTEGER) {
        count = reply->integer;
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return count;
}

int64_t RedisClient::PushBack(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return -1;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "RPUSH %s %s", 
                                      key.c_str(), value.c_str());
    
    int64_t count = -1;
    if (reply != nullptr && reply->type == REDIS_REPLY_INTEGER) {
        count = reply->integer;
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return count;
}

bool RedisClient::ListTrim(const std::string& key, int start, int end) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return false;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "LTRIM %s %d %d", 
                                      key.c_str(), start, end);
    
    bool success = (reply != nullptr && reply->type != REDIS_REPLY_ERROR);
    if (reply) {
        freeReplyObject(reply);
    }
    
    return success;
}

int64_t RedisClient::ListLength(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return -1;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "LLEN %s", key.c_str());
    
    int64_t length = -1;
    if (reply != nullptr && reply->type == REDIS_REPLY_INTEGER) {
        length = reply->integer;
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return length;
}

bool RedisClient::Expire(const std::string& key, int seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return false;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "EXPIRE %s %d", 
                                      key.c_str(), seconds);
    
    bool success = (reply != nullptr && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    if (reply) {
        freeReplyObject(reply);
    }
    
    return success;
}

std::vector<std::string> RedisClient::SetMembers(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> members;
    
    if (!Connect()) {
        return members;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "SMEMBERS %s", key.c_str());
    
    if (reply != nullptr && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                members.push_back(reply->element[i]->str);
            }
        }
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return members;
}

bool RedisClient::Publish(const std::string& channel, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return false;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "PUBLISH %s %s", 
                                      channel.c_str(), message.c_str());
    
    bool success = (reply != nullptr && reply->type == REDIS_REPLY_INTEGER);
    if (reply) {
        freeReplyObject(reply);
    }
    
    return success;
}

void RedisClient::Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (redis_context_) {
        redisFree(redis_context_);
        redis_context_ = nullptr;
    }
}

redisReply* RedisClient::ExecuteCommand(const char* format, ...) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return nullptr;
    }
    
    va_list args;
    va_start(args, format);
    redisReply* reply = (redisReply*)redisvCommand(redis_context_, format, args);
    va_end(args);
    
    return reply;
}

void RedisClient::FreeReply(redisReply* reply) {
    if (reply) {
        freeReplyObject(reply);
    }
}

int RedisClient::GetTTL(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return -2;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "TTL %s", key.c_str());
    
    int ttl = -2;
    if (reply != nullptr && reply->type == REDIS_REPLY_INTEGER) {
        ttl = static_cast<int>(reply->integer);
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return ttl;
}

int64_t RedisClient::Increment(const std::string& key, int64_t increment) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return -1;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "INCRBY %s %lld",
                                               key.c_str(), increment);
    
    int64_t value = -1;
    if (reply != nullptr && reply->type == REDIS_REPLY_INTEGER) {
        value = reply->integer;
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return value;
}

int64_t RedisClient::Decrement(const std::string& key, int64_t decrement) {
    return Increment(key, -decrement);
}

std::vector<std::string> RedisClient::GetList(const std::string& key, int start, int end) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> values;
    
    if (!Connect()) {
        return values;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "LRANGE %s %d %d",
                                               key.c_str(), start, end);
    
    if (reply != nullptr && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                values.push_back(reply->element[i]->str);
            }
        }
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return values;
}

bool RedisClient::SetHashField(const std::string& key, const std::string& field, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return false;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "HSET %s %s %s",
                                               key.c_str(), field.c_str(), value.c_str());
    
    bool success = (reply != nullptr && reply->type != REDIS_REPLY_ERROR);
    if (reply) {
        freeReplyObject(reply);
    }
    
    return success;
}

std::string RedisClient::GetHashField(const std::string& key, const std::string& field) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string value;
    
    if (!Connect()) {
        return value;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "HGET %s %s",
                                               key.c_str(), field.c_str());
    
    if (reply != nullptr && reply->type == REDIS_REPLY_STRING) {
        value.assign(reply->str, reply->len);
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return value;
}

bool RedisClient::DeleteHashField(const std::string& key, const std::string& field) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return false;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "HDEL %s %s",
                                               key.c_str(), field.c_str());
    
    bool success = (reply != nullptr && reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    if (reply) {
        freeReplyObject(reply);
    }
    
    return success;
}

bool RedisClient::SetHashValues(const std::string& key, const std::map<std::string, std::string>& fields, int expire_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect() || fields.empty()) {
        return false;
    }
    
    // 使用管道来提高效率
    for (const auto& pair : fields) {
        redisAppendCommand(redis_context_, "HSET %s %s %s",
                        key.c_str(), pair.first.c_str(), pair.second.c_str());
    }
    
    // 获取每个命令的回复
    bool success = true;
    redisReply* reply = nullptr;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (redisGetReply(redis_context_, (void**)&reply) != REDIS_OK || 
            reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            success = false;
        }
        
        if (reply) {
            freeReplyObject(reply);
        }
        
        if (!success) {
            break;
        }
    }
    
    // 设置过期时间
    if (success && expire_seconds > 0) {
        success = Expire(key, expire_seconds);
    }
    
    return success;
}

std::map<std::string, std::string> RedisClient::GetHashAll(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, std::string> values;
    
    if (!Connect()) {
        return values;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "HGETALL %s", key.c_str());
    
    if (reply != nullptr && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            if (i + 1 < reply->elements && 
                reply->element[i]->type == REDIS_REPLY_STRING && 
                reply->element[i + 1]->type == REDIS_REPLY_STRING) {
                values[reply->element[i]->str] = reply->element[i + 1]->str;
            }
        }
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return values;
}

int64_t RedisClient::SetAdd(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return 0;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "SADD %s %s",
                                               key.c_str(), value.c_str());
    
    int64_t added = 0;
    if (reply != nullptr && reply->type == REDIS_REPLY_INTEGER) {
        added = reply->integer;
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return added;
}

int64_t RedisClient::SetRemove(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!Connect()) {
        return 0;
    }
    
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "SREM %s %s",
                                               key.c_str(), value.c_str());
    
    int64_t removed = 0;
    if (reply != nullptr && reply->type == REDIS_REPLY_INTEGER) {
        removed = reply->integer;
    }
    
    if (reply) {
        freeReplyObject(reply);
    }
    
    return removed;
}

void RedisClient::ConnectAsync() {
    // 使用值捕获this指针
    std::thread([self = this]() {
        self->Connect();
    }).detach();
}

} // namespace db
} // namespace im