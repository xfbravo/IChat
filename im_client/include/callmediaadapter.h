/**
 * @file callmediaadapter.h
 * @brief 实时通话媒体适配器边界
 */

#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <memory>

class CallMediaAdapter : public QObject {
    Q_OBJECT

public:
    explicit CallMediaAdapter(QObject* parent = nullptr);
    ~CallMediaAdapter() override;

    static bool webRtcAvailable();
    static bool hasRequiredDevices(const QString& call_type, QString* error_message);
    static QJsonArray defaultIceServers(const QString& signaling_host);

    void setIceServers(const QJsonArray& ice_servers);
    void setForceRelay(bool force_relay);

    bool startOffer(const QString& call_id, const QString& call_type);
    bool startAnswer(const QString& call_id, const QString& call_type, const QJsonObject& remote_sdp);
    bool setRemoteDescription(const QJsonObject& sdp);
    bool addRemoteCandidate(const QJsonObject& candidate);
    void close();

signals:
    void localDescriptionReady(const QString& call_id, const QJsonObject& sdp);
    void localCandidateReady(const QString& call_id, const QJsonObject& candidate);
    void stateChanged(const QString& state);
    void errorOccurred(const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
