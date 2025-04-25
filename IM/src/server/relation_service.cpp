#include "server/relation_service.h"
#include "server/utils/logger.h"
#include "server/utils/security.h"
#include "server/utils/datetime.h"
#include "server/utils/config.h"
#include "server/utils/string_util.h"
#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>

namespace im {

using json = nlohmann::json;

RelationServiceImpl::RelationServiceImpl(
    std::shared_ptr<db::MySQLConnection> mysql_conn,
    std::shared_ptr<db::RedisClient> redis_client,
    std::shared_ptr<kafka::KafkaProducer> kafka_producer
) : mysql_conn_(mysql_conn),
    redis_client_(redis_client),
    kafka_producer_(kafka_producer) {
    LOG_INFO("RelationServiceImpl initialized");
}

grpc::Status RelationServiceImpl::AddFriend(
    grpc::ServerContext* context,
    const AddFriendRequest* request,
    AddFriendResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t current_user_id;
    if (!ValidateToken(token, current_user_id)) {
        response->set_success(false);
        response->set_message("Unauthorized: Invalid or missing token");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "无效的令牌");
    }
    
    // 验证请求中的用户ID是否与令牌中的一致
    int64_t from_user_id = request->from_user_id();
    if (from_user_id != current_user_id) {
        response->set_success(false);
        response->set_message("无权代表其他用户发送请求");
        return grpc::Status::OK;
    }
    
    int64_t to_user_id = request->to_user_id();
    
    // 检查目标用户是否存在
    if (!CheckUserExists(to_user_id)) {
        response->set_success(false);
        response->set_message("目标用户不存在");
        return grpc::Status::OK;
    }
    
    // 检查是否已经是好友
    if (CheckIfAlreadyFriends(from_user_id, to_user_id)) {
        response->set_success(false);
        response->set_message("已经是好友关系");
        return grpc::Status::OK;
    }
    
    // 检查是否已经发送过好友请求
    if (CheckPendingRequest(from_user_id, to_user_id)) {
        response->set_success(false);
        response->set_message("已经发送过好友请求，请等待对方处理");
        return grpc::Status::OK;
    }
    
