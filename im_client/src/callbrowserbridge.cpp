/**
 * @file callbrowserbridge.cpp
 * @brief Local HTTP bridge for an external browser WebRTC call page.
 */

#include "callbrowserbridge.h"
#include <QCoreApplication>
#include <QFile>
#include <QHostAddress>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <cstring>

namespace {

constexpr quint16 kPreferredCallBridgePort = 28517;

QByteArray reasonPhrase(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        default: return "Internal Server Error";
    }
}

QByteArray jsonResponseHeaders(int status_code, qsizetype content_length) {
    QByteArray headers;
    headers += "HTTP/1.1 " + QByteArray::number(status_code) + " " + reasonPhrase(status_code) + "\r\n";
    headers += "Content-Type: application/json; charset=utf-8\r\n";
    headers += "Cache-Control: no-store\r\n";
    headers += "Content-Length: " + QByteArray::number(content_length) + "\r\n";
    headers += "Connection: close\r\n\r\n";
    return headers;
}

QString callHtmlPath() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/web/call.html");
}

} // namespace

CallBrowserBridge::CallBrowserBridge(QObject* parent)
    : QObject(parent)
    , server_(new QTcpServer(this)) {
    connect(server_, &QTcpServer::newConnection, this, &CallBrowserBridge::onNewConnection);
}

bool CallBrowserBridge::startServer(QString* error_message) {
    if (server_->isListening()) {
        return true;
    }

    if (!server_->listen(QHostAddress::LocalHost, kPreferredCallBridgePort)
        && !server_->listen(QHostAddress::LocalHost, 0)) {
        if (error_message) {
            *error_message = server_->errorString();
        }
        return false;
    }
    return true;
}

void CallBrowserBridge::stopServer() {
    server_->close();
}

QUrl CallBrowserBridge::callUrl() const {
    if (!server_->isListening()) {
        return QUrl();
    }
    return QUrl(QStringLiteral("http://127.0.0.1:%1/call").arg(server_->serverPort()));
}

void CallBrowserBridge::setIceServers(const QJsonArray& ice_servers) {
    ice_servers_ = ice_servers;
}

void CallBrowserBridge::setCallContext(const QString& call_id,
                                       const QString& call_type,
                                       bool incoming,
                                       const QJsonObject& remote_sdp) {
    call_id_ = call_id;
    call_type_ = call_type == QStringLiteral("video") ? QStringLiteral("video") : QStringLiteral("audio");
    incoming_ = incoming;
    remote_sdp_ = remote_sdp;
    context_sent_ = false;
    pending_events_.clear();
}

void CallBrowserBridge::resetCallContext() {
    call_id_.clear();
    call_type_ = QStringLiteral("audio");
    incoming_ = false;
    remote_sdp_ = QJsonObject();
    context_sent_ = false;
    pending_events_.clear();
}

void CallBrowserBridge::queueRemoteDescription(const QJsonObject& sdp) {
    if (sdp.isEmpty()) {
        return;
    }
    QJsonObject event;
    event["type"] = "remote-description";
    event["sdp"] = sdp;
    pending_events_.enqueue(event);
}

void CallBrowserBridge::queueRemoteCandidate(const QJsonObject& candidate) {
    if (candidate.isEmpty()) {
        return;
    }
    QJsonObject event;
    event["type"] = "remote-candidate";
    event["candidate"] = candidate;
    pending_events_.enqueue(event);
}

void CallBrowserBridge::queueEndCall(const QString& reason) {
    QJsonObject event;
    event["type"] = "end";
    event["reason"] = reason;
    pending_events_.enqueue(event);
}

void CallBrowserBridge::onNewConnection() {
    while (QTcpSocket* socket = server_->nextPendingConnection()) {
        buffers_.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, &CallBrowserBridge::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            buffers_.remove(socket);
            socket->deleteLater();
        });
    }
}

void CallBrowserBridge::onReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    QByteArray& buffer = buffers_[socket];
    buffer += socket->readAll();
    const int header_end = buffer.indexOf("\r\n\r\n");
    if (header_end < 0) {
        return;
    }

    const QByteArray header = buffer.left(header_end);
    qsizetype content_length = 0;
    for (const QByteArray& line : header.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.toLower().startsWith("content-length:")) {
            content_length = trimmed.mid(strlen("content-length:")).trimmed().toLongLong();
        }
    }

    if (buffer.size() < header_end + 4 + content_length) {
        return;
    }

    PendingHttpRequest request;
    if (!parseRequest(buffer.left(header_end + 4 + content_length), &request)) {
        sendJson(socket, QJsonObject{{"error", "bad request"}}, 400);
        buffers_[socket].clear();
        return;
    }
    buffers_[socket].clear();
    handleRequest(socket, request);
}

