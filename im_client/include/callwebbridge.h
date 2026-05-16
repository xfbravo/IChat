/**
 * @file callwebbridge.h
 * @brief QWebChannel bridge between the Qt call state machine and browser WebRTC.
 */

#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class CallWebBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString callId READ callId NOTIFY callContextChanged)
    Q_PROPERTY(QString callType READ callType NOTIFY callContextChanged)
    Q_PROPERTY(bool incoming READ incoming NOTIFY callContextChanged)

public:
    explicit CallWebBridge(QObject* parent = nullptr);

    QString callId() const;
    QString callType() const;
    bool incoming() const;

    void setIceServers(const QJsonArray& ice_servers);
    void setCallContext(const QString& call_id,
                        const QString& call_type,
                        bool incoming,
                        const QJsonObject& remote_sdp = QJsonObject());
    void resetCallContext();

public slots:
    void pageReady();
    void localDescriptionReady(const QString& type, const QString& sdp);
    void localCandidateReady(const QJsonObject& candidate);
    void mediaError(const QString& message);
    void mediaStateChanged(const QString& state);

signals:
    void callContextChanged();
    void startRequested(const QString& call_id,
                        const QString& call_type,
                        bool incoming,
                        const QJsonObject& remote_sdp,
                        const QJsonArray& ice_servers);
    void localOfferReady(const QString& call_id, const QString& sdp);
    void localAnswerReady(const QString& call_id, const QString& sdp);
    void localIceCandidateReady(const QString& call_id, const QJsonObject& candidate);
    void mediaStarted(const QString& call_id);
    void errorOccurred(const QString& message);
    void stateChanged(const QString& state);

private:
    QString call_id_;
    QString call_type_ = QStringLiteral("audio");
    bool incoming_ = false;
    QJsonObject remote_sdp_;
    QJsonArray ice_servers_;
};
