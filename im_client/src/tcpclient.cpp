/**
 * @file tcpclient.cpp
 * @brief TCP 客户端实现
 */

#include "tcpclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QApplication>
#include <QThread>
#include <QSettings>

TcpClient::TcpClient(QObject* parent)
    : QObject(parent)
    , socket_(std::make_unique<QTcpSocket>(this))
    , heartbeat_timer_(new QTimer(this))
    , reconnect_timer_(new QTimer(this))
{
    // 连接信号槽
    connect(socket_.get(), &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(socket_.get(), &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(socket_.get(), &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(socket_.get(), &QAbstractSocket::errorOccurred, this, &TcpClient::onError);

    // 心跳定时器
    connect(heartbeat_timer_, &QTimer::timeout, this, &TcpClient::sendHeartbeat);

    // 重连定时器
    connect(reconnect_timer_, &QTimer::timeout, this, &TcpClient::attemptReconnect);
}

void TcpClient::loadCredentials() {
    QSettings settings("IMClient", "TcpClient");
    user_id_ = settings.value("user_id", "").toString();
    user_nickname_ = settings.value("user_nickname", "").toString();
    token_ = settings.value("token", "").toString();

    if (!user_id_.isEmpty()) {
        qDebug() << "Loaded credentials for user:" << user_id_;
    }
}

void TcpClient::saveCredentials() {
    QSettings settings("IMClient", "TcpClient");
    settings.setValue("user_id", user_id_);
    settings.setValue("user_nickname", user_nickname_);
    settings.setValue("token", token_);
    qDebug() << "Saved credentials for user:" << user_id_;
}

TcpClient::~TcpClient() {
    stopHeartbeat();
    reconnect_timer_->stop();
    if (socket_->isOpen()) {
        socket_->disconnectFromHost();
    }
}

void TcpClient::connectToServer(const QString& host, quint16 port) {
    // 如果已经在连接中，先断开
    if (socket_->isOpen()) {
        socket_->disconnectFromHost();
    }

    // 保存服务器地址
    server_host_ = host;
    server_port_ = port;

    // 停止重连
    reconnect_timer_->stop();
    reconnect_attempts_ = 0;

    state_ = ClientState::Connecting;
    socket_->connectToHost(QHostAddress(host), port);

    // 设置超时
    QTimer::singleShot(5000, this, [this]() {
        if (state_ == ClientState::Connecting) {
            socket_->disconnectFromHost();
            state_ = ClientState::Disconnected;
            emit connectionError("连接超时");
        }
    });
}

void TcpClient::disconnectFromServer() {
    stopHeartbeat();
    reconnect_timer_->stop();
    reconnect_attempts_ = 0;
    if (socket_->isOpen()) {
        socket_->disconnectFromHost();
    }
    state_ = ClientState::Disconnected;
    user_id_.clear();
    user_nickname_.clear();
    token_.clear();
}

void TcpClient::sendMessage(MsgType type, const QString& body) {
    qDebug() << "sendMessage called, state:" << socket_->state();

    // 检查 socket 是否已连接
    if (socket_->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Socket not connected, cannot send message, state:" << socket_->state();
        // 如果还没在重连，触发重连
        if (state_ != ClientState::Connecting && reconnect_attempts_ == 0) {
            attemptReconnect();
        }
        return;
    }

    QByteArray data = Protocol::encode(type, body);
    qDebug() << "Sending" << data.size() << "bytes, type:" << static_cast<int>(type);

    qint64 written = socket_->write(data);
    qDebug() << "write() returned:" << written;

    // 确保数据真正发送出去
    socket_->flush();

    // 检查还有多少数据在缓冲区
    qDebug() << "bytesToWrite after flush:" << socket_->bytesToWrite();

    // 如果数据没有完全发送出去，等待
    if (socket_->bytesToWrite() > 0) {
        qDebug() << "Waiting for data to be sent...";
        QThread::msleep(50);  // 等待 50ms
        qDebug() << "bytesToWrite after wait:" << socket_->bytesToWrite();
    }
}

void TcpClient::login(const QString& user_id, const QString& password) {
    // 如果正在连接中，等待连接完成后再登录
    if (state_ == ClientState::Connecting) {
        qDebug() << "Still connecting, waiting...";
        // 保存登录信息，连接成功后自动登录
        pending_login_user_id_ = user_id;
        pending_login_password_ = password;
        return;
    }

    // 如果未连接，触发重连
    if (state_ == ClientState::Disconnected) {
        qDebug() << "Disconnected, attempting reconnect first...";
        pending_login_user_id_ = user_id;
        pending_login_password_ = password;
        attemptReconnect();
        return;
    }

    QString body = Protocol::makeLoginRequest(user_id, password);
    sendMessage(MsgType::LOGIN, body);
}

void TcpClient::registerUser(const QString& phone, const QString& nickname, const QString& password) {
    QJsonObject obj;
    obj["phone"] = phone;
    obj["nickname"] = nickname;
    obj["password"] = password;
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::REGISTER_REQ, body);
}

void TcpClient::sendChatMessage(const QString& to_user_id,
                               const QString& content_type,
                               const QString& content) {
    if (state_ != ClientState::LoggedIn) {
        return;
    }

    QString body = Protocol::makeChatMessage(user_id_, to_user_id, content_type, content);
    sendMessage(MsgType::TEXT, body);
}

void TcpClient::onConnected() {
    qDebug() << "Connected to server successfully";
    reconnect_attempts_ = 0;
    reconnect_timer_->stop();
    state_ = ClientState::Connected;
    startHeartbeat();
    emit connected();

    // 如果有待发送的登录请求，自动发送
    if (!pending_login_user_id_.isEmpty()) {
        qDebug() << "Sending pending login for:" << pending_login_user_id_;
        QString body = Protocol::makeLoginRequest(pending_login_user_id_, pending_login_password_);
        sendMessage(MsgType::LOGIN, body);
        pending_login_user_id_.clear();
        pending_login_password_.clear();
    }
}

void TcpClient::onDisconnected() {
    qDebug() << "Disconnected from server";
    stopHeartbeat();
    reconnect_timer_->stop();
    state_ = ClientState::Disconnected;
    read_buffer_.clear();
    emit disconnected();
}

void TcpClient::onReadyRead() {
    read_buffer_.append(socket_->readAll());

    // 循环解析消息（处理粘包）
    while (true) {
        MsgType type;
        QString body;

        if (!Protocol::decode(read_buffer_, type, body)) {
            break;  // 数据不足，等待更多数据
        }

        // 处理消息
        handleMessage(type, body);
    }
}

void TcpClient::onError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    QString error_string = socket_->errorString();
    qWarning() << "Socket error:" << error_string;
    emit connectionError(error_string);
}

void TcpClient::sendHeartbeat() {
    if (state_ != ClientState::Connected && state_ != ClientState::LoggedIn) {
        return;
    }

    QJsonObject obj;
    obj["user_id"] = user_id_;
    obj["timestamp"] = QDateTime::currentSecsSinceEpoch();
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::HEARTBEAT, body);
}

