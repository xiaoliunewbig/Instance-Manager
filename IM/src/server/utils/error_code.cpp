#include "server/utils/error_code.h"

namespace im {
namespace utils {

std::unordered_map<int, std::string> ErrorCode::error_messages_ = {
    {SUCCESS, "成功"},

    // 通用错误码 1000-1999
    {UNKNOWN_ERROR, "未知错误"},
    {INVALID_PARAMS, "参数无效"},
    {INTERNAL_ERROR, "内部错误"},
    {TIMEOUT, "超时"},
    {NOT_FOUND, "未找到"},
    {ALREADY_EXISTS, "已存在"},
    {PERMISSION_DENIED, "权限不足"},
    {RATE_LIMIT_EXCEEDED, "请求频率超限"},
    {NOT_IMPLEMENTED, "未实现"},
    {SERVICE_UNAVAILABLE, "服务不可用"},
    {CONFIG_ERROR, "配置错误"},
    {JSON_PARSE_ERROR, "JSON解析错误"},

    // 用户相关错误码 2000-2999
    {USER_NOT_FOUND, "用户不存在"},
    {USER_ALREADY_EXISTS, "用户已存在"},
    {USER_AUTHENTICATION_FAILED, "用户认证失败"},
    {USER_TOKEN_EXPIRED, "用户令牌过期"},
    {USER_TOKEN_INVALID, "用户令牌无效"},
    {USER_ACCOUNT_LOCKED, "用户账号已锁定"},
    {USER_PASSWORD_WEAK, "用户密码强度不够"},
    {USER_PASSWORD_WRONG, "用户密码错误"},
    {USER_VERIFICATION_FAILED, "用户验证失败"},
    {USER_VERIFICATION_EXPIRED, "用户验证码过期"},
    {USER_STATUS_ABNORMAL, "用户状态异常"},
    {USER_REGISTRATION_FAILED, "用户注册失败"},
    {USER_UPDATE_FAILED, "用户信息更新失败"},

    // 好友相关错误码 3000-3999
    {FRIEND_NOT_FOUND, "好友不存在"},
    {FRIEND_ALREADY_EXISTS, "好友已存在"},
    {FRIEND_REQUEST_NOT_FOUND, "好友请求不存在"},
    {FRIEND_REQUEST_ALREADY_ACCEPTED, "好友请求已接受"},
    {FRIEND_REQUEST_ALREADY_REJECTED, "好友请求已拒绝"},
    {FRIEND_REQUEST_ALREADY_EXISTS, "好友请求已存在"},
    {FRIEND_REQUEST_CANNOT_SELF, "不能添加自己为好友"},
    {FRIEND_DELETE_FAILED, "删除好友失败"},
    {FRIEND_ADD_FAILED, "添加好友失败"},
    {FRIEND_UPDATE_FAILED, "更新好友关系失败"},
    {FRIEND_BLOCKED, "好友已被拉黑"},

    // 群组相关错误码 4000-4999
    {GROUP_NOT_FOUND, "群组不存在"},
    {GROUP_ALREADY_EXISTS, "群组已存在"},
    {GROUP_MEMBER_NOT_FOUND, "群成员不存在"},
    {GROUP_MEMBER_ALREADY_EXISTS, "群成员已存在"},
    {GROUP_MEMBER_PERMISSION_DENIED, "群成员权限不足"},
    {GROUP_MEMBER_LIMIT_EXCEEDED, "群成员数量超限"},
    {GROUP_CREATE_FAILED, "创建群组失败"},
    {GROUP_UPDATE_FAILED, "更新群组信息失败"},
    {GROUP_DELETE_FAILED, "删除群组失败"},
    {GROUP_JOIN_FAILED, "加入群组失败"},
    {GROUP_LEAVE_FAILED, "退出群组失败"},
    {GROUP_KICK_FAILED, "踢出群成员失败"},
    {GROUP_IS_FULL, "群已满"},
    {GROUP_IS_DISBANDED, "群已解散"},

    // 消息相关错误码 5000-5999
    {MESSAGE_NOT_FOUND, "消息不存在"},
    {MESSAGE_SEND_FAILED, "消息发送失败"},
    {MESSAGE_RECALL_TIMEOUT, "消息撤回超时"},
    {MESSAGE_RECALL_FAILED, "消息撤回失败"},
    {MESSAGE_CONTENT_INVALID, "消息内容无效"},
    {MESSAGE_TOO_LONG, "消息内容过长"},
    {MESSAGE_TYPE_INVALID, "消息类型无效"},
    {MESSAGE_READ_FAILED, "消息已读标记失败"},
    {MESSAGE_DELETE_FAILED, "消息删除失败"},
    {MESSAGE_RECEIVER_NOT_FOUND, "消息接收者不存在"},
    {MESSAGE_BLOCKED, "消息被屏蔽"},

    // 文件相关错误码 6000-6999
    {FILE_NOT_FOUND, "文件不存在"},
    {FILE_UPLOAD_FAILED, "文件上传失败"},
    {FILE_DOWNLOAD_FAILED, "文件下载失败"},
    {FILE_SIZE_EXCEEDED, "文件大小超限"},
    {FILE_TYPE_NOT_ALLOWED, "文件类型不允许"},
    {FILE_ALREADY_EXISTS, "文件已存在"},
    {FILE_DAMAGED, "文件损坏"},
    {FILE_UPLOAD_INCOMPLETE, "文件上传不完整"},
    {FILE_DELETE_FAILED, "文件删除失败"},
    {FILE_READ_FAILED, "文件读取失败"},
    {FILE_WRITE_FAILED, "文件写入失败"},

    // 数据库相关错误码 7000-7999
    {DB_CONNECTION_FAILED, "数据库连接失败"},
    {DB_QUERY_FAILED, "数据库查询失败"},
    {DB_INSERT_FAILED, "数据库插入失败"},
    {DB_UPDATE_FAILED, "数据库更新失败"},
    {DB_DELETE_FAILED, "数据库删除失败"},
    {DB_TRANSACTION_FAILED, "数据库事务失败"},
    {DB_DUPLICATE_ENTRY, "数据库记录重复"},
    {DB_RESULT_EMPTY, "数据库结果为空"},
    {DB_RESULT_TOO_LARGE, "数据库结果过大"},
    {DB_POOL_EXHAUSTED, "数据库连接池耗尽"},
    {DB_TIMEOUT, "数据库操作超时"},

    // 缓存相关错误码 8000-8999
    {CACHE_CONNECTION_FAILED, "缓存连接失败"},
    {CACHE_OPERATION_FAILED, "缓存操作失败"},
    {CACHE_KEY_NOT_FOUND, "缓存键不存在"},
    {CACHE_SET_FAILED, "缓存设置失败"},
    {CACHE_GET_FAILED, "缓存获取失败"},
    {CACHE_DELETE_FAILED, "缓存删除失败"},
    {CACHE_EXPIRE_FAILED, "缓存过期设置失败"},
    {CACHE_INCR_FAILED, "缓存增加操作失败"},
    {CACHE_DECR_FAILED, "缓存减少操作失败"},
    {CACHE_NAMESPACE_INVALID, "缓存命名空间无效"},
    {CACHE_POOL_EXHAUSTED, "缓存连接池耗尽"},

    // Kafka相关错误码 9000-9999
    {KAFKA_CONNECTION_FAILED, "Kafka连接失败"},
    {KAFKA_PRODUCER_FAILED, "Kafka生产者创建失败"},
    {KAFKA_CONSUMER_FAILED, "Kafka消费者创建失败"},
    {KAFKA_MESSAGE_SEND_FAILED, "Kafka消息发送失败"},
    {KAFKA_MESSAGE_CONSUME_FAILED, "Kafka消息消费失败"},
    {KAFKA_TOPIC_NOT_FOUND, "Kafka主题不存在"},
    {KAFKA_TOPIC_CREATE_FAILED, "Kafka主题创建失败"},
    {KAFKA_PARTITION_INVALID, "Kafka分区无效"},
    {KAFKA_OFFSET_INVALID, "Kafka偏移量无效"},
    {KAFKA_GROUP_ID_INVALID, "Kafka消费组ID无效"},
    {KAFKA_BROKER_UNAVAILABLE, "Kafka代理不可用"},

    // 网络相关错误码 10000-10999
    {NETWORK_CONNECTION_FAILED, "网络连接失败"},
    {NETWORK_TIMEOUT, "网络超时"},
    {NETWORK_DISCONNECTED, "网络断开"},
    {NETWORK_RECONNECT_FAILED, "网络重连失败"},
    {NETWORK_REQUEST_FAILED, "网络请求失败"},
    {NETWORK_RESPONSE_INVALID, "网络响应无效"},
    {NETWORK_PROTOCOL_ERROR, "网络协议错误"},
    {NETWORK_DNS_RESOLVE_FAILED, "DNS解析失败"},
    {NETWORK_SSL_ERROR, "SSL证书错误"},
    {NETWORK_HOST_UNREACHABLE, "主机不可达"},
    {NETWORK_ADDRESS_INVALID, "网络地址无效"},

    // gRPC相关错误码 11000-11999
    {GRPC_CONNECTION_FAILED, "gRPC连接失败"},
    {GRPC_REQUEST_FAILED, "gRPC请求失败"},
    {GRPC_RESPONSE_INVALID, "gRPC响应无效"},
    {GRPC_TIMEOUT, "gRPC超时"},
    {GRPC_CANCELLED, "gRPC已取消"},
    {GRPC_UNKNOWN, "gRPC未知错误"},
    {GRPC_INVALID_ARGUMENT, "gRPC参数无效"},
    {GRPC_DEADLINE_EXCEEDED, "gRPC截止日期已过"},
    {GRPC_NOT_FOUND, "gRPC资源未找到"},
    {GRPC_ALREADY_EXISTS, "gRPC资源已存在"},
    {GRPC_PERMISSION_DENIED, "gRPC权限不足"},

    // WebSocket相关错误码 12000-12999
    {WS_CONNECTION_FAILED, "WebSocket连接失败"},
    {WS_DISCONNECTED, "WebSocket断开连接"},
    {WS_MESSAGE_SEND_FAILED, "WebSocket消息发送失败"},
    {WS_HANDSHAKE_FAILED, "WebSocket握手失败"},
    {WS_AUTH_FAILED, "WebSocket认证失败"},
    {WS_PING_TIMEOUT, "WebSocket ping超时"},
    {WS_PONG_TIMEOUT, "WebSocket pong超时"},
    {WS_MESSAGE_SIZE_EXCEEDED, "WebSocket消息大小超限"},
    {WS_MESSAGE_TYPE_INVALID, "WebSocket消息类型无效"},
    {WS_RECONNECT_FAILED, "WebSocket重连失败"},
    {WS_CLOSE_ABNORMAL, "WebSocket异常关闭"},

    // 安全相关错误码 13000-13999
    {SECURITY_TOKEN_INVALID, "安全令牌无效"},
    {SECURITY_TOKEN_EXPIRED, "安全令牌过期"},
    {SECURITY_SIGNATURE_INVALID, "签名无效"},
    {SECURITY_ENCRYPTION_FAILED, "加密失败"},
    {SECURITY_DECRYPTION_FAILED, "解密失败"},
    {SECURITY_HASH_FAILED, "哈希计算失败"},
    {SECURITY_CSRF_TOKEN_INVALID, "CSRF令牌无效"},
    {SECURITY_IP_BLOCKED, "IP已被封锁"},
    {SECURITY_TOO_MANY_REQUESTS, "请求过于频繁"},
    {SECURITY_PARAMETERS_TAMPERED, "参数被篡改"},
    {SECURITY_UNAUTHORIZED, "未授权访问"}
};

std::unordered_map<int, std::string> ErrorCode::error_types_ = {
    {SUCCESS, "SUCCESS"},

    // 通用错误码类型
    {UNKNOWN_ERROR, "GENERAL"},
    {INVALID_PARAMS, "GENERAL"},
    {INTERNAL_ERROR, "GENERAL"},
    {TIMEOUT, "GENERAL"},
    {NOT_FOUND, "GENERAL"},
    {ALREADY_EXISTS, "GENERAL"},
    {PERMISSION_DENIED, "GENERAL"},
    {RATE_LIMIT_EXCEEDED, "GENERAL"},
    {NOT_IMPLEMENTED, "GENERAL"},
    {SERVICE_UNAVAILABLE, "GENERAL"},
    {CONFIG_ERROR, "GENERAL"},
    {JSON_PARSE_ERROR, "GENERAL"},

    // 用户相关错误码类型
    {USER_NOT_FOUND, "USER"},
    {USER_ALREADY_EXISTS, "USER"},
    {USER_AUTHENTICATION_FAILED, "USER"},
    {USER_TOKEN_EXPIRED, "USER"},
    {USER_TOKEN_INVALID, "USER"},
    {USER_ACCOUNT_LOCKED, "USER"},
    {USER_PASSWORD_WEAK, "USER"},
    {USER_PASSWORD_WRONG, "USER"},
    {USER_VERIFICATION_FAILED, "USER"},
    {USER_VERIFICATION_EXPIRED, "USER"},
    {USER_STATUS_ABNORMAL, "USER"},
    {USER_REGISTRATION_FAILED, "USER"},
    {USER_UPDATE_FAILED, "USER"},

    // 好友相关错误码类型
    {FRIEND_NOT_FOUND, "FRIEND"},
    {FRIEND_ALREADY_EXISTS, "FRIEND"},
    {FRIEND_REQUEST_NOT_FOUND, "FRIEND"},
    {FRIEND_REQUEST_ALREADY_ACCEPTED, "FRIEND"},
    {FRIEND_REQUEST_ALREADY_REJECTED, "FRIEND"},
    {FRIEND_REQUEST_ALREADY_EXISTS, "FRIEND"},
    {FRIEND_REQUEST_CANNOT_SELF, "FRIEND"},
    {FRIEND_DELETE_FAILED, "FRIEND"},
    {FRIEND_ADD_FAILED, "FRIEND"},
    {FRIEND_UPDATE_FAILED, "FRIEND"},
    {FRIEND_BLOCKED, "FRIEND"},

    // 群组相关错误码类型
    {GROUP_NOT_FOUND, "GROUP"},
    {GROUP_ALREADY_EXISTS, "GROUP"},
    {GROUP_MEMBER_NOT_FOUND, "GROUP"},
    {GROUP_MEMBER_ALREADY_EXISTS, "GROUP"},
    {GROUP_MEMBER_PERMISSION_DENIED, "GROUP"},
    {GROUP_MEMBER_LIMIT_EXCEEDED, "GROUP"},
    {GROUP_CREATE_FAILED, "GROUP"},
    {GROUP_UPDATE_FAILED, "GROUP"},
    {GROUP_DELETE_FAILED, "GROUP"},
    {GROUP_JOIN_FAILED, "GROUP"},
    {GROUP_LEAVE_FAILED, "GROUP"},
    {GROUP_KICK_FAILED, "GROUP"},
    {GROUP_IS_FULL, "GROUP"},
    {GROUP_IS_DISBANDED, "GROUP"},

    // 消息相关错误码类型
    {MESSAGE_NOT_FOUND, "MESSAGE"},
    {MESSAGE_SEND_FAILED, "MESSAGE"},
    {MESSAGE_RECALL_TIMEOUT, "MESSAGE"},
    {MESSAGE_RECALL_FAILED, "MESSAGE"},
    {MESSAGE_CONTENT_INVALID, "MESSAGE"},
    {MESSAGE_TOO_LONG, "MESSAGE"},
    {MESSAGE_TYPE_INVALID, "MESSAGE"},
    {MESSAGE_READ_FAILED, "MESSAGE"},
    {MESSAGE_DELETE_FAILED, "MESSAGE"},
    {MESSAGE_RECEIVER_NOT_FOUND, "MESSAGE"},
    {MESSAGE_BLOCKED, "MESSAGE"},

    // 文件相关错误码类型
    {FILE_NOT_FOUND, "FILE"},
    {FILE_UPLOAD_FAILED, "FILE"},
    {FILE_DOWNLOAD_FAILED, "FILE"},
    {FILE_SIZE_EXCEEDED, "FILE"},
    {FILE_TYPE_NOT_ALLOWED, "FILE"},
    {FILE_ALREADY_EXISTS, "FILE"},
    {FILE_DAMAGED, "FILE"},
    {FILE_UPLOAD_INCOMPLETE, "FILE"},
    {FILE_DELETE_FAILED, "FILE"},
    {FILE_READ_FAILED, "FILE"},
    {FILE_WRITE_FAILED, "FILE"},

    // 数据库相关错误码类型
    {DB_CONNECTION_FAILED, "DATABASE"},
    {DB_QUERY_FAILED, "DATABASE"},
    {DB_INSERT_FAILED, "DATABASE"},
    {DB_UPDATE_FAILED, "DATABASE"},
    {DB_DELETE_FAILED, "DATABASE"},
    {DB_TRANSACTION_FAILED, "DATABASE"},
    {DB_DUPLICATE_ENTRY, "DATABASE"},
    {DB_RESULT_EMPTY, "DATABASE"},
    {DB_RESULT_TOO_LARGE, "DATABASE"},
    {DB_POOL_EXHAUSTED, "DATABASE"},
    {DB_TIMEOUT, "DATABASE"},

    // 缓存相关错误码类型
    {CACHE_CONNECTION_FAILED, "CACHE"},
    {CACHE_OPERATION_FAILED, "CACHE"},
    {CACHE_KEY_NOT_FOUND, "CACHE"},
    {CACHE_SET_FAILED, "CACHE"},
    {CACHE_GET_FAILED, "CACHE"},
    {CACHE_DELETE_FAILED, "CACHE"},
    {CACHE_EXPIRE_FAILED, "CACHE"},
    {CACHE_INCR_FAILED, "CACHE"},
    {CACHE_DECR_FAILED, "CACHE"},
    {CACHE_NAMESPACE_INVALID, "CACHE"},
    {CACHE_POOL_EXHAUSTED, "CACHE"},

    // Kafka相关错误码类型
    {KAFKA_CONNECTION_FAILED, "KAFKA"},
    {KAFKA_PRODUCER_FAILED, "KAFKA"},
    {KAFKA_CONSUMER_FAILED, "KAFKA"},
    {KAFKA_MESSAGE_SEND_FAILED, "KAFKA"},
    {KAFKA_MESSAGE_CONSUME_FAILED, "KAFKA"},
    {KAFKA_TOPIC_NOT_FOUND, "KAFKA"},
    {KAFKA_TOPIC_CREATE_FAILED, "KAFKA"},
    {KAFKA_PARTITION_INVALID, "KAFKA"},
    {KAFKA_OFFSET_INVALID, "KAFKA"},
    {KAFKA_GROUP_ID_INVALID, "KAFKA"},
    {KAFKA_BROKER_UNAVAILABLE, "KAFKA"},

    // 网络相关错误码类型
    {NETWORK_CONNECTION_FAILED, "NETWORK"},
    {NETWORK_TIMEOUT, "NETWORK"},
    {NETWORK_DISCONNECTED, "NETWORK"},
    {NETWORK_RECONNECT_FAILED, "NETWORK"},
    {NETWORK_REQUEST_FAILED, "NETWORK"},
    {NETWORK_RESPONSE_INVALID, "NETWORK"},
    {NETWORK_PROTOCOL_ERROR, "NETWORK"},
    {NETWORK_DNS_RESOLVE_FAILED, "NETWORK"},
    {NETWORK_SSL_ERROR, "NETWORK"},
    {NETWORK_HOST_UNREACHABLE, "NETWORK"},
    {NETWORK_ADDRESS_INVALID, "NETWORK"},

    // gRPC相关错误码类型
    {GRPC_CONNECTION_FAILED, "GRPC"},
    {GRPC_REQUEST_FAILED, "GRPC"},
    {GRPC_RESPONSE_INVALID, "GRPC"},
    {GRPC_TIMEOUT, "GRPC"},
    {GRPC_CANCELLED, "GRPC"},
    {GRPC_UNKNOWN, "GRPC"},
    {GRPC_INVALID_ARGUMENT, "GRPC"},
    {GRPC_DEADLINE_EXCEEDED, "GRPC"},
    {GRPC_NOT_FOUND, "GRPC"},
    {GRPC_ALREADY_EXISTS, "GRPC"},
    {GRPC_PERMISSION_DENIED, "GRPC"},

    // WebSocket相关错误码类型
    {WS_CONNECTION_FAILED, "WEBSOCKET"},
    {WS_DISCONNECTED, "WEBSOCKET"},
    {WS_MESSAGE_SEND_FAILED, "WEBSOCKET"},
    {WS_HANDSHAKE_FAILED, "WEBSOCKET"},
    {WS_AUTH_FAILED, "WEBSOCKET"},
    {WS_PING_TIMEOUT, "WEBSOCKET"},
    {WS_PONG_TIMEOUT, "WEBSOCKET"},
    {WS_MESSAGE_SIZE_EXCEEDED, "WEBSOCKET"},
    {WS_MESSAGE_TYPE_INVALID, "WEBSOCKET"},
    {WS_RECONNECT_FAILED, "WEBSOCKET"},
    {WS_CLOSE_ABNORMAL, "WEBSOCKET"},

    // 安全相关错误码类型
    {SECURITY_TOKEN_INVALID, "SECURITY"},
    {SECURITY_TOKEN_EXPIRED, "SECURITY"},
    {SECURITY_SIGNATURE_INVALID, "SECURITY"},
    {SECURITY_ENCRYPTION_FAILED, "SECURITY"},
    {SECURITY_DECRYPTION_FAILED, "SECURITY"},
    {SECURITY_HASH_FAILED, "SECURITY"},
    {SECURITY_CSRF_TOKEN_INVALID, "SECURITY"},
    {SECURITY_IP_BLOCKED, "SECURITY"},
    {SECURITY_TOO_MANY_REQUESTS, "SECURITY"},
    {SECURITY_PARAMETERS_TAMPERED, "SECURITY"},
    {SECURITY_UNAUTHORIZED, "SECURITY"}
};

std::string ErrorCode::GetMessage(int code) {
    auto it = error_messages_.find(code);
    if (it != error_messages_.end()) {
        return it->second;
    }
    return "未知错误码: " + std::to_string(code);
}

std::string ErrorCode::GetMessage(Code code) {
    return GetMessage(static_cast<int>(code));
}

std::string ErrorCode::GetType(int code) {
    auto it = error_types_.find(code);
    if (it != error_types_.end()) {
        return it->second;
    }
    return "UNKNOWN";
}

std::string ErrorCode::GetType(Code code) {
    return GetType(static_cast<int>(code));
}

bool ErrorCode::IsSuccess(int code) {
    return code == SUCCESS;
}

bool ErrorCode::IsSuccess(Code code) {
    return code == SUCCESS;
}

} // namespace utils
} // namespace im 