    // 创建好友请求
    int64_t request_id = 0;
    try {
        std::string sql = "INSERT INTO friend_requests "
                         "(from_user_id, to_user_id, message, create_time, is_accepted, is_rejected) "
                         "VALUES (?, ?, ?, ?, 0, 0)";
        
        int64_t now = utils::DateTime::NowSeconds();
        request_id = mysql_conn_->ExecuteInsert(sql, {
            std::to_string(from_user_id),
            std::to_string(to_user_id),
            request->message(),
            std::to_string(now)
        });
        
        if (request_id == 0) {
            response->set_success(false);
            response->set_message("创建好友请求失败");
            return grpc::Status::OK;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("创建好友请求失败: {}", e.what());
        response->set_success(false);
        response->set_message("服务器内部错误");
        return grpc::Status::OK;
    }
    
    // 发送事件到Kafka
    json event;
    event["event_type"] = "friend_request_sent";
    event["from_user_id"] = from_user_id;
    event["to_user_id"] = to_user_id;
    event["request_id"] = request_id;
    event["timestamp"] = utils::DateTime::NowSeconds();
    
    kafka_producer_->SendMessage("im_events", event.dump(), std::to_string(to_user_id));
    
    // 如果用户在线，通过Redis缓存通知对方
    std::string online_key = "user:" + std::to_string(to_user_id) + ":online";
    if (redis_client_->KeyExists(online_key)) {
        // 将通知存储到Redis中，等待客户端轮询或通过WebSocket推送
        std::string notification_key = "user:" + std::to_string(to_user_id) + ":notifications";
        json notification;
        notification["type"] = "friend_request";
        notification["from_user_id"] = from_user_id;
        notification["request_id"] = request_id;
        notification["message"] = request->message();
        notification["timestamp"] = utils::DateTime::NowSeconds();
        
        redis_client_->ListPush(notification_key, notification.dump());
        redis_client_->Expire(notification_key, 604800);  // 一周过期
    }
    
    // 设置响应
    response->set_success(true);
    response->set_message("好友请求已发送");
    response->set_request_id(request_id);
    
    LOG_INFO("用户 {} 向用户 {} 发送好友请求, id={}", from_user_id, to_user_id, request_id);
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::HandleFriendRequest(
    grpc::ServerContext* context,
    const HandleFriendRequestRequest* request,
    HandleFriendRequestResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t current_user_id;
    if (!ValidateToken(token, current_user_id)) {
        response->set_success(false);
        response->set_message("Unauthorized: Invalid or missing token");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "无效的令牌");
    }
    
    int64_t request_id = request->request_id();
    bool accept = request->accept();
    
    // 获取好友请求信息
    int64_t from_user_id = 0;
    int64_t to_user_id = 0;
    bool is_accepted = false;
    bool is_rejected = false;
    
    try {
        std::string sql = "SELECT from_user_id, to_user_id, is_accepted, is_rejected "
                         "FROM friend_requests WHERE id = ?";
        
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {std::to_string(request_id)});
        
        if (results.empty()) {
            response->set_success(false);
            response->set_message("好友请求不存在");
            return grpc::Status::OK;
        }
        
        auto& req = results[0];
        from_user_id = std::stoll(req["from_user_id"]);
        to_user_id = std::stoll(req["to_user_id"]);
        is_accepted = req["is_accepted"] == "1";
        is_rejected = req["is_rejected"] == "1";
        
        // 验证当前用户是否是请求的接收者
        if (to_user_id != current_user_id) {
            response->set_success(false);
            response->set_message("Forbidden: Cannot handle friend request with unauthorized user");
            return grpc::Status::OK;
        }
        
        // 检查请求是否已经被处理
        if (is_accepted || is_rejected) {
            response->set_success(false);
            response->set_message("此好友请求已经被处理");
            return grpc::Status::OK;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("获取好友请求信息失败: {}", e.what());
        response->set_success(false);
        response->set_message("Internal error: Failed to get friend request information");
        return grpc::Status::OK;
    }
    
    // 更新好友请求状态
    try {
        mysql_conn_->BeginTransaction();
        
        std::string sql = "UPDATE friend_requests SET is_accepted = ?, is_rejected = ?, update_time = ? "
                         "WHERE id = ?";
        
        int64_t now = utils::DateTime::NowSeconds();
        bool success = mysql_conn_->ExecuteUpdate(sql, {
            accept ? "1" : "0",
            accept ? "0" : "1",
            std::to_string(now),
            std::to_string(request_id)
        });
        
        if (!success) {
            mysql_conn_->RollbackTransaction();
            response->set_success(false);
            response->set_message("Internal error: Failed to update friend request status");
            return grpc::Status::OK;
        }
        
        // 如果接受请求，创建好友关系
        if (accept) {
            if (!CreateFriendRelation(from_user_id, to_user_id)) {
                mysql_conn_->RollbackTransaction();
                response->set_success(false);
                response->set_message("Internal error: Failed to create friend relationship");
                return grpc::Status::OK;
            }
        }
        
        mysql_conn_->CommitTransaction();
    } catch (const std::exception& e) {
        mysql_conn_->RollbackTransaction();
        LOG_ERROR("处理好友请求失败: {}", e.what());
        response->set_success(false);
        response->set_message("Internal error: Failed to handle friend request");
        return grpc::Status::OK;
    }
    
    // 发送事件到Kafka
    std::string event_type = accept ? "friend_request_accepted" : "friend_request_rejected";
    SendRelationEvent(event_type, from_user_id, to_user_id);
    
    // 设置响应
    response->set_success(true);
    response->set_message(accept ? "已接受好友请求" : "已拒绝好友请求");
    
    LOG_INFO("用户 {} {} 来自用户 {} 的好友请求", to_user_id, accept ? "接受" : "拒绝", from_user_id);
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::GetFriends(
    grpc::ServerContext* context,
    const GetFriendsRequest* request,
    GetFriendsResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t current_user_id;
    if (!ValidateToken(token, current_user_id)) {
        response->set_success(false);
        response->set_message("Unauthorized: Invalid or missing token");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "无效的令牌");
    }
    
    // 如果请求中的用户ID为0，则使用当前用户ID
    int64_t user_id = request->user_id() == 0 ? current_user_id : request->user_id();
    
    // 如果请求的是其他用户的好友列表，需要检查权限
    if (user_id != current_user_id) {
        // 这里可以添加权限检查逻辑，例如是否是管理员
        // 简单起见，这里允许查询任何用户的好友列表
    }
    
    try {
        // 查询用户的好友关系
        std::string sql = "SELECT f.friend_id, f.remark, u.username, u.nickname, u.avatar, u.status "
                         "FROM friend_relations f "
                         "JOIN users u ON f.friend_id = u.id "
                         "WHERE f.user_id = ?";
        
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {std::to_string(user_id)});
        
        // 设置响应
        response->set_success(true);
        response->set_message("成功");
        
        for (const auto& friend_row : results) {
            UserInfo* friend_info = response->add_friends();
            
            int64_t friend_id = std::stoll(friend_row.at("friend_id"));
            friend_info->set_user_id(friend_id);
            friend_info->set_username(friend_row.at("username"));
            
            if (friend_row.find("nickname") != friend_row.end() && !friend_row.at("nickname").empty()) {
                friend_info->set_nickname(friend_row.at("nickname"));
            }
            
            if (friend_row.find("avatar") != friend_row.end() && !friend_row.at("avatar").empty()) {
                friend_info->set_avatar_url(friend_row.at("avatar"));
            }
            
            if (friend_row.find("status") != friend_row.end()) {
                // 设置基本状态
                im::UserStatus status = static_cast<im::UserStatus>(std::stoi(friend_row.at("status")));
                friend_info->set_status(status);
            }
            
            // 检查是否在线并更新状态
            std::string online_key = "user:" + std::to_string(friend_id) + ":online";
            if (redis_client_->KeyExists(online_key)) {
                friend_info->set_status(im::UserStatus::ONLINE);
            } else {
                // 如果不在线，保持数据库中的状态
            }
        }
        
        LOG_INFO("获取用户 {} 的好友列表, 共 {} 个好友", user_id, results.size());
    } catch (const std::exception& e) {
        LOG_ERROR("获取好友列表失败: {}", e.what());
        response->set_success(false);
        response->set_message("Internal error: Failed to get friend list");
    }
    
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::GetPendingFriendRequests(
    grpc::ServerContext* context,
    const GetPendingFriendRequestsRequest* request,
    GetPendingFriendRequestsResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t current_user_id;
    if (!ValidateToken(token, current_user_id)) {
        response->set_success(false);
        response->set_message("Unauthorized: Invalid or missing token");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "无效的令牌");
    }
    
