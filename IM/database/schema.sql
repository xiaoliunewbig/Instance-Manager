-- 创建数据库
CREATE DATABASE IF NOT EXISTS imsystem;
USE imsystem;

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    user_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(50) NOT NULL UNIQUE,
    password_hash VARCHAR(100) NOT NULL,
    salt VARCHAR(32) NOT NULL,
    email VARCHAR(100) NOT NULL UNIQUE,
    nickname VARCHAR(50) NOT NULL,
    avatar VARCHAR(255) DEFAULT NULL,
    gender TINYINT DEFAULT 0 COMMENT '0:未知, 1:男, 2:女',
    bio TEXT DEFAULT NULL COMMENT '个人简介',
    status TINYINT DEFAULT 0 COMMENT '0:离线, 1:在线, 2:离开, 3:忙碌',
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    role TINYINT DEFAULT 0 COMMENT '0:普通用户, 1:管理员',
    last_login_at BIGINT DEFAULT NULL,
    created_at BIGINT DEFAULT NULL,
    updated_at BIGINT DEFAULT NULL,
    notification_enabled BOOLEAN DEFAULT TRUE,
    is_admin BOOLEAN DEFAULT FALSE,
    is_approved BOOLEAN DEFAULT FALSE COMMENT '是否已审批通过',
    INDEX idx_email (email),
    INDEX idx_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 用户验证码表
