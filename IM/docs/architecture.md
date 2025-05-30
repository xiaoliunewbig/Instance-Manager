# 即时通信系统架构设计

## 1. 总体架构

即时通信系统采用分布式微服务架构，由多个专门的服务组成，每个服务负责特定的功能模块。系统分为客户端和服务器端两大部分，通过多种通信协议实现数据交互。

### 1.1 整体架构图

```
+------------------+     +------------------+     +------------------+
|    客户端应用     |     |      Web客户端    |     |     移动客户端    |
|  (Qt C++ 客户端)  |     |  (浏览器/Node.js) |     |  (未来可扩展)     |
+--------+---------+     +---------+--------+     +--------+---------+
         |                         |                       |
         +-------------------------+-----------------------+
                                   |
                                   | (gRPC/WebSocket/HTTP)
                                   |
+----------------------------------+-----------------------------------+
|                             API网关层                                |
|  +------------------+  +------------------+  +------------------+   |
|  |    认证与授权     |  |    负载均衡      |  |    API路由       |   |
|  +------------------+  +------------------+  +------------------+   |
+----------------------------------+-----------------------------------+
                                   |
+----------------------------------+-----------------------------------+
|                             服务层                                   |
|  +------------------+  +------------------+  +------------------+   |
|  |    用户服务      |  |    消息服务      |  |   关系服务       |   |
|  | (用户管理/认证)   |  | (消息处理/存储)   |  | (好友/群组管理)  |   |
|  +------------------+  +------------------+  +------------------+   |
|                                                                     |
|  +------------------+  +------------------+  +------------------+   |
|  |    文件服务      |  |    通知服务      |  |   管理服务       |   |
|  | (文件传输/存储)   |  | (推送/通知管理)   |  | (系统管理/监控)  |   |
|  +------------------+  +------------------+  +------------------+   |
+----------------------------------+-----------------------------------+
                                   |
+----------------------------------+-----------------------------------+
|                             数据层                                   |
|  +------------------+  +------------------+  +------------------+   |
|  |      MySQL       |  |      Redis       |  |      Kafka       |   |
|  | (持久化存储)      |  | (缓存/实时数据)   |  | (消息队列)       |   |
|  +------------------+  +------------------+  +------------------+   |
|                                                                     |
|  +------------------+  +------------------+                         |
|  |  文件存储系统    |  |   监控/日志系统   |                         |
|  | (用户文件存储)    |  | (系统运行监控)    |                         |
|  +------------------+  +------------------+                         |
+---------------------------------------------------------------------+
```

### 1.2 数据流向

1. **用户注册流程**
   - 客户端发送注册请求 → API网关 → 用户服务 → 邮件服务(Node.js) → MySQL
   - 管理员审批 → 用户服务 → 通知服务 → 客户端

2. **消息发送流程**
   - 客户端发送消息 → API网关 → 消息服务 → Kafka → 消息持久化服务 → MySQL
   - 对于在线用户: 消息服务 → WebSocket/gRPC → 接收方客户端
   - 对于离线用户: 消息服务 → 离线消息处理 → Redis/MySQL → 用户上线时推送

3. **文件传输流程**
   - 客户端上传文件 → API网关 → 文件服务 → 文件存储系统
   - 接收方确认接收 → 文件服务 → 文件传输 → 接收方客户端

## 2. 技术栈

### 2.1 后端技术

- **编程语言**: C++17/20
- **RPC框架**: gRPC (使用Protobuf作为IDL)
- **网络库**: Boost.Asio
- **多线程处理**: std::thread, Boost.Thread
- **数据库访问**: MySQL Connector/C++, cpp-redis
- **消息队列**: librdkafka (Kafka C/C++客户端)
- **JSON处理**: nlohmann/json
- **WebSocket**: Boost.Beast
- **邮件发送**: Node.js服务
- **日志系统**: spdlog
- **密码哈希**: bcrypt
- **SSL/TLS**: OpenSSL

