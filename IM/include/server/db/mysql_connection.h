#ifndef IM_MYSQL_CONNECTION_H
#define IM_MYSQL_CONNECTION_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <mysql/mysql.h>

namespace im {
namespace db {

/**
 * @brief MySQL数据库连接封装类
 */
class MySQLConnection {
public:
    using Row = std::map<std::string, std::string>;
    using ResultSet = std::vector<Row>;

    /**
     * @brief 构造函数
     * @param host 主机地址
     * @param port 端口
     * @param user 用户名
     * @param password 密码
     * @param database 数据库名
     */
    MySQLConnection(
        const std::string& host,
        int port,
        const std::string& user,
        const std::string& password,
        const std::string& database
    );

    /**
     * @brief 析构函数
     */
    ~MySQLConnection();

    /**
     * @brief 连接到数据库
     * @return 成功返回true，失败返回false
     */
    bool Connect();

    /**
     * @brief 断开数据库连接
     */
    void Disconnect();

    /**
     * @brief 检查连接是否有效
     * @return 有效返回true，无效返回false
     */
    bool IsConnected() const;

    /**
     * @brief 执行查询语句
     * @param sql SQL语句
     * @param params 参数列表
     * @return 查询结果集
     */
    ResultSet ExecuteQuery(const std::string& sql, const std::vector<std::string>& params = {});

    /**
     * @brief 执行更新语句
     * @param sql SQL语句
     * @param params 参数列表
     * @return 成功返回true，失败返回false
     */
    bool ExecuteUpdate(const std::string& sql, const std::vector<std::string>& params = {});

    /**
     * @brief 执行插入语句并返回自增ID
     * @param sql SQL语句
     * @param params 参数列表
     * @return 插入记录的自增ID，失败返回0
     */
    int64_t ExecuteInsert(const std::string& sql, const std::vector<std::string>& params = {});

    /**
     * @brief 开始事务
     * @return 成功返回true，失败返回false
     */
    bool BeginTransaction();

    /**
     * @brief 提交事务
     * @return 成功返回true，失败返回false
     */
    bool CommitTransaction();

    /**
     * @brief 回滚事务
     * @return 成功返回true，失败返回false
     */
    bool RollbackTransaction();

    /**
     * @brief 获取上次错误信息
     * @return 错误信息
     */
    std::string GetLastError() const;

    /**
     * @brief 检查当前用户是否有管理员权限
     * @return 有管理员权限返回true，否则返回false
     */
    bool HasAdminPrivileges();

private:
    /**
     * @brief 预处理SQL语句，防止SQL注入
     * @param sql SQL语句
     * @param params 参数列表
     * @return 预处理后的MYSQL_STMT指针
     * @throws std::runtime_error 如果预处理失败
     */
    MYSQL_STMT* PrepareStatement(const std::string& sql, const std::vector<std::string>& params);

    /**
     * @brief 绑定参数到预处理语句
     * @param stmt 预处理语句指针
     * @param params 参数列表
     * @return 成功返回true，失败返回false
     */
    bool BindParams(MYSQL_STMT* stmt, const std::vector<std::string>& params);

    /**
     * @brief 从结果集中提取数据
     * @param stmt 预处理语句指针
     * @return 结果集
     */
    ResultSet FetchResults(MYSQL_STMT* stmt);

    /**
     * @brief 检查并尝试重连
     * @return 重连成功返回true，失败返回false
     */
    bool CheckAndReconnect();

    /**
     * @brief 记录日志，调试用
     * @param message 日志消息
     */
    void LogDebug(const std::string& message) const;

    /**
     * @brief 记录信息日志
     * @param message 日志信息
     */
    void LogInfo(const std::string& message) const;
    
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

    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string database_;
    
    MYSQL* mysql_;
    mutable std::mutex mutex_;
    bool connected_;
    std::string last_error_;
};

} // namespace db
} // namespace im

#endif // IM_MYSQL_CONNECTION_H 