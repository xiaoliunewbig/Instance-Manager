# IM 系统 Qt 客户端设计文档

## 1. 技术选型

### 1.1 基础框架
- Qt 6.5+
- C++17
- gRPC C++ 客户端
- WebSocket 客户端
- Qt Quick (QML) + Qt Widgets 混合开发

### 1.2 主要模块
```cpp
// 项目结构
im_client/
├── src/
│   ├── main.cpp
│   ├── core/           // 核心功能
│   ├── ui/             // 界面相关
│   ├── network/        // 网络通信
│   ├── models/         // 数据模型
│   └── utils/          // 工具类
├── include/            // 头文件
├── resources/          // 资源文件
├── qml/                // QML 文件
└── tests/              // 测试文件
```

## 2. 界面设计

### 2.1 主要窗口类
```cpp
// 主窗口
class MainWindow : public QMainWindow {
    Q_OBJECT
private:
    QStackedWidget* contentStack;
    ChatWidget* chatWidget;
    ContactWidget* contactWidget;
    SettingsWidget* settingsWidget;
};

// 聊天窗口
class ChatWidget : public QWidget {
    Q_OBJECT
private:
    MessageListView* messageList;
    InputWidget* inputWidget;
    UserInfoWidget* userInfo;
};

// 联系人窗口
class ContactWidget : public QWidget {
    Q_OBJECT
private:
    ContactListView* contactList;
    GroupListView* groupList;
    SearchWidget* searchWidget;
};
```

### 2.2 界面布局

#### 2.2.1 主界面布局
```qml
// MainWindow.qml
ApplicationWindow {
    id: mainWindow
    width: 1024
    height: 768
    
    RowLayout {
        anchors.fill: parent
        
        // 左侧导航栏
        NavigationBar {
            Layout.preferredWidth: 80
            Layout.fillHeight: true
        }
        
        // 联系人列表
        ContactList {
            Layout.preferredWidth: 250
            Layout.fillHeight: true
        }
        
        // 聊天区域
        ChatArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
```

#### 2.2.2 聊天界面
```qml
// ChatArea.qml
Rectangle {
    ColumnLayout {
        anchors.fill: parent
        
        // 聊天标题栏
        ChatHeader {
            Layout.fillWidth: true
            height: 60
        }
        
        // 消息列表
        MessageList {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        
        // 输入区域
        InputArea {
            Layout.fillWidth: true
            height: 150
        }
    }
}
```

## 3. 核心类设计

### 3.1 网络通信
```cpp
// gRPC 客户端管理器
class GrpcClientManager : public QObject {
    Q_OBJECT
public:
    static GrpcClientManager& instance();
    
    // 异步消息发送
    void sendMessage(const Message& msg);
    
    // 消息接收
    void startMessageLoop();

signals:
    void messageReceived(const Message& msg);
    void connectionStateChanged(bool connected);
    
private:
    std::unique_ptr<IMService::Stub> stub_;
    QThread workerThread_;
};

// WebSocket 客户端
class WebSocketClient : public QObject {
    Q_OBJECT
public:
    void connectToServer(const QString& url);
    void sendMessage(const QString& message);

signals:
    void connected();
    void messageReceived(const QString& message);
    
private:
    QWebSocket webSocket_;
};
```

### 3.2 数据模型
```cpp
// 消息模型
class MessageModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        ContentRole = Qt::UserRole + 1,
        TimestampRole,
        SenderRole,
        MessageTypeRole
    };
    
    Q_INVOKABLE void appendMessage(const Message& message);
    
private:
    QList<Message> messages_;
};

// 联系人模型
class ContactModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        AvatarRole,
        StatusRole,
        UnreadCountRole
    };
    
    Q_INVOKABLE void updateContact(const Contact& contact);
    
private:
    QList<Contact> contacts_;
};
```