### 2.2 前端技术

- **客户端框架**: Qt (C++)
- **Web前端**: Node.js, HTML5, CSS3, JavaScript
- **UI组件**: Qt Widgets, Qt Quick
- **图像处理**: Qt Multimedia
- **本地存储**: SQLite

### 2.3 存储技术

- **关系型数据库**: MySQL 8.0+
- **缓存数据库**: Redis 6.0+
- **消息队列**: Apache Kafka 2.8+
- **文件存储**: 本地文件系统或对象存储服务

## 3. 服务组件设计

### 3.1 用户服务 (UserService)

**职责**:
- 用户注册、登录、认证
- 用户信息管理
- 用户状态管理
- 发送验证码
- 用户审批流程管理

**API**:
- Register: 用户注册
- Login: 用户登录
- SendVerificationCode: 发送验证码
- GetUserInfo: 获取用户信息
- UpdateUserInfo: 更新用户信息
- GetPendingApprovals: 获取待审批用户列表
- ApproveUser: 审批用户注册

**数据存储**:
- MySQL: 用户表、验证码表、用户令牌表
- Redis: 用户会话缓存、验证码缓存、在线状态缓存

### 3.2 消息服务 (MessageService)

**职责**:
- 处理实时消息发送和接收
- 管理离线消息存储和推送
- 消息历史记录管理
- 消息已读状态管理

**API**:
- SendMessage: 发送消息
- GetMessageHistory: 获取消息历史
- GetOfflineMessages: 获取离线消息
- MarkMessageRead: 标记消息为已读
- MessageStream: 双向消息流(实时聊天)

**数据存储**:
- MySQL: 消息表(个人和群组)、离线消息表
- Redis: 最近消息缓存、未读消息计数
- Kafka: 消息队列、离线消息队列

### 3.3 关系服务 (RelationService)

**职责**:
- 好友关系管理
- 好友请求处理
- 群组管理
- 群组成员管理

**API**:
- AddFriend: 添加好友请求
- HandleFriendRequest: 处理好友请求
- GetFriends: 获取好友列表
- GetPendingFriendRequests: 获取待处理的好友请求
- DeleteFriend: 删除好友
- CreateGroup: 创建群组
- AddGroupMember: 添加群成员
- RemoveGroupMember: 移除群成员

**数据存储**:
- MySQL: 好友关系表、好友请求表、群组表、群组成员表
- Redis: 好友列表缓存、群组成员缓存

### 3.4 文件服务 (FileService)

**职责**:
- 文件上传处理
- 文件下载处理
- 文件传输请求管理
- 大文件分块传输
- 断点续传支持

**API**:
- UploadFile: 请求上传文件
- UploadFileChunk: 上传文件块(流式)
- DownloadFile: 请求下载文件
- DownloadFileChunk: 下载文件块(流式)
- SendFileTransferRequest: 发送文件传输请求
- HandleFileTransferRequest: 处理文件传输请求

**数据存储**:
- MySQL: 文件表、文件传输请求表
- Redis: 文件传输状态缓存
- 文件系统: 实际文件存储

### 3.5 通知服务 (NotificationService)

**职责**:
- 系统通知管理
- 实时事件推送
- 好友请求通知
- 文件传输请求通知

**API**:
- NotificationStream: 获取通知流(服务器推送)
- SendSystemNotification: 发送系统通知
- GetUnreadNotifications: 获取未读通知
- MarkNotificationRead: 标记通知为已读

**数据存储**:
- MySQL: 系统通知表
- Redis: 通知缓存、Pub/Sub通道
- Kafka: 通知事件队列

### 3.6 管理服务 (AdminService)

**职责**:
- 服务器监控和管理
- 用户连接管理
- 系统日志查看
- 强制断开用户连接
- 用户审批管理

**API**:
- GetServerStatus: 获取服务器状态
- GetActiveConnections: 获取活跃连接列表
- DisconnectUser: 强制断开用户连接
- GetServerLogs: 获取服务器日志
- ApproveUserRegistration: 审批用户注册

