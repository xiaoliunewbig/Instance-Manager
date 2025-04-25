-- 修改users表，添加缺失的字段
ALTER TABLE users
ADD COLUMN IF NOT EXISTS salt VARCHAR(32) NULL AFTER password_hash,
ADD COLUMN IF NOT EXISTS avatar VARCHAR(255) DEFAULT NULL AFTER nickname,
ADD COLUMN IF NOT EXISTS gender TINYINT DEFAULT 0 COMMENT '0:未知, 1:男, 2:女' AFTER avatar,
ADD COLUMN IF NOT EXISTS bio TEXT DEFAULT NULL COMMENT '个人简介' AFTER gender,
ADD COLUMN IF NOT EXISTS role TINYINT DEFAULT 0 COMMENT '0:普通用户, 1:管理员' AFTER create_time,
ADD COLUMN IF NOT EXISTS last_login_at BIGINT DEFAULT NULL AFTER role,
ADD COLUMN IF NOT EXISTS created_at BIGINT DEFAULT NULL AFTER last_login_at,
ADD COLUMN IF NOT EXISTS updated_at BIGINT DEFAULT NULL AFTER created_at,
ADD COLUMN IF NOT EXISTS notification_enabled BOOLEAN DEFAULT TRUE AFTER updated_at;

-- 添加用户设置表（如果不存在）
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

-- 更新admin用户的salt字段
UPDATE users SET 
    salt = 'saltvalueforadmin',
    role = 1,
    status = 1,
    created_at = UNIX_TIMESTAMP(),
    updated_at = UNIX_TIMESTAMP()
WHERE username = 'admin';

-- 修改users表，添加缺失的字段（不使用IF NOT EXISTS）
-- 使用单独的ALTER TABLE语句添加每个列，避免一个错误影响整体执行

-- 添加salt列
ALTER TABLE users ADD COLUMN salt VARCHAR(32) NULL AFTER password_hash;

-- 添加avatar列
ALTER TABLE users ADD COLUMN avatar VARCHAR(255) DEFAULT NULL AFTER nickname;

-- 添加gender列
ALTER TABLE users ADD COLUMN gender TINYINT DEFAULT 0 COMMENT '0:未知, 1:男, 2:女' AFTER avatar;

-- 添加bio列
ALTER TABLE users ADD COLUMN bio TEXT DEFAULT NULL COMMENT '个人简介' AFTER gender;

-- 添加role列
ALTER TABLE users ADD COLUMN role TINYINT DEFAULT 0 COMMENT '0:普通用户, 1:管理员' AFTER create_time;

-- 添加last_login_at列
ALTER TABLE users ADD COLUMN last_login_at BIGINT DEFAULT NULL AFTER role;

-- 添加created_at列
ALTER TABLE users ADD COLUMN created_at BIGINT DEFAULT NULL AFTER last_login_at;

-- 添加updated_at列
ALTER TABLE users ADD COLUMN updated_at BIGINT DEFAULT NULL AFTER created_at;

-- 添加notification_enabled列
ALTER TABLE users ADD COLUMN notification_enabled BOOLEAN DEFAULT TRUE AFTER updated_at; 