    // 如果请求中的用户ID为0，则使用当前用户ID
    int64_t user_id = request->user_id() == 0 ? current_user_id : request->user_id();
    
    // 如果请求的是其他用户的待处理请求，需要检查权限
    if (user_id != current_user_id) {
        // 简单起见，这里允许查询任何用户的待处理请求
    }
    
    try {
        // 查询待处理的好友请求
        std::string sql = "SELECT fr.id, fr.from_user_id, fr.message, fr.create_time, "
                         "u.username, u.nickname, u.avatar "
                         "FROM friend_requests fr "
                         "JOIN users u ON fr.from_user_id = u.id "
                         "WHERE fr.to_user_id = ? AND fr.is_accepted = 0 AND fr.is_rejected = 0 "
                         "ORDER BY fr.create_time DESC";
        
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {std::to_string(user_id)});
        
        // 设置响应
        response->set_success(true);
        response->set_message("成功");
        
        for (const auto& req : results) {
            FriendRequest* request = response->add_requests();
            
            request->set_request_id(std::stoll(req.at("id")));
            request->set_from_user_id(std::stoll(req.at("from_user_id")));
            request->set_to_user_id(user_id);
            request->set_message(req.at("message"));
            request->set_create_time(std::stoll(req.at("create_time")));
            request->set_is_accepted(false);
            request->set_is_rejected(false);
        }
        
