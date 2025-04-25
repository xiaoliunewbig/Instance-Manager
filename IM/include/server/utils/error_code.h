#ifndef IM_ERROR_CODE_H
#define IM_ERROR_CODE_H

#include <string>
#include <unordered_map>

namespace im {
namespace utils {

/**
 * @brief 错误码定义类
 */
class ErrorCode {
public:
    /**
     * @brief 错误码枚举
     */
    enum Code {
        // 成功
        SUCCESS = 0,

        // 通用错误码 1000-1999
        UNKNOWN_ERROR = 1000,
        INVALID_PARAMS = 1001,
        INTERNAL_ERROR = 1002,
        TIMEOUT = 1003,
        NOT_FOUND = 1004,
        ALREADY_EXISTS = 1005,
        PERMISSION_DENIED = 1006,
        RATE_LIMIT_EXCEEDED = 1007,
        NOT_IMPLEMENTED = 1008,
        SERVICE_UNAVAILABLE = 1009,
        CONFIG_ERROR = 1010,
        JSON_PARSE_ERROR = 1011,

        // 用户相关错误码 2000-2999
        USER_NOT_FOUND = 2000,
        USER_ALREADY_EXISTS = 2001,
        USER_AUTHENTICATION_FAILED = 2002,
        USER_TOKEN_EXPIRED = 2003,
        USER_TOKEN_INVALID = 2004,
        USER_ACCOUNT_LOCKED = 2005,
        USER_PASSWORD_WEAK = 2006,
        USER_PASSWORD_WRONG = 2007,
        USER_VERIFICATION_FAILED = 2008,
        USER_VERIFICATION_EXPIRED = 2009,
        USER_STATUS_ABNORMAL = 2010,
        USER_REGISTRATION_FAILED = 2011,
        USER_UPDATE_FAILED = 2012,

        // 好友相关错误码 3000-3999
        FRIEND_NOT_FOUND = 3000,
        FRIEND_ALREADY_EXISTS = 3001,
        FRIEND_REQUEST_NOT_FOUND = 3002,
        FRIEND_REQUEST_ALREADY_ACCEPTED = 3003,
        FRIEND_REQUEST_ALREADY_REJECTED = 3004,
        FRIEND_REQUEST_ALREADY_EXISTS = 3005,
        FRIEND_REQUEST_CANNOT_SELF = 3006,
        FRIEND_DELETE_FAILED = 3007,
        FRIEND_ADD_FAILED = 3008,
        FRIEND_UPDATE_FAILED = 3009,
        FRIEND_BLOCKED = 3010,

        // 群组相关错误码 4000-4999
        GROUP_NOT_FOUND = 4000,
        GROUP_ALREADY_EXISTS = 4001,
        GROUP_MEMBER_NOT_FOUND = 4002,
        GROUP_MEMBER_ALREADY_EXISTS = 4003,
        GROUP_MEMBER_PERMISSION_DENIED = 4004,
        GROUP_MEMBER_LIMIT_EXCEEDED = 4005,
        GROUP_CREATE_FAILED = 4006,
        GROUP_UPDATE_FAILED = 4007,
        GROUP_DELETE_FAILED = 4008,
        GROUP_JOIN_FAILED = 4009,
        GROUP_LEAVE_FAILED = 4010,
        GROUP_KICK_FAILED = 4011,
        GROUP_IS_FULL = 4012,
        GROUP_IS_DISBANDED = 4013,

        // 消息相关错误码 5000-5999
        MESSAGE_NOT_FOUND = 5000,
        MESSAGE_SEND_FAILED = 5001,
        MESSAGE_RECALL_TIMEOUT = 5002,
        MESSAGE_RECALL_FAILED = 5003,
        MESSAGE_CONTENT_INVALID = 5004,
        MESSAGE_TOO_LONG = 5005,
        MESSAGE_TYPE_INVALID = 5006,
        MESSAGE_READ_FAILED = 5007,
        MESSAGE_DELETE_FAILED = 5008,
        MESSAGE_RECEIVER_NOT_FOUND = 5009,
        MESSAGE_BLOCKED = 5010,

        // 文件相关错误码 6000-6999
        FILE_NOT_FOUND = 6000,
        FILE_UPLOAD_FAILED = 6001,
        FILE_DOWNLOAD_FAILED = 6002,
        FILE_SIZE_EXCEEDED = 6003,
        FILE_TYPE_NOT_ALLOWED = 6004,
        FILE_ALREADY_EXISTS = 6005,
        FILE_DAMAGED = 6006,
        FILE_UPLOAD_INCOMPLETE = 6007,
        FILE_DELETE_FAILED = 6008,
        FILE_READ_FAILED = 6009,
        FILE_WRITE_FAILED = 6010,

        // 数据库相关错误码 7000-7999
        DB_CONNECTION_FAILED = 7000,
        DB_QUERY_FAILED = 7001,
        DB_INSERT_FAILED = 7002,
        DB_UPDATE_FAILED = 7003,
        DB_DELETE_FAILED = 7004,
        DB_TRANSACTION_FAILED = 7005,
        DB_DUPLICATE_ENTRY = 7006,
        DB_RESULT_EMPTY = 7007,
        DB_RESULT_TOO_LARGE = 7008,
        DB_POOL_EXHAUSTED = 7009,
        DB_TIMEOUT = 7010,

