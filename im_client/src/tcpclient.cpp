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
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QDir>
#include <QBuffer>
#include <QDateTime>
#include <QEventLoop>
#include <QImageReader>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

namespace {

QString imageToDataUrl(const QImage& source, const QSize& max_size, int quality = 76) {
    if (source.isNull()) {
        return QString();
    }

    QImage image = source;
    if (image.width() > max_size.width() || image.height() > max_size.height()) {
        image = image.scaled(max_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "JPEG", quality)) {
        return QString();
    }
    return QString("data:image/jpeg;base64,%1").arg(QString::fromLatin1(bytes.toBase64()));
}

} // namespace

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
                                  const QString& content,
                                  const QString& chat_type) {
    if (state_ != ClientState::LoggedIn) {
        return QString();
    }

    QString msg_id = Protocol::generateMsgId();
    QJsonObject obj;
    obj["msg_id"] = msg_id;
    obj["from_user_id"] = user_id_;
    obj["to_user_id"] = to_user_id;
    obj["chat_type"] = chat_type == "group" ? "group" : "p2p";
    obj["content_type"] = content_type;
    obj["content"] = content;
    obj["client_time"] = QDateTime::currentSecsSinceEpoch();
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::CHAT_MESSAGE, body);
    return msg_id;
}

QString TcpClient::makeImagePreviewDataUrl(const QString& file_path) const {
    QImageReader reader(file_path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    return imageToDataUrl(image, QSize(1280, 1280), 78);
}

QString TcpClient::makeVideoPosterDataUrl(const QString& file_path) const {
    QMediaPlayer player;
    QVideoSink video_sink;
    QEventLoop loop;
    QTimer timeout;
    QString poster_data_url;

    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(&player, &QMediaPlayer::errorOccurred, &loop,
            [&](QMediaPlayer::Error, const QString&) { loop.quit(); });
    connect(&video_sink, &QVideoSink::videoFrameChanged, &loop, [&](const QVideoFrame& frame) {
        QImage image = frame.toImage();
        if (!image.isNull()) {
            poster_data_url = imageToDataUrl(image, QSize(960, 540), 76);
            loop.quit();
        }
    });

    player.setVideoSink(&video_sink);
    player.setSource(QUrl::fromLocalFile(file_path));
    player.play();
    timeout.start(3000);
    loop.exec();
    player.stop();
    return poster_data_url;
}

void TcpClient::sendFiles(const QString& to_user_id,
                          const QStringList& file_paths,
                          const QString& chat_type) {
    if (state_ != ClientState::LoggedIn || to_user_id.isEmpty()) {
        return;
    }

    constexpr qint64 max_file_size = 200LL * 1024LL * 1024LL;
    constexpr qint64 chunk_size = 256LL * 1024LL;
    QMimeDatabase mime_database;

    for (const QString& file_path : file_paths) {
        QFileInfo info(file_path);
        if (!info.exists() || !info.isFile()) {
            emit fileTransferFinished(QString(), info.fileName(), QString(), true, false, "文件不存在");
            continue;
        }
        if (info.size() <= 0) {
            emit fileTransferFinished(QString(), info.fileName(), QString(), true, false, "不能发送空文件");
            continue;
        }
        if (info.size() > max_file_size) {
            emit fileTransferFinished(QString(), info.fileName(), QString(), true, false, "单个文件不能超过200MB");
            continue;
        }

        PendingUpload upload;
        upload.transfer_id = Protocol::generateMsgId();
        upload.to_user_id = to_user_id;
        upload.chat_type = chat_type == "group" ? "group" : "p2p";
        upload.file_path = info.absoluteFilePath();
        upload.file_name = info.fileName();
        upload.file_size = info.size();
        upload.total_chunks = static_cast<int>((upload.file_size + chunk_size - 1) / chunk_size);
        upload.mime_type = mime_database.mimeTypeForFile(info).name();
        if (upload.mime_type.isEmpty()) {
            upload.mime_type = "application/octet-stream";
        }
        if (upload.mime_type.startsWith("image/")) {
            upload.content_type = "image";
            upload.preview_data_url = makeImagePreviewDataUrl(upload.file_path);
        } else if (upload.mime_type.startsWith("video/")) {
            upload.content_type = "video";
            upload.poster_data_url = makeVideoPosterDataUrl(upload.file_path);
        }
        pending_uploads_[upload.transfer_id] = upload;

        QJsonObject obj;
        obj["transfer_id"] = upload.transfer_id;
        obj["to_user_id"] = to_user_id;
        obj["chat_type"] = upload.chat_type;
        obj["file_name"] = upload.file_name;
        obj["file_size"] = static_cast<double>(upload.file_size);
        obj["mime_type"] = upload.mime_type;
        obj["total_chunks"] = upload.total_chunks;
        sendMessage(MsgType::FILE_UPLOAD_START, QJsonDocument(obj).toJson(QJsonDocument::Compact));
        emit fileTransferProgress(upload.transfer_id, upload.file_name, 0, upload.file_size, true);
    }
}

void TcpClient::downloadFile(const QString& file_id, const QString& file_name, const QString& save_path) {
    if (state_ != ClientState::LoggedIn || file_id.isEmpty() || save_path.isEmpty()) {
        return;
    }

    QFile target(save_path);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit fileTransferFinished(QString(), file_name, save_path, false, false, "无法创建保存文件");
        return;
    }
    target.close();

    PendingDownload download;
    download.transfer_id = Protocol::generateMsgId();
    download.file_id = file_id;
    download.file_name = file_name;
    download.save_path = save_path;
    pending_downloads_[download.transfer_id] = download;

    QJsonObject obj;
    obj["transfer_id"] = download.transfer_id;
    obj["file_id"] = file_id;
    obj["file_name"] = file_name;
    sendMessage(MsgType::FILE_DOWNLOAD_REQ, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void TcpClient::sendNextFileChunk(const QString& transfer_id, int chunk_index) {
    auto it = pending_uploads_.find(transfer_id);
    if (it == pending_uploads_.end()) {
        return;
    }

    constexpr qint64 chunk_size = 256LL * 1024LL;
    PendingUpload& upload = it.value();
    QFile file(upload.file_path);
    if (!file.open(QIODevice::ReadOnly) || !file.seek(static_cast<qint64>(chunk_index) * chunk_size)) {
        emit fileTransferFinished(transfer_id, upload.file_name, QString(), true, false, "读取文件失败");
        pending_uploads_.erase(it);
        return;
    }

    QByteArray chunk = file.read(chunk_size);
    if (chunk.isEmpty() && chunk_index < upload.total_chunks) {
        emit fileTransferFinished(transfer_id, upload.file_name, QString(), true, false, "读取文件分片失败");
        pending_uploads_.erase(it);
        return;
    }

    QJsonObject obj;
    obj["transfer_id"] = transfer_id;
    obj["chunk_index"] = chunk_index;
    obj["data"] = QString::fromLatin1(chunk.toBase64());
    sendMessage(MsgType::FILE_UPLOAD_CHUNK, QJsonDocument(obj).toJson(QJsonDocument::Compact));

    const qint64 sent = qMin(upload.file_size, (static_cast<qint64>(chunk_index) + 1) * chunk_size);
    emit fileTransferProgress(transfer_id, upload.file_name, sent, upload.file_size, true);
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

void TcpClient::createGroup(const QString& group_name, const QStringList& member_ids) {
    if (state_ != ClientState::LoggedIn) {
        emit groupCreateResult(401, "未登录", QString(), QString(), QString(), 0);
        return;
    }

    QJsonArray members;
    for (const QString& member_id : member_ids) {
        if (!member_id.trimmed().isEmpty()) {
            members.append(member_id.trimmed());
        }
    }

    QJsonObject obj;
    obj["group_name"] = group_name;
    obj["member_ids"] = members;
    sendMessage(MsgType::CREATE_GROUP, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void TcpClient::getGroupList() {
    if (state_ != ClientState::LoggedIn) {
        emit groupListReceived("[]");
        return;
    }
    sendMessage(MsgType::GET_GROUP_LIST, "{}");
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

    pending_profile_update_ = true;
    pending_profile_nickname_ = nickname;
    pending_profile_gender_ = gender;
    pending_profile_region_ = region;
    pending_profile_signature_ = signature;

    QJsonObject obj;
    obj["nickname"] = nickname;
    obj["gender"] = gender;
    obj["region"] = region;
    obj["signature"] = signature;
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::UPDATE_PROFILE, body);
}

void TcpClient::getUserProfile(const QString& user_id) {
    if (state_ != ClientState::LoggedIn || user_id.isEmpty()) {
        return;
    }

    pending_user_profile_id_ = user_id;

    QJsonObject obj;
    obj["user_id"] = user_id;
    if (user_id == user_id_) {
        obj["client_user_id"] = user_id_;
        QJsonObject local_profile;
        local_profile["nickname"] = user_nickname_;
        local_profile["gender"] = user_gender_;
        local_profile["region"] = user_region_;
        local_profile["signature"] = user_signature_;
        obj["local_profile"] = local_profile;
    }
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::GET_USER_PROFILE, body);
}

void TcpClient::createMoment(const QString& content,
                             const QJsonArray& images) {
    if (state_ != ClientState::LoggedIn) {
        emit momentCreateResult(401, "未登录");
        return;
    }

    QJsonObject obj;
    obj["content"] = content;
    obj["images"] = images;
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::CREATE_MOMENT, body);
}

void TcpClient::getMoments(int limit, const QString& target_user_id) {
    if (state_ != ClientState::LoggedIn) {
        emit momentsReceived("[]");
        return;
    }

    QJsonObject obj;
    obj["limit"] = limit;
    if (!target_user_id.trimmed().isEmpty()) {
        obj["target_user_id"] = target_user_id.trimmed();
    }
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::GET_MOMENTS, body);
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

void TcpClient::getChatHistory(const QString& peer_id,
                               int limit,
                               int64_t before_time,
                               const QString& chat_type) {
    if (state_ != ClientState::LoggedIn) {
        return;
    }

    current_chat_history_peer_id_ = peer_id;
    current_chat_history_type_ = chat_type == "group" ? "group" : "p2p";

    QJsonObject obj;
    obj["friend_id"] = peer_id;
    obj["peer_id"] = peer_id;
    obj["chat_type"] = current_chat_history_type_;
    obj["limit"] = limit;
    if (before_time > 0) {
        obj["before_time"] = before_time;
    }
    QString body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    sendMessage(MsgType::GET_CHAT_HISTORY, body);
}

QString TcpClient::startCall(const QString& to_user_id,
                             const QString& call_type,
                             const QString& sdp,
                             const QString& fixed_call_id) {
    if (state_ != ClientState::LoggedIn || to_user_id.isEmpty()) {
        return QString();
    }

    const QString call_id = fixed_call_id.isEmpty() ? Protocol::generateMsgId() : fixed_call_id;
    QJsonObject obj;
    obj["call_id"] = call_id;
    obj["from_user_id"] = user_id_;
    obj["to_user_id"] = to_user_id;
    obj["call_type"] = call_type == "video" ? QStringLiteral("video") : QStringLiteral("audio");
    obj["timestamp"] = QDateTime::currentSecsSinceEpoch();
    if (!sdp.isEmpty()) {
        QJsonObject sdp_obj;
        sdp_obj["type"] = "offer";
        sdp_obj["sdp"] = sdp;
        obj["sdp"] = sdp_obj;
    }
    sendMessage(MsgType::CALL_INVITE, QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return call_id;
}

void TcpClient::acceptCall(const QString& call_id, const QString& to_user_id, const QString& sdp) {
    if (state_ != ClientState::LoggedIn || call_id.isEmpty() || to_user_id.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["call_id"] = call_id;
    obj["from_user_id"] = user_id_;
    obj["to_user_id"] = to_user_id;
    obj["timestamp"] = QDateTime::currentSecsSinceEpoch();
    if (!sdp.isEmpty()) {
        QJsonObject sdp_obj;
        sdp_obj["type"] = "answer";
        sdp_obj["sdp"] = sdp;
        obj["sdp"] = sdp_obj;
    }
    sendMessage(MsgType::CALL_ACCEPT, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void TcpClient::rejectCall(const QString& call_id, const QString& to_user_id, const QString& reason) {
    if (state_ != ClientState::LoggedIn || call_id.isEmpty() || to_user_id.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["call_id"] = call_id;
    obj["from_user_id"] = user_id_;
    obj["to_user_id"] = to_user_id;
    obj["reason"] = reason;
    obj["timestamp"] = QDateTime::currentSecsSinceEpoch();
    sendMessage(MsgType::CALL_REJECT, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void TcpClient::cancelCall(const QString& call_id, const QString& to_user_id, const QString& reason) {
    if (state_ != ClientState::LoggedIn || call_id.isEmpty() || to_user_id.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["call_id"] = call_id;
    obj["from_user_id"] = user_id_;
    obj["to_user_id"] = to_user_id;
    obj["reason"] = reason;
    obj["timestamp"] = QDateTime::currentSecsSinceEpoch();
    sendMessage(MsgType::CALL_CANCEL, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void TcpClient::hangupCall(const QString& call_id, const QString& to_user_id, const QString& reason) {
    if (state_ != ClientState::LoggedIn || call_id.isEmpty() || to_user_id.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["call_id"] = call_id;
    obj["from_user_id"] = user_id_;
    obj["to_user_id"] = to_user_id;
    obj["reason"] = reason;
    obj["timestamp"] = QDateTime::currentSecsSinceEpoch();
    sendMessage(MsgType::CALL_HANGUP, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void TcpClient::timeoutCall(const QString& call_id, const QString& to_user_id) {
    if (state_ != ClientState::LoggedIn || call_id.isEmpty() || to_user_id.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["call_id"] = call_id;
    obj["from_user_id"] = user_id_;
    obj["to_user_id"] = to_user_id;
    obj["reason"] = "呼叫超时";
    obj["timestamp"] = QDateTime::currentSecsSinceEpoch();
    sendMessage(MsgType::CALL_TIMEOUT, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void TcpClient::sendCallIce(const QString& call_id, const QString& to_user_id, const QJsonObject& candidate) {
    if (state_ != ClientState::LoggedIn || call_id.isEmpty() || to_user_id.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["call_id"] = call_id;
    obj["from_user_id"] = user_id_;
    obj["to_user_id"] = to_user_id;
    obj["candidate"] = candidate;
    obj["timestamp"] = QDateTime::currentSecsSinceEpoch();
    sendMessage(MsgType::CALL_ICE, QJsonDocument(obj).toJson(QJsonDocument::Compact));
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
                    const QString previous_user_id = user_id_;
                    const QString login_user_id = QString::fromStdString(rsp.user_id);
                    const bool same_saved_user = previous_user_id == login_user_id;
                    const QString login_gender = QString::fromStdString(rsp.gender);
                    const QString login_region = QString::fromStdString(rsp.region);
                    const QString login_signature = QString::fromStdString(rsp.signature);
                    state_ = ClientState::LoggedIn;
                    user_id_ = login_user_id;
                    user_nickname_ = QString::fromStdString(rsp.nickname);
                    user_avatar_url_ = QString::fromStdString(rsp.avatar_url);
                    user_gender_ = login_gender.isEmpty() && same_saved_user ? user_gender_ : login_gender;
                    user_region_ = login_region.isEmpty() && same_saved_user ? user_region_ : login_region;
                    user_signature_ = login_signature.isEmpty() && same_saved_user ? user_signature_ : login_signature;
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
                QString content_type = obj["content_type"].toString("text");
                QString msg_id = obj["msg_id"].toString();
                qint64 server_timestamp = obj["server_timestamp"].toInteger();
                QString server_time = obj["server_time"].toString();
                QString to_user_id = obj["to_user_id"].toString();
                QString chat_type = obj["chat_type"].toString("p2p");
                emit chatMessageReceived(from_user_id, content, content_type, msg_id,
                                         server_timestamp, server_time, to_user_id, chat_type);
            }
            break;
        }

        case MsgType::FILE_UPLOAD_RSP: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (!doc.isObject()) break;

            QJsonObject obj = doc.object();
            const int code = obj["code"].toInt();
            const QString transfer_id = obj["transfer_id"].toString();
            const QString status = obj["status"].toString();
            auto it = pending_uploads_.find(transfer_id);
            if (it == pending_uploads_.end()) break;

            if (code != 0) {
                emit fileTransferFinished(transfer_id, it->file_name, QString(), true, false,
                                          obj["message"].toString("上传失败"));
                pending_uploads_.erase(it);
                break;
            }

            if (status == "ready") {
                it->file_id = obj["file_id"].toString();
                sendNextFileChunk(transfer_id, obj["next_chunk_index"].toInt(0));
            } else if (status == "chunk") {
                sendNextFileChunk(transfer_id, obj["next_chunk_index"].toInt());
            } else if (status == "complete") {
                PendingUpload upload = it.value();
                upload.file_id = obj["file_id"].toString(upload.file_id);

                QJsonObject content;
                content["file_id"] = upload.file_id;
                content["file_name"] = upload.file_name;
                content["file_size"] = static_cast<double>(upload.file_size);
                content["mime_type"] = upload.mime_type;
                content["transfer_id"] = upload.transfer_id;
                if (upload.content_type == "image" && !upload.preview_data_url.isEmpty()) {
                    content["preview_data_url"] = upload.preview_data_url;
                } else if (upload.content_type == "video" && !upload.poster_data_url.isEmpty()) {
                    content["poster_data_url"] = upload.poster_data_url;
                }
                const QString content_json = QString::fromUtf8(
                    QJsonDocument(content).toJson(QJsonDocument::Compact));
                const QString msg_id = sendChatMessage(upload.to_user_id,
                                                       upload.content_type,
                                                       content_json,
                                                       upload.chat_type);
                if (msg_id.isEmpty()) {
                    emit fileTransferFinished(transfer_id, upload.file_name, QString(), true, false,
                                              "文件已上传，但消息发送失败");
                } else {
                    emit fileMessageSent(upload.to_user_id, upload.chat_type, upload.content_type, content_json, msg_id);
                    emit fileTransferFinished(transfer_id, upload.file_name, QString(), true, true, "上传完成");
                }
                pending_uploads_.erase(it);
            }
            break;
        }

        case MsgType::FILE_DOWNLOAD_RSP: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (!doc.isObject()) break;

            QJsonObject obj = doc.object();
            const QString transfer_id = obj["transfer_id"].toString();
            auto it = pending_downloads_.find(transfer_id);
            if (it == pending_downloads_.end()) break;

            const int code = obj["code"].toInt();
            const QString status = obj["status"].toString();
            if (code != 0) {
                emit fileTransferFinished(transfer_id, it->file_name, it->save_path, false, false,
                                          obj["message"].toString("下载失败"));
                pending_downloads_.erase(it);
                break;
            }

            if (status == "ready") {
                it->file_size = static_cast<qint64>(obj["file_size"].toDouble());
                it->total_chunks = obj["total_chunks"].toInt();
                emit fileTransferProgress(transfer_id, it->file_name, 0, it->file_size, false);
            } else if (status == "complete") {
                emit fileTransferFinished(transfer_id, it->file_name, it->save_path, false, true, "下载完成");
                pending_downloads_.erase(it);
            }
            break;
        }

        case MsgType::FILE_DOWNLOAD_CHUNK: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (!doc.isObject()) break;

            QJsonObject obj = doc.object();
            const QString transfer_id = obj["transfer_id"].toString();
            auto it = pending_downloads_.find(transfer_id);
            if (it == pending_downloads_.end()) break;

            QByteArray chunk = QByteArray::fromBase64(obj["data"].toString().toLatin1());
            QFile file(it->save_path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
                emit fileTransferFinished(transfer_id, it->file_name, it->save_path, false, false,
                                          "写入下载文件失败");
                pending_downloads_.erase(it);
                break;
            }
            file.write(chunk);
            it->received_size += chunk.size();
            emit fileTransferProgress(transfer_id, it->file_name, it->received_size, it->file_size, false);
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

        case MsgType::GROUP_LIST_RSP:
        case MsgType::GROUP_LIST_UPDATE: {
            emit groupListReceived(body);
            break;
        }

        case MsgType::CREATE_GROUP_RSP: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                emit groupCreateResult(obj["code"].toInt(),
                                       obj["message"].toString(),
                                       obj["group_id"].toString(),
                                       obj["group_name"].toString(),
                                       obj["group_avatar"].toString(),
                                       obj["member_count"].toInt());
            }
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
                const QString nickname = obj.contains("nickname")
                    ? obj["nickname"].toString()
                    : (pending_profile_update_ ? pending_profile_nickname_ : user_nickname_);
                const QString gender = obj.contains("gender")
                    ? obj["gender"].toString()
                    : (pending_profile_update_ ? pending_profile_gender_ : user_gender_);
                const QString region = obj.contains("region")
                    ? obj["region"].toString()
                    : (pending_profile_update_ ? pending_profile_region_ : user_region_);
                const QString signature = obj.contains("signature")
                    ? obj["signature"].toString()
                    : (pending_profile_update_ ? pending_profile_signature_ : user_signature_);
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
                pending_profile_update_ = false;
                pending_profile_nickname_.clear();
                pending_profile_gender_.clear();
                pending_profile_region_.clear();
                pending_profile_signature_.clear();
            }
            break;
        }

        case MsgType::USER_PROFILE_RSP: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                const int code = obj["code"].toInt();
                const QString response_user_id = obj["user_id"].toString();
                const QString profile_user_id = response_user_id.isEmpty()
                    ? pending_user_profile_id_
                    : response_user_id;
                const bool is_current_profile = profile_user_id == user_id_;
                const QString nickname = obj.contains("nickname")
                    ? obj["nickname"].toString()
                    : (is_current_profile ? user_nickname_ : QString());
                const QString avatar_url = obj.contains("avatar_url")
                    ? obj["avatar_url"].toString()
                    : (is_current_profile ? user_avatar_url_ : QString());
                const QString gender = obj.contains("gender")
                    ? obj["gender"].toString()
                    : (is_current_profile ? user_gender_ : QString());
                const QString region = obj.contains("region")
                    ? obj["region"].toString()
                    : (is_current_profile ? user_region_ : QString());
                const QString signature = obj.contains("signature")
                    ? obj["signature"].toString()
                    : (is_current_profile ? user_signature_ : QString());

                if (code == 0 && is_current_profile) {
                    if (!nickname.isEmpty()) {
                        user_nickname_ = nickname;
                    }
                    if (!avatar_url.isEmpty()) {
                        user_avatar_url_ = avatar_url;
                    }
                    user_gender_ = gender;
                    user_region_ = region;
                    user_signature_ = signature;
                    saveCredentials();
                }

                emit userProfileReceived(code,
                                         obj["message"].toString(),
                                         profile_user_id,
                                         nickname,
                                         avatar_url,
                                         gender,
                                         region,
                                         signature);
                if (response_user_id.isEmpty() || response_user_id == pending_user_profile_id_) {
                    pending_user_profile_id_.clear();
                }
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

        case MsgType::CREATE_MOMENT_RSP: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                emit momentCreateResult(obj["code"].toInt(),
                                        obj["message"].toString());
            }
            break;
        }

        case MsgType::MOMENTS_RSP: {
            emit momentsReceived(body);
            break;
        }

        case MsgType::CALL_INVITE:
        case MsgType::CALL_ACCEPT:
        case MsgType::CALL_REJECT:
        case MsgType::CALL_CANCEL:
        case MsgType::CALL_HANGUP:
        case MsgType::CALL_ICE:
        case MsgType::CALL_TIMEOUT: {
            QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
            if (!doc.isObject()) {
                break;
            }

            QString signal_type;
            switch (type) {
                case MsgType::CALL_INVITE: signal_type = "invite"; break;
                case MsgType::CALL_ACCEPT: signal_type = "accept"; break;
                case MsgType::CALL_REJECT: signal_type = "reject"; break;
                case MsgType::CALL_CANCEL: signal_type = "cancel"; break;
                case MsgType::CALL_HANGUP: signal_type = "hangup"; break;
                case MsgType::CALL_ICE: signal_type = "ice"; break;
                case MsgType::CALL_TIMEOUT: signal_type = "timeout"; break;
                default: break;
            }
            emit callSignalReceived(signal_type, doc.object());
            break;
        }

        case MsgType::CHAT_HISTORY_RSP: {
            emit chatHistoryReceived(current_chat_history_peer_id_, current_chat_history_type_, body);
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
                    QString content_type = obj["content_type"].toString("text");
                    QString msg_id = obj["msg_id"].toString();
                    QString server_time = obj["server_time"].toString();
                    qint64 server_timestamp = obj["server_timestamp"].toInteger();
                    QString to_user_id = obj["to_user_id"].toString();
                    QString chat_type = obj["chat_type"].toInt(1) == 2
                        ? QStringLiteral("group")
                        : obj["chat_type"].toString("p2p");
                    emit offlineMessageReceived(from_user_id, content, content_type, msg_id,
                                                server_timestamp, server_time, to_user_id, chat_type);
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
