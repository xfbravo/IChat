/**
 * @file callbrowserbridge.h
 * @brief Local HTTP bridge for an external browser WebRTC call page.
 */

#pragma once

#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QQueue>
#include <QString>
#include <QUrl>

class QTcpServer;
class QTcpSocket;

class CallBrowserBridge : public QObject {
    Q_OBJECT

public:
    explicit CallBrowserBridge(QObject* parent = nullptr);

    bool startServer(QString* error_message = nullptr);
    void stopServer();
    QUrl callUrl() const;

    void setIceServers(const QJsonArray& ice_servers);
    void setCallContext(const QString& call_id,
                        const QString& call_type,
                        bool incoming,
                        const QJsonObject& remote_sdp = QJsonObject());
    void resetCallContext();
    void queueRemoteDescription(const QJsonObject& sdp);
    void queueRemoteCandidate(const QJsonObject& candidate);
    void queueEndCall(const QString& reason);

signals:
    void localOfferReady(const QString& call_id, const QString& sdp);
    void localAnswerReady(const QString& call_id, const QString& sdp);
    void localIceCandidateReady(const QString& call_id, const QJsonObject& candidate);
    void mediaStarted(const QString& call_id);
    void errorOccurred(const QString& message);
    void stateChanged(const QString& state);

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    struct PendingHttpRequest {
        QByteArray method;
        QByteArray path;
        QByteArray body;
    };

    bool parseRequest(const QByteArray& raw, PendingHttpRequest* request) const;
    void handleRequest(QTcpSocket* socket, const PendingHttpRequest& request);
    void sendJson(QTcpSocket* socket, const QJsonObject& body, int status_code = 200);
    void sendHtml(QTcpSocket* socket);
    void sendNotFound(QTcpSocket* socket);
    void sendOptions(QTcpSocket* socket);
    QJsonObject nextEvent();

    QTcpServer* server_ = nullptr;
    QHash<QTcpSocket*, QByteArray> buffers_;
    QJsonArray ice_servers_;
    QString call_id_;
    QString call_type_ = QStringLiteral("audio");
    bool incoming_ = false;
    QJsonObject remote_sdp_;
    bool context_sent_ = false;
    QQueue<QJsonObject> pending_events_;
};
