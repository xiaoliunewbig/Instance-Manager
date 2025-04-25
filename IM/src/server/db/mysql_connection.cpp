#include <stdexcept>
#include <sstream>

#include "../../../include/server/db/mysql_connection.h"
#include <stdexcept>
#include <sstream>
#include "../../../include/server/utils/logger.h"

namespace im {
namespace db {

MySQLConnection::MySQLConnection(
    const std::string& host,
    int port,
    const std::string& user,
    const std::string& password,
    const std::string& database
) : host_(host),
    port_(port),
    user_(user),
    password_(password),
    database_(database),
    mysql_(nullptr),
    connected_(false) {
}

MySQLConnection::~MySQLConnection() {
    Disconnect();
}

bool MySQLConnection::Connect() {
    if (IsConnected()) {
        return true;
    }
    
    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        LOG_CRITICAL("MySQL初始化失败");
        return false;
    }
    
    bool reconnect = true;
    mysql_options(mysql_, MYSQL_OPT_RECONNECT, &reconnect);
    
    mysql_options(mysql_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    
    int connect_timeout = 5;
    mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout);
    
    unsigned long max_allowed_packet = 1073741824; // 1GB
    mysql_options(mysql_, MYSQL_OPT_MAX_ALLOWED_PACKET, &max_allowed_packet);
    
    if (!mysql_real_connect(mysql_, host_.c_str(), user_.c_str(), 
                            password_.c_str(), database_.c_str(), 
                            port_, nullptr, 0)) {
        std::string error = mysql_error(mysql_);
        LOG_CRITICAL("MySQL连接失败: {}", error);
        mysql_close(mysql_);
        mysql_ = nullptr;
        return false;
    }
    
    LOG_INFO("MySQL连接成功");
    connected_ = true;
    
    bool success = true;
    
    success &= ExecuteSimpleQuery("SET SESSION max_allowed_packet=1073741824");
    if (!success) {
        LOG_WARN("无法设置max_allowed_packet");
    }
    
    success &= ExecuteSimpleQuery("SET SESSION net_buffer_length=1048576");
    if (!success) {
        LOG_WARN("无法设置net_buffer_length");
    }
    
    success &= ExecuteSimpleQuery("SET SESSION interactive_timeout=28800");
    success &= ExecuteSimpleQuery("SET SESSION wait_timeout=28800");
    success &= ExecuteSimpleQuery("SET SESSION connect_timeout=60");
    success &= ExecuteSimpleQuery("SET SESSION net_read_timeout=60");
    success &= ExecuteSimpleQuery("SET SESSION net_write_timeout=60");
    
    return true;
}

void MySQLConnection::Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
    
    connected_ = false;
}

bool MySQLConnection::IsConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!mysql_ || !connected_) {
        return false;
    }
    
    return mysql_ping(mysql_) == 0;
}

MySQLConnection::ResultSet MySQLConnection::ExecuteQuery(
    const std::string& sql, 
    const std::vector<std::string>& params
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!CheckAndReconnect()) {
        throw std::runtime_error("Failed to connect to database: " + last_error_);
    }
    
    try {
        MYSQL_STMT* stmt = PrepareStatement(sql, params);
        if (!stmt) {
            throw std::runtime_error("Failed to prepare statement: " + last_error_);
        }
        
        if (mysql_stmt_execute(stmt) != 0) {
            last_error_ = mysql_stmt_error(stmt);
            LogError("Failed to execute query: " + last_error_);
            mysql_stmt_close(stmt);
            throw std::runtime_error("Failed to execute query: " + last_error_);
        }
        
        ResultSet results = FetchResults(stmt);
        
        mysql_stmt_close(stmt);
        
        return results;
    } catch (const std::exception& e) {
        LogError("ExecuteQuery exception: " + std::string(e.what()));
        throw;
    }
}

bool MySQLConnection::ExecuteUpdate(
    const std::string& sql, 
    const std::vector<std::string>& params
) {
    int retries = 3;
    while (retries > 0) {
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            
            if (!CheckAndReconnect()) {
                LogError("Failed to connect to database: " + last_error_);
                return false;
            }
            
            MYSQL_STMT* stmt = PrepareStatement(sql, params);
            if (!stmt) {
                LogError("Failed to prepare statement: " + last_error_);
                return false;
            }
            
            if (mysql_stmt_execute(stmt) != 0) {
                last_error_ = mysql_stmt_error(stmt);
                LogError("Failed to execute update: " + last_error_);
                mysql_stmt_close(stmt);
                return false;
            }
            
            my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
            
            mysql_stmt_close(stmt);
            
            return affected_rows > 0;
        } catch (const std::exception& e) {
            std::string error = e.what();
            if (error.find("max_allowed_packet") != std::string::npos && retries > 1) {
                LogWarning("[MySQL] 执行查询时出现max_allowed_packet错误，尝试重新设置并重试");
                
                try {
                    ExecuteUpdate("SET SESSION max_allowed_packet=1073741824");
                } catch (...) {
                    // 忽略设置错误，继续重试
                }
                
                retries--;
                continue;
            }
            
            LogError(std::string("[MySQL] ") + __func__ + " 错误: " + e.what());
            retries = 0;
        }
    }
    return false;
}

