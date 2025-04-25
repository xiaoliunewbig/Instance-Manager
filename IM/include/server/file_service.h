#ifndef IM_FILE_SERVICE_H
#define IM_FILE_SERVICE_H

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <grpcpp/grpcpp.h>
#include "generated/service.grpc.pb.h"
#include "db/mysql_connection.h"
#include "db/redis_client.h"
#include "kafka/kafka_producer.h"

namespace im {

/**
 * @brief 文件服务实现类，处理文件上传、下载等功能
 */
class FileServiceImpl final : public FileService::Service {
public:
    /**
     * @brief 构造函数
     * @param mysql_conn MySQL连接
     * @param redis_client Redis客户端
     * @param kafka_producer Kafka生产者
     */
    FileServiceImpl(
        std::shared_ptr<im::db::MySQLConnection> mysql_conn,
        std::shared_ptr<im::db::RedisClient> redis_client,
        std::shared_ptr<im::kafka::KafkaProducer> kafka_producer
    );

    /**
     * @brief 析构函数
     */
    ~FileServiceImpl() override = default;

    /**
     * @brief 请求上传文件
     * @param context RPC上下文
     * @param request 上传文件请求
     * @param response 上传文件响应
     * @return RPC状态
     */
    grpc::Status UploadFile(
        grpc::ServerContext* context,
        const UploadFileRequest* request,
        UploadFileResponse* response
    ) override;

    /**
     * @brief 上传文件块(流式)
     * @param context RPC上下文
     * @param reader 文件块流
     * @param response 通用响应
     * @return RPC状态
     */
    grpc::Status UploadFileChunk(
        grpc::ServerContext* context,
        grpc::ServerReader<FileChunk>* reader,
        CommonResponse* response
    ) override;

    /**
     * @brief 下载文件请求
     * @param context RPC上下文
     * @param request 下载文件请求
     * @param response 下载文件响应
     * @return RPC状态
     */
    grpc::Status DownloadFile(
        grpc::ServerContext* context,
        const DownloadFileRequest* request,
        DownloadFileResponse* response
    ) override;

    /**
     * @brief 下载文件块(流式)
     * @param context RPC上下文
     * @param request 下载文件请求
     * @param writer 文件块流
     * @return RPC状态
     */
    grpc::Status DownloadFileChunk(
        grpc::ServerContext* context,
        const DownloadFileRequest* request,
        grpc::ServerWriter<FileChunk>* writer
    ) override;

    /**
     * @brief 发送文件传输请求
     * @param context RPC上下文
     * @param request 文件传输请求
     * @param response 发送文件传输响应
     * @return RPC状态
     */
    grpc::Status SendFileTransfer(
        grpc::ServerContext* context,
        const SendFileTransferRequest* request,
        SendFileTransferResponse* response
    ) override;

    /**
     * @brief 处理文件传输请求
     * @param context RPC上下文
     * @param request 处理文件传输请求
     * @param response 处理文件传输响应
     * @return RPC状态
     */
    grpc::Status HandleFileTransfer(
        grpc::ServerContext* context,
        const HandleFileTransferRequest* request,
        HandleFileTransferResponse* response
    ) override;

private:
    /**
     * @brief 从请求元数据中获取授权令牌
     * @param context RPC上下文
     * @return 授权令牌
     */
    std::string GetAuthToken(const grpc::ServerContext* context) const;

    /**
     * @brief 验证JWT令牌
     * @param token JWT令牌
     * @param user_id 输出参数，成功时返回用户ID
     * @return 有效返回true，无效返回false
     */
    bool ValidateToken(const std::string& token, int64_t& user_id) const;

    /**
     * @brief 创建文件记录
     * @param file_info 文件信息
     * @param uploader_id 上传者ID
     * @return 成功返回文件ID，失败返回0
     */
    int64_t CreateFileRecord(const FileInfo& file_info, int64_t uploader_id);

    /**
     * @brief 获取文件信息
     * @param file_id 文件ID
     * @param file_info 输出参数，返回文件信息
     * @return 成功返回true，失败返回false
     */
    bool GetFileInfo(int64_t file_id, FileInfo* file_info);

    /**
     * @brief 检查用户是否有权限访问文件
     * @param user_id 用户ID
     * @param file_id 文件ID
     * @return 有权限返回true，无权限返回false
     */
    bool CheckFileAccess(int64_t user_id, int64_t file_id) const;

    /**
     * @brief 创建文件目录
     * @param path 目录路径
     * @return 成功返回true，失败返回false
     */
    bool CreateDirectory(const std::string& path) const;

    /**
     * @brief 生成文件存储路径
     * @param file_id 文件ID
     * @param user_id 用户ID
     * @param file_name 文件名
     * @return 文件存储路径
     */
    std::string GenerateFilePath(int64_t file_id, int64_t user_id, const std::string& file_name) const;

    std::shared_ptr<db::MySQLConnection> mysql_conn_;
    std::shared_ptr<db::RedisClient> redis_client_;
    std::shared_ptr<im::kafka::KafkaProducer> kafka_producer_;

    // 文件上传会话缓存
    std::mutex upload_sessions_mutex_;
    std::unordered_map<int64_t, std::string> upload_sessions_; // file_id -> temp_path

    // 文件存储根目录
    std::string file_storage_path_;
};

} // namespace im

#endif // IM_FILE_SERVICE_H