# IM 系统配置说明

## 配置文件结构

### 1. 主配置文件
位置：`config/server.json`
```json
{
    "server": {
        "host": "0.0.0.0",
        "port": 50051,
        "worker_threads": 4
    },
    "log": {
        "level": "info",
        "path": "/var/log/im"
    }
}
```

### 2. 数据库配置
位置：`config/database.json`
```json
{
    "mysql": {
        "host": "localhost",
        "port": 3306,
        "user": "im_user",
        "password": "your_password",
        "database": "im_db",
        "pool_size": 10
    },
    "redis": {
        "host": "localhost",
        "port": 6379,
        "password": "",
        "db": 0
    }
}
```

### 3. Kafka 配置
位置：`config/kafka.json`
```json
{
    "brokers": "localhost:9092",
    "client_id": "im_server",
    "topics": {
        "message": "im_messages",
        "notification": "im_notifications"
    }
}
```

[更多配置说明待补充...] 