CREATE TABLE IF NOT EXISTS verification_codes (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    email VARCHAR(100) NOT NULL,
    code VARCHAR(10) NOT NULL,
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expire_time TIMESTAMP,
    is_used BOOLEAN DEFAULT FALSE,
    INDEX idx_email (email)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 好友关系表
CREATE TABLE IF NOT EXISTS friend_relations (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    friend_id BIGINT NOT NULL,
    remark VARCHAR(50),
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY unique_friendship (user_id, friend_id),
    INDEX idx_user_id (user_id),
    INDEX idx_friend_id (friend_id),
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE,
    FOREIGN KEY (friend_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 好友请求表
CREATE TABLE IF NOT EXISTS friend_requests (
    request_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    from_user_id BIGINT NOT NULL,
    to_user_id BIGINT NOT NULL,
    message VARCHAR(255),
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_accepted BOOLEAN DEFAULT FALSE,
    is_rejected BOOLEAN DEFAULT FALSE,
    handle_time TIMESTAMP,
    INDEX idx_from_user (from_user_id),
    INDEX idx_to_user (to_user_id),
    FOREIGN KEY (from_user_id) REFERENCES users(user_id) ON DELETE CASCADE,
    FOREIGN KEY (to_user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 群组表
-- 1. 创建 user_groups 表
CREATE TABLE IF NOT EXISTS user_groups (
    group_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    group_name VARCHAR(50) NOT NULL,
    group_avatar VARCHAR(255),
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    creator_id BIGINT NOT NULL,
    description VARCHAR(255),
    INDEX idx_creator (creator_id),
    FOREIGN KEY (creator_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 2. 创建 group_members 表
CREATE TABLE IF NOT EXISTS group_members (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    group_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    join_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    role TINYINT DEFAULT 0 COMMENT '0:, 1:, 2:',
    UNIQUE KEY unique_membership (group_id, user_id),
    INDEX idx_group (group_id),
    INDEX idx_user (user_id),
    FOREIGN KEY (group_id) REFERENCES user_groups(group_id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 消息表（个人消息）
CREATE TABLE IF NOT EXISTS personal_messages (
    message_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    from_user_id BIGINT NOT NULL,
    to_user_id BIGINT NOT NULL,
    message_type TINYINT NOT NULL COMMENT '0:文本, 1:图片, 2:文件, 3:表情',
    content TEXT,
    extra_info TEXT COMMENT '额外信息,如图片URL, 文件信息等',
    send_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_read BOOLEAN DEFAULT FALSE,
    INDEX idx_from_user (from_user_id),
    INDEX idx_to_user (to_user_id),
    INDEX idx_send_time (send_time),
    FOREIGN KEY (from_user_id) REFERENCES users(user_id) ON DELETE CASCADE,
    FOREIGN KEY (to_user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 消息表（群组消息）
CREATE TABLE IF NOT EXISTS group_messages (
    message_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    group_id BIGINT NOT NULL,
    from_user_id BIGINT NOT NULL,
    message_type TINYINT NOT NULL COMMENT '0:文本, 1:图片, 2:文件, 3:表情',
    content TEXT,
    extra_info TEXT COMMENT '额外信息,如图片URL, 文件信息等',
    send_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_group (group_id),
    INDEX idx_from_user (from_user_id),
    INDEX idx_send_time (send_time),
    FOREIGN KEY (group_id) REFERENCES user_groups(group_id) ON DELETE CASCADE,
    FOREIGN KEY (from_user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 消息已读表（群组消息的已读状态）
CREATE TABLE IF NOT EXISTS message_read_status (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    message_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    read_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY unique_read_status (message_id, user_id),
    INDEX idx_message (message_id),
    INDEX idx_user (user_id),
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE,
    FOREIGN KEY (message_id) REFERENCES group_messages(message_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 文件表
CREATE TABLE IF NOT EXISTS files (
    file_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    file_name VARCHAR(255) NOT NULL,
    file_path VARCHAR(255) NOT NULL,
    file_size BIGINT NOT NULL,
    file_type VARCHAR(50),
    uploader_id BIGINT NOT NULL,
    upload_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_deleted BOOLEAN DEFAULT FALSE,
    INDEX idx_uploader (uploader_id),
    FOREIGN KEY (uploader_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 文件传输请求表
CREATE TABLE IF NOT EXISTS file_transfer_requests (
    request_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    from_user_id BIGINT NOT NULL,
    to_user_id BIGINT NOT NULL,
    file_id BIGINT NOT NULL,
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    status TINYINT DEFAULT 0 COMMENT '0:待处理, 1:传输中, 2:已完成, 3:失败, 4:拒绝',
    complete_time TIMESTAMP,
    INDEX idx_from_user (from_user_id),
    INDEX idx_to_user (to_user_id),
    INDEX idx_file (file_id),
    FOREIGN KEY (from_user_id) REFERENCES users(user_id) ON DELETE CASCADE,
    FOREIGN KEY (to_user_id) REFERENCES users(user_id) ON DELETE CASCADE,
    FOREIGN KEY (file_id) REFERENCES files(file_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 系统通知表
CREATE TABLE IF NOT EXISTS system_notifications (
    notification_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    content TEXT NOT NULL,
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_read BOOLEAN DEFAULT FALSE,
    notification_type TINYINT NOT NULL COMMENT '0:系统通知, 1:好友请求, 2:文件传输请求',
    related_id BIGINT COMMENT '关联ID,如好友请求ID或文件传输请求ID',
    INDEX idx_user (user_id),
    INDEX idx_create_time (create_time),
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 用户登录令牌表
CREATE TABLE IF NOT EXISTS user_tokens (
    token_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    token VARCHAR(255) NOT NULL,
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expire_time TIMESTAMP,
    is_valid BOOLEAN DEFAULT TRUE,
    INDEX idx_user (user_id),
    INDEX idx_token (token),
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 离线消息表（用于临时存储离线消息）
CREATE TABLE IF NOT EXISTS offline_messages (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    message_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_user (user_id),
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 服务器日志表
CREATE TABLE IF NOT EXISTS server_logs (
    log_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    log_level TINYINT NOT NULL COMMENT '0:DEBUG, 1:INFO, 2:WARNING, 3:ERROR, 4:FATAL',
    log_message TEXT NOT NULL,
    log_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    source VARCHAR(50),
    INDEX idx_log_time (log_time),
    INDEX idx_log_level (log_level)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 设备令牌表
CREATE TABLE IF NOT EXISTS device_tokens (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    device_token VARCHAR(255) NOT NULL,
    device_type TINYINT NOT NULL COMMENT '0:Android, 1:iOS, 2:Web',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_user_id (user_id),
    INDEX idx_device_token (device_token),
    UNIQUE KEY uk_user_token (user_id, device_token),
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 推送通知日志表
CREATE TABLE IF NOT EXISTS notification_logs (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    title VARCHAR(255) NOT NULL,
    body TEXT NOT NULL,
    type TINYINT NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_user_id (user_id),
    INDEX idx_created_at (created_at),
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 用户设置表
CREATE TABLE IF NOT EXISTS user_settings (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL UNIQUE,
    notification_enabled BOOLEAN DEFAULT TRUE,
    theme VARCHAR(20) DEFAULT 'light',
    language VARCHAR(10) DEFAULT 'zh-CN',
    created_at BIGINT DEFAULT NULL,
    updated_at BIGINT DEFAULT NULL,
    INDEX idx_user_id (user_id),
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 创建初始管理员用户（密码为 admin123 的哈希值）
INSERT INTO users (username, password_hash, email, nickname, is_admin, is_approved) 
VALUES ('admin', '$2a$10$IrGZ5cH5YZWj64/Rq.WC8Oe4L0S2BKoYRqNXBCJG3XUo.9iF4ufL2', 'admin@example.com', '系统管理员', TRUE, TRUE);

-- 创建初始管理员用户（密码为 admin123 的哈希值）
INSERT INTO users (username, password_hash, salt, email, nickname, role, status, is_admin, is_approved, created_at, updated_at) 
VALUES ('admin', '$2a$10$IrGZ5cH5YZWj64/Rq.WC8Oe4L0S2BKoYRqNXBCJG3XUo.9iF4ufL2', 'saltvalueforadmin', 'admin@example.com', '系统管理员', 1, 1, TRUE, TRUE, UNIX_TIMESTAMP(), UNIX_TIMESTAMP()); 