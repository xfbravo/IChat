/**
 * @file tcpclient.cpp
 * @brief TCP 客户端实现
 */

#include "tcpclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>
#include <QApplication>
#include <QThread>
#include <QSettings>

TcpClient::TcpClient(QObject* parent)
    : QObject(parent)
    , socket_(std::make_unique<QTcpSocket>(this))
    , heartbeat_timer_(new QTimer(this))
    , heartbeat_timeout_timer_(new QTimer(this))
    , reconnect_timer_(new QTimer(this))
{
    // 连接信号槽
    connect(socket_.get(), &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(socket_.get(), &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(socket_.get(), &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(socket_.get(), &QAbstractSocket::errorOccurred, this, &TcpClient::onError);

    // 心跳定时器 - 定期发送心跳
    connect(heartbeat_timer_, &QTimer::timeout, this, &TcpClient::sendHeartbeat);

    // 心跳超时定时器 - 如果服务器没响应就重连
    connect(heartbeat_timeout_timer_, &QTimer::timeout, this, &TcpClient::onHeartbeatTimeout);

    // 重连定时器
    connect(reconnect_timer_, &QTimer::timeout, this, &TcpClient::attemptReconnect);
}

void TcpClient::loadCredentials() {
    QSettings settings("IMClient", "TcpClient");
    user_id_ = settings.value("user_id", "").toString();
    user_nickname_ = settings.value("user_nickname", "").toString();
    user_avatar_url_ = settings.value("avatar_url", "").toString();
    user_gender_ = settings.value("gender", "").toString();
    user_region_ = settings.value("region", "").toString();
    user_signature_ = settings.value("signature", "").toString();
    token_ = settings.value("token", "").toString();

    if (!user_id_.isEmpty()) {
        qDebug() << "Loaded credentials for user:" << user_id_;
    }
}

void TcpClient::saveCredentials() {
    QSettings settings("IMClient", "TcpClient");
    settings.setValue("user_id", user_id_);
    settings.setValue("user_nickname", user_nickname_);
    settings.setValue("avatar_url", user_avatar_url_);
    settings.setValue("gender", user_gender_);
    settings.setValue("region", user_region_);
    settings.setValue("signature", user_signature_);
    settings.setValue("token", token_);
    qDebug() << "Saved credentials for user:" << user_id_;
}

TcpClient::~TcpClient() {
    stopHeartbeat();
    heartbeat_timeout_timer_->stop();
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

    // 停止所有定时器
    reconnect_timer_->stop();
    heartbeat_timer_->stop();
    heartbeat_timeout_timer_->stop();
    reconnect_attempts_ = 0;

    state_ = ClientState::Connecting;
    socket_->connectToHost(QHostAddress(host), port);
    emit connectionStatusChanged(false);

    // 设置连接超时
    QTimer::singleShot(5000, this, [this]() {
        if (state_ == ClientState::Connecting) {
            socket_->disconnectFromHost();
            state_ = ClientState::Disconnected;
            emit connectionError("连接超时");
            emit connectionStatusChanged(false);
        }
    });
}

void TcpClient::disconnectFromServer() {
    stopHeartbeat();
    heartbeat_timeout_timer_->stop();
    reconnect_timer_->stop();
    reconnect_attempts_ = 0;
    if (socket_->isOpen()) {
        socket_->disconnectFromHost();
    }
    state_ = ClientState::Disconnected;
    emit connectionStatusChanged(false);
}

void TcpClient::sendMessage(MsgType type, const QString& body) {
    // 检查 socket 是否已连接
    if (socket_->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Socket not connected, attempting reconnect, state:" << socket_->state();
        if (state_ != ClientState::Connecting && reconnect_attempts_ == 0) {
            attemptReconnect();
        }
        return;
    }

    QByteArray data = Protocol::encode(type, body);
    qint64 written = socket_->write(data);
    socket_->flush();

    if (socket_->bytesToWrite() > 0) {
        QThread::msleep(50);
    }
}

void TcpClient::login(const QString& user_id, const QString& password) {
    // 如果正在连接中，等待连接完成后再登录
    if (state_ == ClientState::Connecting) {
        qDebug() << "Still connecting, waiting...";
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

QString TcpClient::sendChatMessage(const QString& to_user_id,
                                  const QString& content_type,
                                  const QString& content) {
    if (state_ != ClientState::LoggedIn) {
        return QString();
    }

    QString msg_id = Protocol::generateMsgId();
    QString body = Protocol::makeChatMessage(msg_id, user_id_, to_user_id, content_type, content);
    sendMessage(MsgType::CHAT_MESSAGE, body);
    return msg_id;
}

void TcpClient::ackOfflineMessage(const QString& msg_id) {
    if (state_ != ClientState::LoggedIn || msg_id.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["msg_id"] = msg_id;
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::OFFLINE_MESSAGE_ACK, body);
}

void TcpClient::sendFriendRequest(const QString& phone, const QString& remark) {
    if (state_ != ClientState::LoggedIn) {
        return;
    }

    QJsonObject obj;
    obj["phone"] = phone;
    obj["remark"] = remark;
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::FRIEND_REQUEST, body);
}

void TcpClient::getFriendList() {
    if (state_ != ClientState::LoggedIn) {
        return;
    }
    sendMessage(MsgType::GET_FRIEND_LIST, "{}");
}

void TcpClient::getFriendRequests() {
    if (state_ != ClientState::LoggedIn) {
        return;
    }
    expecting_friend_requests_ = true;
    sendMessage(MsgType::GET_FRIEND_REQUESTS, "{}");
}

void TcpClient::respondFriendRequest(const QString& request_id, bool accept) {
    if (state_ != ClientState::LoggedIn) {
        return;
    }

    QJsonObject obj;
    obj["request_id"] = request_id;
    obj["accept"] = accept;
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::FRIEND_REQUEST_RSP, body);
}

void TcpClient::updateFriendRemark(const QString& friend_id, const QString& remark) {
    if (state_ != ClientState::LoggedIn || friend_id.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["friend_id"] = friend_id;
    obj["remark"] = remark;
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::UPDATE_FRIEND_REMARK, body);
}

void TcpClient::updateAvatar(const QString& avatar_url) {
    if (state_ != ClientState::LoggedIn || avatar_url.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["avatar_url"] = avatar_url;
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::UPDATE_AVATAR, body);
}

void TcpClient::updateProfile(const QString& nickname,
                              const QString& gender,
                              const QString& region,
                              const QString& signature) {
    if (state_ != ClientState::LoggedIn) {
        return;
    }

    QJsonObject obj;
    obj["nickname"] = nickname;
    obj["gender"] = gender;
    obj["region"] = region;
    obj["signature"] = signature;
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::UPDATE_PROFILE, body);
}

void TcpClient::changePassword(const QString& old_password, const QString& new_password) {
    if (state_ != ClientState::LoggedIn) {
        return;
    }

    QJsonObject obj;
    obj["old_password"] = old_password;
    obj["new_password"] = new_password;
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::CHANGE_PASSWORD, body);
}

void TcpClient::getChatHistory(const QString& friend_id, int limit, int64_t before_time) {
    if (state_ != ClientState::LoggedIn) {
        return;
    }

    current_chat_history_friend_id_ = friend_id;

    QJsonObject obj;
    obj["friend_id"] = friend_id;
    obj["limit"] = limit;
    if (before_time > 0) {
        obj["before_time"] = before_time;
    }
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::GET_CHAT_HISTORY, body);
}