**数据存储**:
- MySQL: 服务器日志表
- Redis: 活跃连接缓存

### 3.7 邮件服务 (Node.js)

**职责**:
- 发送验证码邮件
- 发送系统通知邮件
- 发送好友请求邮件

**API**:
- SendEmail: 发送邮件
- SendVerificationCode: 发送验证码

**技术栈**:
- Node.js
- nodemailer库
- Express框架(REST API)

## 4. 客户端设计

### 4.1 Qt桌面客户端

**主要组件**:
- 登录/注册界面
- 主界面(好友列表、最近会话)
- 聊天窗口
- 文件传输管理器
- 设置界面
- 消息通知管理器

**技术特点**:
- 使用Qt Widgets或Qt Quick构建UI
- 多线程处理网络请求和文件传输
- 本地SQLite数据库存储聊天记录
- 集成gRPC和WebSocket客户端
- 支持图片和表情渲染
- 支持文件拖放上传

### 4.2 服务器管理客户端

**主要组件**:
- 服务器状态监控面板
- 活跃连接管理
- 用户审批界面
- 日志查看器
- 系统设置面板

**技术特点**:
- 使用Qt构建富UI界面
- 实时日志显示和过滤
- 用户连接管理和强制断开功能
- 系统资源监控图表

## 5. 安全设计

### 5.1 认证与授权

- 基于JWT的用户认证
- 密码加盐哈希存储(bcrypt)
- 角色和权限管理
- API访问控制
- 会话管理和超时机制

### 5.2 数据安全

- 传输层加密(TLS)
- 敏感数据加密存储
- SQL注入防护
- XSS攻击防护
- CSRF防护

### 5.3 网络安全

- API请求速率限制
- 防暴力破解机制
- 异常行为检测
- IP封禁机制
- 防DDoS策略

## 6. 消息处理机制

### 6.1 实时消息

- 使用WebSocket或gRPC双向流进行实时消息传递
- 消息状态跟踪(发送中、已发送、已送达、已读)
- 消息确认机制
- 消息重试机制(网络不稳定时)

### 6.2 离线消息

- 通过Kafka消息队列存储离线消息
- 用户上线时批量推送未读消息
- 消息优先级排序
- 消息过期策略

### 6.3 群组消息

- 群消息广播机制
- 群消息已读状态管理
- 特定用户的@提及功能
- 消息撤回功能

## 7. 文件传输机制

### 7.1 文件上传

- 分块上传大文件
- 传输进度跟踪和显示
- 断点续传支持
- 文件完整性验证(校验和)

### 7.2 文件下载

- 文件下载请求和确认
- 分块下载大文件
- 下载进度跟踪和显示
- 文件缓存机制

### 7.3 文件安全

- 文件类型验证
- 病毒扫描集成
- 用户配额管理
- 文件访问权限控制

## 8. 扩展性和容错设计

### 8.1 服务扩展

- 服务无状态设计，支持水平扩展
- 使用负载均衡分发请求
- 服务自动发现
- 可配置的服务参数

### 8.2 容错机制

- 服务熔断器模式
- 消息重试队列
- 备份和故障转移
- 分布式事务支持

### 8.3 监控和警报

- 服务健康检查
- 性能指标收集
- 日志聚合
- 异常自动报警

## 9. 部署架构

### 9.1 开发环境

- 本地开发环境配置
- Docker开发容器
- 模拟数据生成器
- 单元测试和集成测试

### 9.2 生产环境

- 多服务器部署
- 数据库主从复制
- Redis集群
- Kafka集群
- 负载均衡器配置
- 防火墙和安全组设置

## 10. 未来扩展方向

- 移动客户端开发(iOS/Android)
- 语音和视频通话功能
- AI智能助手集成
- 端到端加密
- 多语言支持
- 企业定制功能 