        LOG_INFO("获取用户 {} 的待处理好友请求, 共 {} 个请求", user_id, results.size());
    } catch (const std::exception& e) {
        LOG_ERROR("获取待处理好友请求失败: {}", e.what());
        response->set_success(false);
        response->set_message("Internal error: Failed to get pending friend requests");
    }
    
    return grpc::Status::OK;
}

grpc::Status RelationServiceImpl::DeleteFriend(
    grpc::ServerContext* context,
    const FriendRelation* request,
    CommonResponse* response
) {
    // 验证授权
    std::string token = GetAuthToken(context);
    int64_t current_user_id;
    if (!ValidateToken(token, current_user_id)) {
        response->set_success(false);
        response->set_message("Unauthorized: Invalid or missing token");
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "无效的令牌");
    }
    
    int64_t user_id = request->user_id();
    int64_t friend_id = request->friend_id();
    
    // 验证当前用户是否是关系的拥有者
    if (user_id != current_user_id) {
        response->set_success(false);
        response->set_message("Forbidden: Cannot delete friend relationship with unauthorized user");
        return grpc::Status::OK;
    }
    
    try {
        mysql_conn_->BeginTransaction();
        
        // 删除双向好友关系
        std::string sql = "DELETE FROM friend_relations WHERE "
                         "((user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?))";
        
        bool success = mysql_conn_->ExecuteUpdate(sql, {
            std::to_string(user_id),
            std::to_string(friend_id),
            std::to_string(friend_id),
            std::to_string(user_id)
        });
        
        if (!success) {
            mysql_conn_->RollbackTransaction();
            response->set_success(false);
            response->set_message("Internal error: Failed to delete friend relationship");
            return grpc::Status::OK;
        }
        
        mysql_conn_->CommitTransaction();
        
        // 发送事件到Kafka
        SendRelationEvent("friend_deleted", user_id, friend_id);
        
        response->set_success(true);
        response->set_message("Friend successfully deleted");
        
        LOG_INFO("用户 {} 删除了与用户 {} 的好友关系", user_id, friend_id);
    } catch (const std::exception& e) {
        mysql_conn_->RollbackTransaction();
        LOG_ERROR("删除好友关系失败: {}", e.what());
        response->set_success(false);
        response->set_message("Internal error: Failed to delete friend relationship");
    }
    
    return grpc::Status::OK;
}

std::string RelationServiceImpl::GetAuthToken(const grpc::ServerContext* context) const {
    auto metadata = context->client_metadata();
    auto it = metadata.find("authorization");
    if (it != metadata.end()) {
        std::string auth_header(it->second.data(), it->second.size());
        if (auth_header.substr(0, 7) == "Bearer ") {
            return auth_header.substr(7);
        }
    }
    return "";
}

bool RelationServiceImpl::ValidateToken(const std::string& token, int64_t& user_id) const {
    if (token.empty()) {
        return false;
    }
    
    std::map<std::string, std::string> payload;
    std::string secret = utils::Config::GetInstance().GetString("security.jwt_secret", "your_jwt_secret");
    
    if (!utils::Security::VerifyJWT(token, secret, payload)) {
        return false;
    }
    
    auto it = payload.find("user_id");
    if (it == payload.end()) {
        return false;
    }
    
    try {
        user_id = std::stoll(it->second);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Token中的user_id无效: {}", e.what());
        return false;
    }
}

bool RelationServiceImpl::CheckUserExists(int64_t user_id) const {
    try {
        std::string sql = "SELECT COUNT(*) as count FROM users WHERE user_id = ?";
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {std::to_string(user_id)});
        
        if (!results.empty()) {
            return std::stoi(results[0]["count"]) > 0;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("检查用户是否存在失败: {}", e.what());
    }
    
    return false;
}