bool CallBrowserBridge::parseRequest(const QByteArray& raw, PendingHttpRequest* request) const {
    const int header_end = raw.indexOf("\r\n\r\n");
    if (header_end < 0 || !request) {
        return false;
    }

    const QList<QByteArray> lines = raw.left(header_end).split('\n');
    if (lines.isEmpty()) {
        return false;
    }

    const QList<QByteArray> request_line = lines.first().trimmed().split(' ');
    if (request_line.size() < 2) {
        return false;
    }

    request->method = request_line.at(0).trimmed();
    request->path = request_line.at(1).trimmed();
    const int query_index = request->path.indexOf('?');
    if (query_index >= 0) {
        request->path = request->path.left(query_index);
    }
    request->body = raw.mid(header_end + 4);
    return true;
}

void CallBrowserBridge::handleRequest(QTcpSocket* socket, const PendingHttpRequest& request) {
    if (request.method == "OPTIONS") {
        sendOptions(socket);
        return;
    }

    if (request.method == "GET" && (request.path == "/" || request.path == "/call")) {
        sendHtml(socket);
        return;
    }

    if (request.method == "GET" && request.path == "/api/context") {
        QJsonObject body;
        body["has_call"] = !call_id_.isEmpty();
        body["call_id"] = call_id_;
        body["call_type"] = call_type_;
        body["incoming"] = incoming_;
        body["remote_sdp"] = remote_sdp_;
        body["ice_servers"] = ice_servers_;
        context_sent_ = true;
        sendJson(socket, body);
        return;
    }

    if (request.method == "GET" && request.path == "/api/events") {
        sendJson(socket, nextEvent());
        return;
    }

    if (request.method == "POST" && request.path == "/api/local-description") {
        const QJsonObject body = QJsonDocument::fromJson(request.body).object();
        const QString call_id = body["call_id"].toString();
        const QString type = body["type"].toString();
        const QString sdp = body["sdp"].toString();
        if (call_id == call_id_ && !sdp.isEmpty()) {
            if (type == "answer") {
                emit localAnswerReady(call_id, sdp);
            } else {
                emit localOfferReady(call_id, sdp);
            }
        }
        sendJson(socket, QJsonObject{{"ok", true}});
        return;
    }

    if (request.method == "POST" && request.path == "/api/local-candidate") {
        const QJsonObject body = QJsonDocument::fromJson(request.body).object();
        const QString call_id = body["call_id"].toString();
        const QJsonObject candidate = body["candidate"].toObject();
        if (call_id == call_id_ && !candidate.isEmpty()) {
            emit localIceCandidateReady(call_id, candidate);
        }
        sendJson(socket, QJsonObject{{"ok", true}});
        return;
    }

    if (request.method == "POST" && request.path == "/api/media-state") {
        const QJsonObject body = QJsonDocument::fromJson(request.body).object();
        const QString state = body["state"].toString();
        const QString message = body["message"].toString();
        if (state == "media-started" && !call_id_.isEmpty()) {
            emit mediaStarted(call_id_);
        } else if (state == "error") {
            emit errorOccurred(message.isEmpty() ? QStringLiteral("WebRTC 媒体初始化失败。") : message);
        } else if (!message.isEmpty()) {
            emit stateChanged(message);
        }
        sendJson(socket, QJsonObject{{"ok", true}});
        return;
    }

    sendNotFound(socket);
}

void CallBrowserBridge::sendJson(QTcpSocket* socket, const QJsonObject& body, int status_code) {
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    socket->write(jsonResponseHeaders(status_code, payload.size()));
    socket->write(payload);
    socket->disconnectFromHost();
}

void CallBrowserBridge::sendHtml(QTcpSocket* socket) {
    QFile file(callHtmlPath());
    QByteArray payload;
    if (file.open(QIODevice::ReadOnly)) {
        payload = file.readAll();
    } else {
        payload = "<!doctype html><meta charset=\"utf-8\"><title>IChat Call</title>"
                  "<body>call.html not found. Please deploy the web directory beside the executable.</body>";
    }

    QByteArray headers;
    headers += "HTTP/1.1 200 OK\r\n";
    headers += "Content-Type: text/html; charset=utf-8\r\n";
    headers += "Cache-Control: no-store\r\n";
    headers += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
    headers += "Connection: close\r\n\r\n";
    socket->write(headers);
    socket->write(payload);
    socket->disconnectFromHost();
}

void CallBrowserBridge::sendNotFound(QTcpSocket* socket) {
    sendJson(socket, QJsonObject{{"error", "not found"}}, 404);
}

void CallBrowserBridge::sendOptions(QTcpSocket* socket) {
    socket->write(jsonResponseHeaders(204, 0));
    socket->disconnectFromHost();
}

QJsonObject CallBrowserBridge::nextEvent() {
    if (!pending_events_.isEmpty()) {
        return pending_events_.dequeue();
    }
    return QJsonObject{{"type", "none"}};
}
