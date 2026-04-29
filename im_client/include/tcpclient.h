/**
 * @file tcpclient.h
 * @brief TCP 客户端封装
 */

#pragma once

#include "protocol.h"
#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <memory>

/**
 * @brief TCP 客户端状态
 */
enum class ClientState {
    Disconnected,
    Connecting,
    Connected,
    LoggedIn
};

/**
 * @brief TCP 客户端类
 *
 * 负责：
 * 1. 管理与服务器的 TCP 连接
 * 2. 消息的发送和接收
 * 3. 心跳保活
 * 4. 重连机制
 */
class TcpClient : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit TcpClient(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~TcpClient();

    /**
     * @brief 连接到服务器
     * @param host 服务器地址
     * @param port 服务器端口
     */
    void connectToServer(const QString& host, quint16 port);

    /**
     * @brief 断开连接
     */
    void disconnectFromServer();

    /**
     * @brief 发送消息
     * @param type 消息类型
     * @param body 消息体
     */
    void sendMessage(MsgType type, const QString& body);

    /**
     * @brief 发送登录请求
     * @param user_id 用户ID
     * @param password 密码
     */
    void login(const QString& user_id, const QString& password);

    /**
     * @brief 发送注册请求
     * @param phone 手机号
     * @param nickname 昵称
     * @param password 密码
     */
    void registerUser(const QString& phone, const QString& nickname, const QString& password);

    /**
     * @brief 发送聊天消息
     * @param to_user_id 接收者ID
     * @param content_type 内容类型
     * @param content 内容
     */
    void sendChatMessage(const QString& to_user_id,
                         const QString& content_type,
                         const QString& content);

    /**
     * @brief 发送好友请求
     * @param phone 目标用户手机号
     * @param remark 备注信息
     */
    void sendFriendRequest(const QString& to_user_id, const QString& remark);

    /**
     * @brief 获取好友列表
     */
    void getFriendList();

    /**
     * @brief 获取好友请求列表
     */
    void getFriendRequests();

    /**
     * @brief 响应好友请求
     * @param request_id 请求ID
     * @param accept 是否接受
     */
    void respondFriendRequest(const QString& request_id, bool accept);

    /**
     * @brief 获取聊天记录
     * @param friend_id 好友ID
     * @param limit 消息数量
     * @param before_time 时间戳（之前的消息）
     */
    void getChatHistory(const QString& friend_id, int limit = 20, int64_t before_time = 0);

    /**
     * @brief 获取当前状态
     */
    ClientState state() const { return state_; }

    /**
     * @brief 获取用户ID
     */
    const QString& userId() const { return user_id_; }

    /**
     * @brief 是否有保存的登录凭证
     */
    bool hasSavedCredentials() const { return !user_id_.isEmpty(); }

    /**
     * @brief 获取保存的用户ID
     */
    const QString& savedUserId() const { return user_id_; }

    /**
     * @brief 加载本地保存的登录凭证
     */
    void loadCredentials();

signals:
    /**
     * @brief 连接成功信号
     */
    void connected();

    /**
     * @brief 连接断开信号
     */
    void disconnected();

    /**
     * @brief 连接错误信号
     * @param error_string 错误信息
     */
    void connectionError(const QString& error_string);

    /**
     * @brief 登录响应信号
     * @param code 返回码
     * @param message 消息
     * @param user_id 用户ID
     * @param nickname 昵称
     * @param token Token
     */
    void loginResponse(int code, const QString& message,
                       const QString& user_id, const QString& nickname,
                       const QString& token);

    /**
     * @brief 注册响应信号
     */
    void registerResponse(int code, const QString& message, const QString& user_id);

    /**
     * @brief 收到聊天消息信号
     * @param from_user_id 发送者
     * @param content 内容
     */
    void chatMessageReceived(const QString& from_user_id, const QString& content);

    /**
     * @brief 心跳响应信号
     */
    void heartbeatResponse();

    /**
     * @brief 连接状态变化信号
     * @param is_connected 是否已连接
     */
    void connectionStatusChanged(bool is_connected);

    /**
     * @brief 收到新好友请求信号
     */
    void friendRequestReceived(const QString& from_user_id, const QString& from_nickname, const QString& message);

    /**
     * @brief 好友请求结果信号
     */
    void friendRequestResult(int code, const QString& message);

    /**
     * @brief 好友列表信号
     */
    void friendListReceived(const QString& friend_list_json);

    /**
     * @brief 好友请求列表信号
     */
    void friendRequestsReceived(const QString& requests_json);

    /**
     * @brief 聊天记录信号
     */
    void chatHistoryReceived(const QString& friend_id, const QString& history_json);

    /**
     * @brief 收到离线消息信号
     */
    void offlineMessageReceived(const QString& from_user_id, const QString& content);

private slots:
    /**
     * @brief 处理连接成功
     */
    void onConnected();

    /**
     * @brief 处理断开连接
     */
    void onDisconnected();

    /**
     * @brief 处理读取数据
     */
    void onReadyRead();

    /**
     * @brief 处理socket错误
     */
    void onError(QAbstractSocket::SocketError error);

    /**
     * @brief 发送心跳
     */
    void sendHeartbeat();

    /**
     * @brief 处理心跳超时
     */
    void onHeartbeatTimeout();

private:
    /**
     * @brief 处理接收到的消息
     */
    void handleMessage(MsgType type, const QString& body);

    /**
     * @brief 启动心跳
     */
    void startHeartbeat();

    /**
     * @brief 停止心跳
     */
    void stopHeartbeat();

    /**
     * @brief 尝试重连
     */
    void attemptReconnect();

    /**
     * @brief 保存登录凭证到本地
     */
    void saveCredentials();

    bool expecting_friend_requests_ = false;
    QString current_chat_history_friend_id_;

    std::unique_ptr<QTcpSocket> socket_;
    ClientState state_ = ClientState::Disconnected;
    QString user_id_;
    QString user_nickname_;
    QString token_;
    QString server_host_;
    quint16 server_port_;
    QString pending_login_user_id_;
    QString pending_login_password_;

    QTimer* heartbeat_timer_ = nullptr;
    QTimer* heartbeat_timeout_timer_ = nullptr;
    QTimer* reconnect_timer_ = nullptr;
    QByteArray read_buffer_;

    int reconnect_attempts_ = 0;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
    static constexpr int RECONNECT_INTERVAL = 3000;
    static constexpr int HEARTBEAT_INTERVAL = 15000;
    static constexpr int HEARTBEAT_TIMEOUT = 10000;
};