void TcpClient::handleMessage(MsgType type, const QString& body) {
    switch (type) {
        case MsgType::LOGIN_RSP: {
            LoginResponse rsp;
            if (Protocol::parseLoginResponse(body, rsp)) {
                if (rsp.code == 0) {
                    state_ = ClientState::LoggedIn;
                    user_id_ = QString::fromStdString(rsp.user_id);
                    user_nickname_ = QString::fromStdString(rsp.nickname);
                    token_ = QString::fromStdString(rsp.token);
                    // 保存登录凭证
                    saveCredentials();
                }
                emit loginResponse(rsp.code, QString::fromStdString(rsp.message),
                                   QString::fromStdString(rsp.user_id),
                                   QString::fromStdString(rsp.nickname),
                                   QString::fromStdString(rsp.token));
            }
            break;
        }

        case MsgType::REGISTER_RSP: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                int code = obj["code"].toInt();
                QString message = obj["message"].toString();
                QString user_id = obj["user_id"].toString();
                emit registerResponse(code, message, user_id);
            }
            break;
        }

        case MsgType::TEXT:
        case MsgType::IMAGE:
        case MsgType::FILE:
        case MsgType::VOICE: {
            // 解析聊天消息
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString from_user_id = obj["from_user_id"].toString();
                QString content = obj["content"].toString();
                emit chatMessageReceived(from_user_id, content);
            }
            break;
        }

        case MsgType::HEARTBEAT: {
            emit heartbeatResponse();
            break;
        }

        default:
            qDebug() << "Unknown message type:" << static_cast<quint16>(type);
            break;
    }
}

void TcpClient::startHeartbeat() {
    heartbeat_timer_->start(HEARTBEAT_INTERVAL);
}

void TcpClient::stopHeartbeat() {
    heartbeat_timer_->stop();
}

void TcpClient::attemptReconnect() {
    if (server_host_.isEmpty() || server_port_ == 0) {
        qDebug() << "No server info saved, cannot reconnect";
        return;
    }

    if (reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
        qDebug() << "Max reconnection attempts reached, giving up";
        emit connectionError("连接断开，请手动重试");
        reconnect_attempts_ = 0;
        reconnect_timer_->stop();
        return;
    }

    // 如果已经在重连中，不要重复启动
    if (state_ == ClientState::Connecting) {
        qDebug() << "Already connecting, skipping reconnect attempt";
        return;
    }

    reconnect_attempts_++;
    qDebug() << "Attempting to reconnect, attempt" << reconnect_attempts_
             << "of" << MAX_RECONNECT_ATTEMPTS;

    state_ = ClientState::Connecting;
    socket_->connectToHost(QHostAddress(server_host_), server_port_);

    // 设置超时
    QTimer::singleShot(5000, this, [this]() {
        if (state_ == ClientState::Connecting) {
            socket_->disconnectFromHost();
            qDebug() << "Reconnect attempt timeout";
            // 重连定时器：如果还没达到最大次数，继续尝试
            if (reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS) {
                reconnect_timer_->start(RECONNECT_INTERVAL);
            } else {
                emit connectionError("连接失败，请手动重试");
                reconnect_attempts_ = 0;
            }
        }
    });
}