int64_t MySQLConnection::ExecuteInsert(
    const std::string& sql, 
    const std::vector<std::string>& params
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!CheckAndReconnect()) {
        LogError("Failed to connect to database: " + last_error_);
        return 0;
    }
    
    try {
        MYSQL_STMT* stmt = PrepareStatement(sql, params);
        if (!stmt) {
            LogError("Failed to prepare statement: " + last_error_);
            return 0;
        }
        
        if (mysql_stmt_execute(stmt) != 0) {
            last_error_ = mysql_stmt_error(stmt);
            LogError("Failed to execute insert: " + last_error_);
            mysql_stmt_close(stmt);
            return 0;
        }
        
        my_ulonglong insert_id = mysql_stmt_insert_id(stmt);
        
        mysql_stmt_close(stmt);
        
        return static_cast<int64_t>(insert_id);
    } catch (const std::exception& e) {
        LogError("ExecuteInsert exception: " + std::string(e.what()));
        return 0;
    }
}

bool MySQLConnection::BeginTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!CheckAndReconnect()) {
        LogError("Failed to connect to database: " + last_error_);
        return false;
    }
    
    if (mysql_query(mysql_, "START TRANSACTION") != 0) {
        last_error_ = mysql_error(mysql_);
        LogError("Failed to begin transaction: " + last_error_);
        return false;
    }
    
    return true;
}

bool MySQLConnection::CommitTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!CheckAndReconnect()) {
        LogError("Failed to connect to database: " + last_error_);
        return false;
    }
    
    if (mysql_query(mysql_, "COMMIT") != 0) {
        last_error_ = mysql_error(mysql_);
        LogError("Failed to commit transaction: " + last_error_);
        return false;
    }
    
    return true;
}

bool MySQLConnection::RollbackTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!CheckAndReconnect()) {
        LogError("Failed to connect to database: " + last_error_);
        return false;
    }
    
    if (mysql_query(mysql_, "ROLLBACK") != 0) {
        last_error_ = mysql_error(mysql_);
        LogError("Failed to rollback transaction: " + last_error_);
        return false;
    }
    
    return true;
}

std::string MySQLConnection::GetLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

MYSQL_STMT* MySQLConnection::PrepareStatement(
    const std::string& sql, 
    const std::vector<std::string>& params
) {
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (!stmt) {
        last_error_ = mysql_error(mysql_);
        LogError("Failed to initialize statement: " + last_error_);
        return nullptr;
    }
    
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length()) != 0) {
        last_error_ = mysql_stmt_error(stmt);
        LogError("Failed to prepare statement: " + last_error_);
        mysql_stmt_close(stmt);
        return nullptr;
    }
    
    unsigned long param_count = mysql_stmt_param_count(stmt);
    if (param_count != params.size()) {
        std::ostringstream oss;
        oss << "Parameter count mismatch: expected " << param_count << ", got " << params.size();
        last_error_ = oss.str();
        LogError(last_error_);
        mysql_stmt_close(stmt);
        return nullptr;
    }
    
    if (param_count > 0) {
        if (!BindParams(stmt, params)) {
            mysql_stmt_close(stmt);
            return nullptr;
        }
    }
    
    return stmt;
}

bool MySQLConnection::BindParams(
    MYSQL_STMT* stmt, 
    const std::vector<std::string>& params
) {
    unsigned long param_count = mysql_stmt_param_count(stmt);
    
    std::vector<MYSQL_BIND> binds(param_count);
    std::vector<unsigned long> lengths(param_count);
    
    for (unsigned long i = 0; i < param_count; i++) {
        const std::string& param = params[i];
        lengths[i] = param.length();
        
        memset(&binds[i], 0, sizeof(MYSQL_BIND));
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = const_cast<char*>(param.c_str());
        binds[i].buffer_length = lengths[i];
        binds[i].length = &lengths[i];
        binds[i].is_null = 0;
    }
    
    if (mysql_stmt_bind_param(stmt, binds.data()) != 0) {
        last_error_ = mysql_stmt_error(stmt);
        LogError("Failed to bind parameters: " + last_error_);
        return false;
    }
    
    return true;
}

