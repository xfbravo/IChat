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
     * @brief 获取当前状态
     */
    ClientState state() const { return state_; }

    /**
     * @brief 获取用户ID
     */
    const QString& userId() const { return user_id_; }

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

private:
    std::unique_ptr<QTcpSocket> socket_;  // TCP socket
    ClientState state_ = ClientState::Disconnected;  // 状态
    QString user_id_;  // 当前用户ID
    QString user_nickname_;  // 当前用户昵称
    QString token_;  // 认证Token
    QString server_host_;  // 服务器地址
    quint16 server_port_;  // 服务器端口

    QTimer* heartbeat_timer_ = nullptr;  // 心跳定时器
    QTimer* reconnect_timer_ = nullptr;  // 重连定时器
    QByteArray read_buffer_;  // 读取缓冲区

    int reconnect_attempts_ = 0;  // 重连次数
    static constexpr int MAX_RECONNECT_ATTEMPTS = 3;  // 最大重连次数
    static constexpr int RECONNECT_INTERVAL = 5000;   // 重连间隔（毫秒）
    static constexpr int HEARTBEAT_INTERVAL = 30000;  // 心跳间隔（毫秒）
};
