# IM 系统 API 文档

## gRPC 服务接口

### UserService

#### 用户注册
```protobuf
rpc Register(RegisterRequest) returns (RegisterResponse);
```
- 功能：新用户注册
- 参数说明
- 返回值说明
- 示例

#### 用户登录
```protobuf
rpc Login(LoginRequest) returns (LoginResponse);
```
- 功能：用户登录认证
- 参数说明
- 返回值说明
- 示例

### MessageService

#### 发送消息
```protobuf
rpc SendMessage(SendMessageRequest) returns (SendMessageResponse);
```
- 功能：发送即时消息
- 参数说明
- 返回值说明
- 示例

#### 获取历史消息
```protobuf
rpc GetHistory(GetHistoryRequest) returns (GetHistoryResponse);
```
- 功能：获取历史消息记录
- 参数说明
- 返回值说明
- 示例

[更多详细 API 文档待补充...] 