MySQLConnection::ResultSet MySQLConnection::FetchResults(MYSQL_STMT* stmt) {
    ResultSet results;
    
    MYSQL_RES* meta = mysql_stmt_result_metadata(stmt);
    if (!meta) {
        return results;
    }
    
    int num_fields = mysql_num_fields(meta);
    
    std::vector<MYSQL_BIND> binds(num_fields);
    std::vector<std::vector<char>> buffers(num_fields);
    std::vector<unsigned long> lengths(num_fields);
    std::vector<char> is_nulls(num_fields, 0);
    std::vector<char> errors(num_fields, 0);
    
    std::vector<std::string> field_names;
    MYSQL_FIELD* fields = mysql_fetch_fields(meta);
    for (int i = 0; i < num_fields; i++) {
        field_names.push_back(fields[i].name);
        
        buffers[i].resize(65536);
        
        memset(&binds[i], 0, sizeof(MYSQL_BIND));
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = buffers[i].data();
        binds[i].buffer_length = buffers[i].size() - 1;
        binds[i].length = &lengths[i];
        binds[i].is_null = (bool*)&is_nulls[i];
        binds[i].error = (bool*)&errors[i];
    }
    
    if (mysql_stmt_bind_result(stmt, binds.data()) != 0) {
        last_error_ = mysql_stmt_error(stmt);
        LogError("Failed to bind result: " + last_error_);
        mysql_free_result(meta);
        return results;
    }
    
    if (mysql_stmt_store_result(stmt) != 0) {
        last_error_ = mysql_stmt_error(stmt);
        LogError("Failed to store result: " + last_error_);
        mysql_free_result(meta);
        return results;
    }
    
    my_ulonglong num_rows = mysql_stmt_num_rows(stmt);
    
    while (mysql_stmt_fetch(stmt) == 0) {
        Row row;
        for (int i = 0; i < num_fields; i++) {
            if (is_nulls[i]) {
                row[field_names[i]] = "NULL";
            } else {
                if (errors[i]) {
                    std::vector<char> large_buffer(lengths[i] + 1);
                    
                    MYSQL_BIND bind;
                    memset(&bind, 0, sizeof(MYSQL_BIND));
                    bind.buffer_type = MYSQL_TYPE_STRING;
                    bind.buffer = large_buffer.data();
                    bind.buffer_length = large_buffer.size() - 1;
                    bind.length = &lengths[i];
                    
                    if (mysql_stmt_fetch_column(stmt, &bind, i, 0) != 0) {
                        last_error_ = mysql_stmt_error(stmt);
                        LogError("Failed to fetch column: " + last_error_);
                        row[field_names[i]] = "";
                    } else {
                        large_buffer[lengths[i]] = '\0';
                        row[field_names[i]] = large_buffer.data();
                    }
                } else {
                    buffers[i][lengths[i]] = '\0';
                    row[field_names[i]] = buffers[i].data();
                }
            }
        }
        results.push_back(row);
    }
    
    mysql_free_result(meta);
    return results;
}

bool MySQLConnection::CheckAndReconnect() {
    if (connected_ && mysql_) {
        if (mysql_ping(mysql_) == 0) {
            return true;
        }
        
        LogDebug("MySQL connection lost, attempting to reconnect...");
        connected_ = false;
    }
    
    return Connect();
}

void MySQLConnection::LogDebug(const std::string& message) const {
    LOG_DEBUG("[MySQL] {}", message);
}

void MySQLConnection::LogError(const std::string& message) const {
    LOG_ERROR("[MySQL] {}", message);
}

void MySQLConnection::LogInfo(const std::string& message) const {
    LOG_INFO("[MySQL] {}", message);
}

void MySQLConnection::LogWarning(const std::string& message) const {
    LOG_WARN("[MySQL] {}", message);
}

bool MySQLConnection::HasAdminPrivileges() {
    auto result = ExecuteQuery("SHOW GRANTS FOR CURRENT_USER()");
    for (const auto& row : result) {
        for (const auto& [column, value] : row) {
            if (value.find("ALL PRIVILEGES") != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

bool MySQLConnection::ExecuteSimpleQuery(const std::string& sql) {
    if (!mysql_) {
        LOG_ERROR("[MySQL] MySQL连接无效");
        return false;
    }
    
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        last_error_ = mysql_error(mysql_);
        LOG_ERROR("[MySQL] 无法执行查询: {}", last_error_);
        return false;
    }
    
    return true;
}

} // namespace db
} // namespace im