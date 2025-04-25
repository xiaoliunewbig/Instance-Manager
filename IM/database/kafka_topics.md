# Kafka主题设计

本文档定义了即时通信系统中Kafka主题的设计和使用方式。

## 消息相关主题

### 即时消息主题
- 主题名: `im_messages`
- 分区数: 16
- 复制因子: 3
- 保留策略: 7天
- 用途: 存储所有即时消息，包括个人消息和群组消息
- 消息格式: 
  ```json
  {
    "message_id": 12345,
    "from_user_id": 100,
    "to_user_id": 200,  // 如果是群消息，则为group_id
    "chat_type": "personal",  // "personal" 或 "group"
    "message_type": 0,  // 0:文本, 1:图片, 2:文件, 3:表情
    "content": "消息内容",
    "extra_info": "{}",  // 额外信息的JSON字符串
    "send_time": 1617762309123,
    "is_read": false
  }
  ```
- 消费者组:
  - message_persistence_group: 将消息持久化到MySQL
  - offline_message_processor_group: 处理离线消息
  - message_notification_group: 发送消息通知

### 离线消息主题
- 主题名: `im_offline_messages`
- 分区数: 8
- 复制因子: 3
- 保留策略: 30天
- 用途: 存储离线用户的消息，等待用户上线后推送
- 消息格式: 与im_messages相同，但包含目标用户ID
- 消费者组:
  - offline_delivery_group: 当用户上线时推送离线消息

### 消息确认主题
- 主题名: `im_message_acks`
- 分区数: 8
- 复制因子: 2
- 保留策略: 2天
- 用途: 处理消息送达和已读确认
- 消息格式:
  ```json
  {
    "message_id": 12345,
    "user_id": 200,
    "ack_type": "delivered",  // "delivered" 或 "read"
    "ack_time": 1617762410456
  }
  ```

## 用户相关主题

### 用户状态变更主题
- 主题名: `im_user_status`
- 分区数: 4
- 复制因子: 2
- 保留策略: 1天
- 用途: 广播用户上线/下线/状态变更事件
- 消息格式:
  ```json
  {
    "user_id": 100,
    "status": 1,  // 0:离线, 1:在线, 2:离开, 3:忙碌
    "timestamp": 1617762309123,
    "client_info": "Web Chrome 90.0"
  }
  ```

### 用户注册主题
- 主题名: `im_user_registrations`
- 分区数: 2
- 复制因子: 3
- 保留策略: 永久保留
- 用途: 记录新用户注册事件
- 消息格式:
  ```json
  {
    "user_id": 100,
    "username": "johndoe",
    "email": "john@example.com",
    "registration_time": 1617762309123,
    "client_ip": "192.168.1.1",
    "is_approved": false
  }
  ```

## 关系相关主题

### 好友关系变更主题
- 主题名: `im_friend_relations`
- 分区数: 4
- 复制因子: 2
- 保留策略: 30天
- 用途: 记录好友关系变更事件
- 消息格式:
  ```json
  {
    "event_type": "add",  // "add", "delete", "update"
    "user_id": 100,
    "friend_id": 200,
    "timestamp": 1617762309123
  }
  ```

### 好友请求主题
- 主题名: `im_friend_requests`
- 分区数: 4
- 复制因子: 2
- 保留策略: 30天
- 用途: 处理好友请求事件
- 消息格式:
  ```json
  {
    "request_id": 12345,
    "from_user_id": 100,
    "to_user_id": 200,
    "message": "我是XX，请添加我为好友",
    "create_time": 1617762309123,
    "status": "pending"  // "pending", "accepted", "rejected"
  }
  ```

## 文件相关主题

### 文件传输请求主题
- 主题名: `im_file_transfers`
- 分区数: 4
- 复制因子: 2
- 保留策略: 7天
- 用途: 处理文件传输请求
- 消息格式:
  ```json
  {
    "request_id": 12345,
    "from_user_id": 100,
    "to_user_id": 200,
    "file_id": 5000,
    "file_name": "document.pdf",
    "file_size": 1048576,
    "create_time": 1617762309123,
    "status": "pending"  // "pending", "accepted", "rejected", "completed", "failed"
  }
  ```

### 文件处理主题
- 主题名: `im_file_processing`
- 分区数: 8
- 复制因子: 2
- 保留策略: 3天
- 用途: 处理上传文件的后处理（例如生成缩略图、病毒扫描等）
- 消息格式:
  ```json
  {
    "file_id": 5000,
    "uploader_id": 100,
    "file_path": "/uploads/2023/04/document.pdf",
    "file_type": "application/pdf",
    "processing_type": "virus_scan",  // "virus_scan", "thumbnail", "compress"
    "upload_time": 1617762309123
  }
  ```

## 通知相关主题

### 系统通知主题
- 主题名: `im_system_notifications`
- 分区数: 4
- 复制因子: 2
- 保留策略: 7天
- 用途: 处理系统通知
- 消息格式:
  ```json
  {
    "notification_id": 12345,
    "user_id": 100,
    "notification_type": 0,  // 0:系统通知, 1:好友请求, 2:文件传输请求
    "content": "系统将于今晚22:00进行维护，请提前保存重要信息。",
    "related_id": null,
    "create_time": 1617762309123
  }
  ```

### 邮件发送主题
- 主题名: `im_email_notifications`
- 分区数: 2
- 复制因子: 2
- 保留策略: 3天
- 用途: 处理需要发送的邮件通知
- 消息格式:
  ```json
  {
    "email_id": 12345,
    "recipient": "john@example.com",
    "subject": "IM系统验证码",
    "content": "您的验证码是123456，有效期为5分钟。",
    "content_type": "text/plain",
    "create_time": 1617762309123
  }
  ```

## 日志和审计主题

### 服务器日志主题
- 主题名: `im_server_logs`
- 分区数: 8
- 复制因子: 2
- 保留策略: 14天
- 用途: 收集服务器日志
- 消息格式:
  ```json
  {
    "log_id": 12345,
    "log_level": 1,  // 0:DEBUG, 1:INFO, 2:WARNING, 3:ERROR, 4:FATAL
    "service": "user_service",
    "message": "用户 100 登录失败：密码错误",
    "timestamp": 1617762309123,
    "metadata": {}
  }
  ```

### 用户活动主题
- 主题名: `im_user_activities`
- 分区数: 8
- 复制因子: 2
- 保留策略: 30天
- 用途: 记录用户活动，用于审计和分析
- 消息格式:
  ```json
  {
    "activity_id": 12345,
    "user_id": 100,
    "activity_type": "login",  // "login", "logout", "send_message", "add_friend", etc.
    "client_ip": "192.168.1.1",
    "client_info": "Web Chrome 90.0",
    "timestamp": 1617762309123,
    "metadata": {}
  }
  ```

## Kafka集群配置

### 分区分配策略
- 当多个消费者共同消费一个主题时，使用范围分区策略（RangeAssignor）确保顺序性
- 对于需要高吞吐量的主题，使用轮询分区策略（RoundRobinAssignor）

### 消息压缩
- 对于大多数主题，使用LZ4压缩
- 对于日志类主题，使用GZIP压缩以获得更高的压缩率

### 消息保留策略
- 大多数主题保留7天
- 关键业务数据保留30天
- 用户注册等重要事件永久保留

### 监控和告警
- 对消费者延迟进行监控，如果超过特定阈值则触发告警
- 对Kafka集群的资源使用进行监控（CPU、内存、磁盘） 