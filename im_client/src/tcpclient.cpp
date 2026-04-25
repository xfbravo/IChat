/**
 * @file tcpclient.cpp
 * @brief TCP 客户端实现
 */

#include "tcpclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QApplication>

TcpClient::TcpClient(QObject* parent)
    : QObject(parent)
    , socket_(std::make_unique<QTcpSocket>(this))
    , heartbeat_timer_(new QTimer(this))
{
    // 连接信号槽
    connect(socket_.get(), &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(socket_.get(), &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(socket_.get(), &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(socket_.get(), &QAbstractSocket::errorOccurred, this, &TcpClient::onError);

    // 心跳定时器
    connect(heartbeat_timer_, &QTimer::timeout, this, &TcpClient::sendHeartbeat);
}

TcpClient::~TcpClient() {
    stopHeartbeat();
    if (socket_->isOpen()) {
        socket_->disconnectFromHost();
    }
}

void TcpClient::connectToServer(const QString& host, quint16 port) {
    // 如果已经在连接中，先断开
    if (socket_->isOpen()) {
        socket_->disconnectFromHost();
    }
    state_ = ClientState::Disconnected;

    state_ = ClientState::Connecting;
    socket_->connectToHost(QHostAddress(host), port);

    // 设置超时
    QTimer::singleShot(5000, this, [this]() {
        if (state_ == ClientState::Connecting) {
            socket_->disconnectFromHost();
            emit connectionError("连接超时");
        }
    });
}

void TcpClient::disconnectFromServer() {
    stopHeartbeat();
    if (socket_->isOpen()) {
        socket_->disconnectFromHost();
    }
    state_ = ClientState::Disconnected;
    user_id_.clear();
    user_nickname_.clear();
    token_.clear();
}

void TcpClient::sendMessage(MsgType type, const QString& body) {
    // 检查 socket 是否已连接
    if (socket_->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Socket not connected, cannot send message";
        return;
    }

    QByteArray data = Protocol::encode(type, body);
    socket_->write(data);
    socket_->flush();
}

void TcpClient::login(const QString& user_id, const QString& password) {
    // 确保已连接
    if (state_ != ClientState::Connected && state_ != ClientState::LoggedIn) {
        qWarning() << "Not connected, cannot login";
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
    state_ = ClientState::Connected;
    startHeartbeat();
    emit connected();
}

void TcpClient::onDisconnected() {
    stopHeartbeat();
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