void TcpClient::onConnected() {
    qDebug() << "Connected to server successfully";
    reconnect_attempts_ = 0;
    reconnect_timer_->stop();
    state_ = ClientState::Connected;
    emit connected();
    emit connectionStatusChanged(true);
    startHeartbeat();

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
    heartbeat_timeout_timer_->stop();
    reconnect_timer_->stop();
    state_ = ClientState::Disconnected;
    read_buffer_.clear();
    emit disconnected();
    emit connectionStatusChanged(false);

    // 自动重连
    if (reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS) {
        QTimer::singleShot(RECONNECT_INTERVAL, this, &TcpClient::attemptReconnect);
    }
}

void TcpClient::onReadyRead() {
    read_buffer_.append(socket_->readAll());

    // 循环解析消息（处理粘包）
    while (true) {
        MsgType type;
        QString body;

        if (!Protocol::decode(read_buffer_, type, body)) {
            break;
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
    emit connectionStatusChanged(false);
}

void TcpClient::sendHeartbeat() {
    if (state_ != ClientState::Connected && state_ != ClientState::LoggedIn) {
        return;
    }

    // 重置心跳超时定时器
    heartbeat_timeout_timer_->start(HEARTBEAT_TIMEOUT);

    QJsonObject obj;
    obj["user_id"] = user_id_;
    obj["timestamp"] = QDateTime::currentSecsSinceEpoch();
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::HEARTBEAT, body);
}

void TcpClient::onHeartbeatTimeout() {
    qWarning() << "Heartbeat timeout, server not responding, reconnecting...";
    heartbeat_timeout_timer_->stop();
    stopHeartbeat();

    // 断开并重连
    if (socket_->isOpen()) {
        socket_->disconnectFromHost();
    }

    if (reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS) {
        attemptReconnect();
    }
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
                    user_avatar_url_ = QString::fromStdString(rsp.avatar_url);
                    user_gender_ = QString::fromStdString(rsp.gender);
                    user_region_ = QString::fromStdString(rsp.region);
                    user_signature_ = QString::fromStdString(rsp.signature);
                    token_ = QString::fromStdString(rsp.token);
                    saveCredentials();
                }
                emit loginResponse(rsp.code, QString::fromStdString(rsp.message),
                                   QString::fromStdString(rsp.user_id),
                                   QString::fromStdString(rsp.nickname),
                                   QString::fromStdString(rsp.avatar_url),
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

        case MsgType::CHAT_MESSAGE:
        case MsgType::IMAGE:
        case MsgType::FILE:
        case MsgType::VOICE: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString from_user_id = obj["from_user_id"].toString();
                QString content = obj["content"].toString();
                QString msg_id = obj["msg_id"].toString();
                qint64 server_timestamp = obj["server_timestamp"].toInteger();
                QString server_time = obj["server_time"].toString();
                emit chatMessageReceived(from_user_id, content, msg_id,
                                         server_timestamp, server_time);
            }
            break;
        }

        case MsgType::HEARTBEAT: {
            // 收到心跳响应，重置超时定时器
            heartbeat_timeout_timer_->stop();
            emit heartbeatResponse();
            break;
        }

        case MsgType::ACK: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                emit messageAckReceived(obj["msg_id"].toString(),
                                        obj["status"].toString(),
                                        obj["code"].toInt(),
                                        obj["message"].toString());
            }
            break;
        }

        case MsgType::FRIEND_LIST_RSP: {
            emit friendListReceived(body);
            break;
        }

        case MsgType::FRIEND_REQUEST_NEW: {
            // 收到好友请求列表
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isArray()) {
                QJsonArray arr = doc.array();
                // 发出原始JSON信号（用于显示请求列表）
                emit friendRequestsReceived(body);
                // 只有在不是主动查询时，才弹出通知
                if (!expecting_friend_requests_ && !arr.isEmpty()) {
                    for (const QJsonValue& value : arr) {
                        QJsonObject obj = value.toObject();
                        QString from_user_id = obj["from_user_id"].toString();
                        QString from_nickname = obj["from_nickname"].toString();
                        QString remark = obj["remark"].toString();
                        emit friendRequestReceived(from_user_id, from_nickname, remark);
                    }
                }
                expecting_friend_requests_ = false;
            }
            break;
        }

        case MsgType::FRIEND_REQUEST_RSP: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                int code = obj["code"].toInt();
                QString message = obj["message"].toString();
                emit friendRequestResult(code, message);
            }
            break;
        }

        case MsgType::UPDATE_FRIEND_REMARK_RSP: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                emit friendRemarkUpdateResult(obj["code"].toInt(),
                                              obj["message"].toString(),
                                              obj["friend_id"].toString(),
                                              obj["remark"].toString());
            }
            break;
        }

        case MsgType::UPDATE_AVATAR_RSP: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                const int code = obj["code"].toInt();
                const QString avatar_url = obj["avatar_url"].toString();
                if (code == 0 && !avatar_url.isEmpty()) {
                    user_avatar_url_ = avatar_url;
                    saveCredentials();
                }
                emit avatarUpdateResult(code,
                                        obj["message"].toString(),
                                        avatar_url);
            }
            break;
        }

        case MsgType::UPDATE_PROFILE_RSP: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                const int code = obj["code"].toInt();
                const QString nickname = obj["nickname"].toString();
                const QString gender = obj["gender"].toString();
                const QString region = obj["region"].toString();
                const QString signature = obj["signature"].toString();
                if (code == 0) {
                    if (!nickname.isEmpty()) {
                        user_nickname_ = nickname;
                    }
                    user_gender_ = gender;
                    user_region_ = region;
                    user_signature_ = signature;
                    saveCredentials();
                }
                emit profileUpdateResult(code,
                                         obj["message"].toString(),
                                         nickname,
                                         gender,
                                         region,
                                         signature);
            }
            break;
        }

        case MsgType::CHANGE_PASSWORD_RSP: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                emit passwordChangeResult(obj["code"].toInt(),
                                          obj["message"].toString());
            }
            break;
        }

        case MsgType::CHAT_HISTORY_RSP: {
            emit chatHistoryReceived(current_chat_history_friend_id_, body);
            break;
        }

        case MsgType::OFFLINE_MESSAGE: {
            // 收到离线消息，格式是消息数组
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isArray()) {
                QJsonArray messages = doc.array();
                for (const QJsonValue& value : messages) {
                    QJsonObject obj = value.toObject();
                    QString from_user_id = obj["from_user_id"].toString();
                    QString content = obj["content"].toString();
                    QString msg_id = obj["msg_id"].toString();
                    QString server_time = obj["server_time"].toString();
                    qint64 server_timestamp = obj["server_timestamp"].toInteger();
                    emit offlineMessageReceived(from_user_id, content, msg_id,
                                                server_timestamp, server_time);
                    ackOfflineMessage(msg_id);
                }
            }
            break;
        }

        case MsgType::FRIEND_LIST_UPDATE: {
            // 好友列表更新通知
            emit friendListReceived(body);
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
    heartbeat_timeout_timer_->stop();
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
        return;
    }

    if (state_ == ClientState::Connecting) {
        qDebug() << "Already connecting, skipping reconnect attempt";
        return;
    }

    reconnect_attempts_++;
    qDebug() << "Attempting to reconnect, attempt" << reconnect_attempts_
             << "of" << MAX_RECONNECT_ATTEMPTS;

    state_ = ClientState::Connecting;
    socket_->connectToHost(QHostAddress(server_host_), server_port_);

    // 连接超时处理
    QTimer::singleShot(5000, this, [this]() {
        if (state_ == ClientState::Connecting) {
            socket_->disconnectFromHost();
            qDebug() << "Reconnect attempt timeout";
            if (reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS) {
                reconnect_timer_->start(RECONNECT_INTERVAL);
            } else {
                emit connectionError("连接失败，请手动重试");
                reconnect_attempts_ = 0;
                state_ = ClientState::Disconnected;
            }
        }
    });
}
