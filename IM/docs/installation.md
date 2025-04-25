# IM 系统安装指南

## 系统要求

### 基础环境
- Linux 操作系统（推荐 Ubuntu 20.04 或更高版本）
- C++17 或更高版本
- CMake 3.10+
- Git

### 依赖组件
1. **数据库**
   - MySQL 8.0+
   - Redis 6.0+

2. **消息队列**
   - Apache Kafka 2.8+
   - ZooKeeper 3.6+

3. **开发库**
   - gRPC
   - Protobuf
   - OpenSSL
   - Boost
   - Abseil
   - hiredis
   - librdkafka

## 安装步骤

### 1. 安装基础依赖
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake git

# CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install cmake git
```

### 2. 安装开发库
```bash
# Ubuntu/Debian
sudo apt install -y \
    libssl-dev \
    libboost-all-dev \
    libhiredis-dev \
    librdkafka-dev \
    libmysqlclient-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libgrpc++-dev \
    protobuf-compiler-grpc

# CentOS/RHEL
sudo yum install -y \
    openssl-devel \
    boost-devel \
    hiredis-devel \
    librdkafka-devel \
    mysql-devel \
    protobuf-devel \
    grpc-devel
```

### 3. 安装数据库

#### MySQL
```bash
# Ubuntu/Debian
sudo apt install -y mysql-server mysql-client

# CentOS/RHEL
sudo yum install -y mysql-server mysql-client
```

#### Redis
```bash
# Ubuntu/Debian
sudo apt install -y redis-server

# CentOS/RHEL
sudo yum install -y redis
```

### 4. 安装 Kafka
```bash
# 下载 Kafka
wget https://downloads.apache.org/kafka/3.5.0/kafka_2.13-3.5.0.tgz
tar -xzf kafka_2.13-3.5.0.tgz
cd kafka_2.13-3.5.0

# 启动 ZooKeeper
bin/zookeeper-server-start.sh config/zookeeper.properties

# 启动 Kafka
bin/kafka-server-start.sh config/server.properties
```

## 编译项目

### 1. 克隆代码
```bash
git clone <repository_url>
cd IM
```

### 2. 创建构建目录
```bash
mkdir build
cd build
```

### 3. 配置和编译
```bash
cmake ..
make -j4
```

## 配置说明

### 1. 数据库配置
编辑 `config/database.json`:
```json
{
    "mysql": {
        "host": "localhost",
        "port": 3306,
        "user": "im_user",
        "password": "your_password",
        "database": "im_db"
    },
    "redis": {
        "host": "localhost",
        "port": 6379
    }
}
```

### 2. Kafka 配置
编辑 `config/kafka.json`:
```json
{
    "brokers": "localhost:9092",
    "client_id": "im_server"
}
```

## 运行服务
```bash
./im_server --config_path=/path/to/config
```

## 常见问题

### 1. 编译错误
- 检查是否安装了所有必要的依赖
- 确保 CMake 版本满足要求
- 检查编译日志中的具体错误信息

### 2. 运行错误
- 确保所有配置文件路径正确
- 检查数据库和 Kafka 服务是否正常运行
- 查看日志文件中的错误信息

### 3. 性能问题
- 调整 Redis 配置参数
- 优化 MySQL 查询性能
- 检查网络连接状况

## 支持和帮助
如有问题，请：
1. 查看项目 Wiki
2. 提交 Issue
3. 联系技术支持团队 