### 3.3 消息处理
```cpp
// 消息管理器
class MessageManager : public QObject {
    Q_OBJECT
public:
    void sendTextMessage(const QString& content, int64_t toUser);
    void sendImageMessage(const QString& imagePath, int64_t toUser);
    void sendFileMessage(const QString& filePath, int64_t toUser);

signals:
    void messageStatusChanged(int64_t msgId, MessageStatus status);
    
private:
    std::unique_ptr<MessageModel> model_;
    GrpcClientManager& grpcClient_;
};
```

## 4. 功能实现

### 4.1 登录界面
```cpp
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    
private:
    QLineEdit* usernameEdit_;
    QLineEdit* passwordEdit_;
    QPushButton* loginButton_;
    QPushButton* registerButton_;
};
```

### 4.2 聊天功能
```cpp
class ChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget* parent = nullptr);
    
public slots:
    void onMessageReceived(const Message& msg);
    void onSendButtonClicked();
    void onFileButtonClicked();
    
private:
    void setupUi();
    void setupConnections();
    
    MessageListView* messageList_;
    QTextEdit* inputEdit_;
    QPushButton* sendButton_;
    QPushButton* fileButton_;
};
```

### 4.3 文件传输
```cpp
class FileTransferManager : public QObject {
    Q_OBJECT
public:
    void startUpload(const QString& filePath, int64_t toUser);
    void startDownload(const QString& fileId, const QString& savePath);

signals:
    void progressChanged(const QString& fileId, int progress);
    void transferCompleted(const QString& fileId);
    void transferFailed(const QString& fileId, const QString& error);
    
private:
    QMap<QString, QFile*> activeTransfers_;
    GrpcClientManager& grpcClient_;
};
```

## 5. 样式设计

### 5.1 全局样式
```qss
/* style.qss */
QMainWindow {
    background-color: #f5f5f5;
}

QListView {
    background-color: white;
    border: none;
    border-radius: 4px;
}

QPushButton {
    background-color: #2196F3;
    color: white;
    border: none;
    border-radius: 4px;
    padding: 8px 16px;
}

QPushButton:hover {
    background-color: #1976D2;
}
```

### 5.2 自定义组件
```cpp
// 自定义消息气泡
class MessageBubble : public QWidget {
    Q_OBJECT
public:
    void setContent(const QString& content);
    void setDirection(BubbleDirection direction);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    
private:
    QString content_;
    BubbleDirection direction_;
};
```

## 6. 性能优化

### 6.1 消息列表优化
```cpp
// 虚拟滚动列表
class VirtualMessageList : public QListView {
    Q_OBJECT
protected:
    void updateGeometries() override;
    void scrollContentsBy(int dx, int dy) override;
    
private:
    void recycleItems();
    void createItemForIndex(const QModelIndex& index);
    
    QVector<MessageItemWidget*> itemCache_;
};
```

### 6.2 图片加载优化
```cpp
// 异步图片加载器
class ImageLoader : public QObject {
    Q_OBJECT
public:
    static ImageLoader& instance();
    void loadImage(const QString& url, QLabel* label);
    
private:
    QThreadPool threadPool_;
    QCache<QString, QPixmap> imageCache_;
};
```

## 7. 调试工具

### 7.1 日志系统
```cpp
class Logger : public QObject {
    Q_OBJECT
public:
    static Logger& instance();
    
    void logDebug(const QString& message);
    void logInfo(const QString& message);
    void logError(const QString& message);
    
private:
    QFile logFile_;
    QTextStream logStream_;
};
```

### 7.2 调试窗口
```cpp
class DebugWindow : public QWidget {
    Q_OBJECT
public:
    void showNetworkStatus();
    void showMemoryUsage();
    void showMessageStats();
    
private:
    QTableWidget* statsTable_;
    QTimer* updateTimer_;
};
```

这个设计包含了：
1. 清晰的项目结构
2. 现代化的 UI 设计
3. 完整的功能实现
4. 性能优化考虑
5. 调试工具支持

如果您需要某个部分的更详细设计或有其他需求，请告诉我。 