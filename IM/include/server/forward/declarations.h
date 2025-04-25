#pragma once

// 前向声明所有重要类，避免循环引用
namespace im {
  class IMServer;
  class MessageServiceImpl;
  class UserServiceImpl;
  class FileServiceImpl;
  class NotificationServiceImpl;
  class PushNotificationServiceImpl;
}

namespace im {
namespace db {
  class MySQLConnection;
  class RedisClient;
}

namespace kafka {
  class KafkaProducer;
  class KafkaConsumer;
}
}