        // 缓存相关错误码 8000-8999
        CACHE_CONNECTION_FAILED = 8000,
        CACHE_OPERATION_FAILED = 8001,
        CACHE_KEY_NOT_FOUND = 8002,
        CACHE_SET_FAILED = 8003,
        CACHE_GET_FAILED = 8004,
        CACHE_DELETE_FAILED = 8005,
        CACHE_EXPIRE_FAILED = 8006,
        CACHE_INCR_FAILED = 8007,
        CACHE_DECR_FAILED = 8008,
        CACHE_NAMESPACE_INVALID = 8009,
        CACHE_POOL_EXHAUSTED = 8010,

        // Kafka相关错误码 9000-9999
        KAFKA_CONNECTION_FAILED = 9000,
        KAFKA_PRODUCER_FAILED = 9001,
        KAFKA_CONSUMER_FAILED = 9002,
        KAFKA_MESSAGE_SEND_FAILED = 9003,
        KAFKA_MESSAGE_CONSUME_FAILED = 9004,
        KAFKA_TOPIC_NOT_FOUND = 9005,
        KAFKA_TOPIC_CREATE_FAILED = 9006,
        KAFKA_PARTITION_INVALID = 9007,
        KAFKA_OFFSET_INVALID = 9008,
        KAFKA_GROUP_ID_INVALID = 9009,
        KAFKA_BROKER_UNAVAILABLE = 9010,

        // 网络相关错误码 10000-10999
        NETWORK_CONNECTION_FAILED = 10000,
        NETWORK_TIMEOUT = 10001,
        NETWORK_DISCONNECTED = 10002,
        NETWORK_RECONNECT_FAILED = 10003,
        NETWORK_REQUEST_FAILED = 10004,
        NETWORK_RESPONSE_INVALID = 10005,
        NETWORK_PROTOCOL_ERROR = 10006,
        NETWORK_DNS_RESOLVE_FAILED = 10007,
        NETWORK_SSL_ERROR = 10008,
        NETWORK_HOST_UNREACHABLE = 10009,
        NETWORK_ADDRESS_INVALID = 10010,

        // gRPC相关错误码 11000-11999
        GRPC_CONNECTION_FAILED = 11000,
        GRPC_REQUEST_FAILED = 11001,
        GRPC_RESPONSE_INVALID = 11002,
        GRPC_TIMEOUT = 11003,
        GRPC_CANCELLED = 11004,
        GRPC_UNKNOWN = 11005,
        GRPC_INVALID_ARGUMENT = 11006,
        GRPC_DEADLINE_EXCEEDED = 11007,
        GRPC_NOT_FOUND = 11008,
        GRPC_ALREADY_EXISTS = 11009,
        GRPC_PERMISSION_DENIED = 11010,

        // WebSocket相关错误码 12000-12999
        WS_CONNECTION_FAILED = 12000,
        WS_DISCONNECTED = 12001,
        WS_MESSAGE_SEND_FAILED = 12002,
        WS_HANDSHAKE_FAILED = 12003,
        WS_AUTH_FAILED = 12004,
        WS_PING_TIMEOUT = 12005,
        WS_PONG_TIMEOUT = 12006,
        WS_MESSAGE_SIZE_EXCEEDED = 12007,
        WS_MESSAGE_TYPE_INVALID = 12008,
        WS_RECONNECT_FAILED = 12009,
        WS_CLOSE_ABNORMAL = 12010,

        // 安全相关错误码 13000-13999
        SECURITY_TOKEN_INVALID = 13000,
        SECURITY_TOKEN_EXPIRED = 13001,
        SECURITY_SIGNATURE_INVALID = 13002,
        SECURITY_ENCRYPTION_FAILED = 13003,
        SECURITY_DECRYPTION_FAILED = 13004,
        SECURITY_HASH_FAILED = 13005,
        SECURITY_CSRF_TOKEN_INVALID = 13006,
        SECURITY_IP_BLOCKED = 13007,
        SECURITY_TOO_MANY_REQUESTS = 13008,
        SECURITY_PARAMETERS_TAMPERED = 13009,
        SECURITY_UNAUTHORIZED = 13010,

        // 最大错误码
        MAX_ERROR_CODE = 20000
    };

    /**
     * @brief 获取错误码描述
     * @param code 错误码
     * @return 错误码描述
     */
    static std::string GetMessage(int code);

    /**
     * @brief 获取错误码描述
     * @param code 错误码
     * @return 错误码描述
     */
    static std::string GetMessage(Code code);

    /**
     * @brief 获取错误码类型
     * @param code 错误码
     * @return 错误码类型
     */
    static std::string GetType(int code);

    /**
     * @brief 获取错误码类型
     * @param code 错误码
     * @return 错误码类型
     */
    static std::string GetType(Code code);

    /**
     * @brief 是否成功
     * @param code 错误码
     * @return 成功返回true，失败返回false
     */
    static bool IsSuccess(int code);

    /**
     * @brief 是否成功
     * @param code 错误码
     * @return 成功返回true，失败返回false
     */
    static bool IsSuccess(Code code);

private:
    static std::unordered_map<int, std::string> error_messages_;
    static std::unordered_map<int, std::string> error_types_;
};

} // namespace utils
} // namespace im

#endif // IM_ERROR_CODE_H 