bool RelationServiceImpl::CheckIfAlreadyFriends(int64_t user_id, int64_t friend_id) const {
    try {
        std::string sql = "SELECT COUNT(*) as count FROM friend_relations "
                         "WHERE user_id = ? AND friend_id = ?";
        
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {
            std::to_string(user_id),
            std::to_string(friend_id)
        });
        
        if (!results.empty()) {
            return std::stoi(results[0]["count"]) > 0;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("检查好友关系失败: {}", e.what());
    }
    
    return false;
}

bool RelationServiceImpl::CheckPendingRequest(int64_t from_user_id, int64_t to_user_id) const {
    try {
        std::string sql = "SELECT COUNT(*) as count FROM friend_requests "
                         "WHERE from_user_id = ? AND to_user_id = ? "
                         "AND is_accepted = 0 AND is_rejected = 0";
        
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {
            std::to_string(from_user_id),
            std::to_string(to_user_id)
        });
        
        if (!results.empty()) {
            return std::stoi(results[0]["count"]) > 0;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("检查待处理请求失败: {}", e.what());
    }
    
    return false;
}

bool RelationServiceImpl::CreateFriendRelation(int64_t user_id, int64_t friend_id) {
    try {
        int64_t now = utils::DateTime::NowSeconds();
        
        // 创建双向好友关系
        std::string sql = "INSERT INTO friend_relations (user_id, friend_id, create_time) VALUES (?, ?, ?)";
        
        // 添加第一条关系记录
        bool success1 = mysql_conn_->ExecuteUpdate(sql, {
            std::to_string(user_id),
            std::to_string(friend_id),
            std::to_string(now)
        });
        
        // 添加反向关系记录
        bool success2 = mysql_conn_->ExecuteUpdate(sql, {
            std::to_string(friend_id),
            std::to_string(user_id),
            std::to_string(now)
        });
        
        return success1 && success2;
    } catch (const std::exception& e) {
        LOG_ERROR("创建好友关系失败: {}", e.what());
        return false;
    }
}

void RelationServiceImpl::SendRelationEvent(
    const std::string& event_type,
    int64_t user_id,
    int64_t friend_id
) {
    json event;
    event["event_type"] = event_type;
    event["user_id"] = user_id;
    event["friend_id"] = friend_id;
    event["timestamp"] = utils::DateTime::NowSeconds();
    
    // 发送给两个用户
    kafka_producer_->SendMessage("im_events", event.dump(), std::to_string(user_id));
    kafka_producer_->SendMessage("im_events", event.dump(), std::to_string(friend_id));
}

bool RelationServiceImpl::GetUserInfoById(int64_t user_id, UserInfo* user_info) {
    try {
        std::string sql = "SELECT user_id, username, email, nickname, avatar, status FROM users WHERE user_id = ?";
        db::MySQLConnection::ResultSet results = mysql_conn_->ExecuteQuery(sql, {std::to_string(user_id)});
        
        if (results.empty()) {
            return false;
        }
        
        auto& user = results[0];
        user_info->set_user_id(std::stoll(user["user_id"]));
        user_info->set_username(user["username"]);
        
        if (user.find("nickname") != user.end() && !user["nickname"].empty()) {
            user_info->set_nickname(user["nickname"]);
        }
        
        if (user.find("avatar") != user.end() && !user["avatar"].empty()) {
            user_info->set_avatar_url(user["avatar"]);
        }
        
        if (user.find("status") != user.end()) {
            int status = std::stoi(user["status"]);
            user_info->set_status(static_cast<im::UserStatus>(status));
        }
        
        // 检查是否在线
        std::string online_key = "user:" + std::to_string(user_id) + ":online";
        if (redis_client_->KeyExists(online_key)) {
            user_info->set_status(im::UserStatus::ONLINE);
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("获取用户信息失败: {}", e.what());
        return false;
    }
}

} // namespace im 