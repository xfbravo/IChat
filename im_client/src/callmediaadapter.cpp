/**
 * @file callmediaadapter.cpp
 * @brief libdatachannel PeerConnection adapter for call signaling
 */

#include "callmediaadapter.h"
#include <QMediaDevices>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>

#if defined(ICHAT_WITH_LIBDATACHANNEL)
#include <rtc/rtc.hpp>
#endif

struct CallMediaAdapter::Impl {
    QString call_id;
    QString call_type = QStringLiteral("audio");
    QJsonArray ice_servers;
    bool force_relay = false;
#if defined(ICHAT_WITH_LIBDATACHANNEL)
    std::shared_ptr<rtc::PeerConnection> peer_connection;
    std::shared_ptr<rtc::DataChannel> control_channel;
#endif
};

namespace {

QString envOrSetting(const char* env_name,
                     const QString& setting_key,
                     const QString& default_value = QString()) {
    const QString env_value = qEnvironmentVariable(env_name).trimmed();
    if (!env_value.isEmpty()) {
        return env_value;
    }
    QSettings settings("IMClient", "TcpClient");
    return settings.value(setting_key, default_value).toString().trimmed();
}

QStringList splitServerList(const QString& value) {
    QStringList result;
    for (QString item : value.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts)) {
        item = item.trimmed();
        if (!item.isEmpty()) {
            result.append(item);
        }
    }
    return result;
}

#if defined(ICHAT_WITH_LIBDATACHANNEL)
rtc::Configuration makeRtcConfiguration(const QJsonArray& ice_servers, bool force_relay) {
    rtc::Configuration config;
    if (force_relay) {
        config.iceTransportPolicy = rtc::TransportPolicy::Relay;
    }

    for (const QJsonValue& value : ice_servers) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject server = value.toObject();
        const QString type = server["type"].toString("stun").trimmed().toLower();
        const QString url = server["url"].toString().trimmed();
        const QString host = server["host"].toString().trimmed();
        const quint16 port = static_cast<quint16>(server["port"].toInt(3478));
        if (!url.isEmpty()) {
            config.iceServers.emplace_back(url.toStdString());
            continue;
        }
        if (host.isEmpty()) {
            continue;
        }
        if (type == "turn") {
            const QString username = server["username"].toString();
            const QString password = server["password"].toString();
            const QString relay = server["relay"].toString("udp").trimmed().toLower();
            rtc::IceServer::RelayType relay_type = rtc::IceServer::RelayType::TurnUdp;
            if (relay == "tcp") {
                relay_type = rtc::IceServer::RelayType::TurnTcp;
            } else if (relay == "tls") {
                relay_type = rtc::IceServer::RelayType::TurnTls;
            }
            config.iceServers.emplace_back(host.toStdString(), port,
                                           username.toStdString(), password.toStdString(),
                                           relay_type);
        } else {
            config.iceServers.emplace_back(host.toStdString(), port);
        }
    }
    return config;
}
#endif

} // namespace

CallMediaAdapter::CallMediaAdapter(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>()) {
}

CallMediaAdapter::~CallMediaAdapter() {
    close();
}

bool CallMediaAdapter::webRtcAvailable() {
#if defined(ICHAT_WITH_LIBDATACHANNEL)
    return true;
#else
    return false;
#endif
}

bool CallMediaAdapter::hasRequiredDevices(const QString& call_type, QString* error_message) {
    if (QMediaDevices::audioInputs().isEmpty()) {
        if (error_message) {
            *error_message = "未检测到可用麦克风。";
        }
        return false;
    }
    if (call_type == "video" && QMediaDevices::videoInputs().isEmpty()) {
        if (error_message) {
            *error_message = "未检测到可用摄像头。";
        }
        return false;
    }
    return true;
}

QJsonArray CallMediaAdapter::defaultIceServers(const QString& signaling_host) {
    QJsonArray servers;

    const QString default_host = signaling_host.trimmed().isEmpty()
        ? QStringLiteral("61.184.13.118")
        : signaling_host.trimmed();
    const QString host = envOrSetting("ICHAT_TURN_HOST", "rtc_turn_host", default_host);
    const int port = envOrSetting("ICHAT_TURN_PORT", "rtc_turn_port", "3478").toInt();
    const QString username = envOrSetting("ICHAT_TURN_USER", "rtc_turn_user", "ichat");
    const QString password = envOrSetting("ICHAT_TURN_PASSWORD", "rtc_turn_password", "dengni0425");

    if (!host.isEmpty() && !username.isEmpty() && !password.isEmpty()) {
        QJsonObject turn;
        turn["type"] = "turn";
        turn["host"] = host;
        turn["port"] = port > 0 ? port : 3478;
        turn["username"] = username;
        turn["password"] = password;
        turn["relay"] = envOrSetting("ICHAT_TURN_RELAY", "rtc_turn_relay", "udp");
        servers.append(turn);
    }

    const QString default_stun = QStringLiteral("stun:%1:3478").arg(default_host);
    const QString stun_list = envOrSetting("ICHAT_STUN_URLS", "rtc_stun_urls", default_stun);
    for (const QString& stun_url : splitServerList(stun_list)) {
        QJsonObject stun;
        stun["type"] = "stun";
        stun["url"] = stun_url;
        servers.append(stun);
    }

    return servers;
}

void CallMediaAdapter::setIceServers(const QJsonArray& ice_servers) {
    impl_->ice_servers = ice_servers;
}

void CallMediaAdapter::setForceRelay(bool force_relay) {
    impl_->force_relay = force_relay;
}

bool CallMediaAdapter::startOffer(const QString& call_id, const QString& call_type) {
#if defined(ICHAT_WITH_LIBDATACHANNEL)
    close();
    impl_->call_id = call_id;
    impl_->call_type = call_type == "video" ? QStringLiteral("video") : QStringLiteral("audio");

    rtc::Configuration config = makeRtcConfiguration(impl_->ice_servers, impl_->force_relay);
    impl_->peer_connection = std::make_shared<rtc::PeerConnection>(config);

    const QString current_call_id = impl_->call_id;
    impl_->peer_connection->onLocalDescription([this, current_call_id](rtc::Description description) {
        QJsonObject sdp;
        sdp["type"] = QString::fromStdString(description.typeString());
        sdp["sdp"] = QString::fromStdString(std::string(description));
        QMetaObject::invokeMethod(this, [this, current_call_id, sdp]() {
            emit localDescriptionReady(current_call_id, sdp);
        }, Qt::QueuedConnection);
    });
    impl_->peer_connection->onLocalCandidate([this, current_call_id](rtc::Candidate candidate) {
        QJsonObject obj;
        obj["candidate"] = QString::fromStdString(candidate.candidate());
        obj["mid"] = QString::fromStdString(candidate.mid());
        QMetaObject::invokeMethod(this, [this, current_call_id, obj]() {
            emit localCandidateReady(current_call_id, obj);
        }, Qt::QueuedConnection);
    });
    impl_->peer_connection->onStateChange([this](rtc::PeerConnection::State state) {
        QString text = QString::number(static_cast<int>(state));
        QMetaObject::invokeMethod(this, [this, text]() {
            emit stateChanged(text);
        }, Qt::QueuedConnection);
    });

    impl_->control_channel = impl_->peer_connection->createDataChannel("ichat-call-control");
    impl_->control_channel->onOpen([this]() {
        QMetaObject::invokeMethod(this, [this]() {
            emit stateChanged(QStringLiteral("datachannel-open"));
        }, Qt::QueuedConnection);
    });
    impl_->peer_connection->setLocalDescription(rtc::Description::Type::Offer);
    return true;
#else
    Q_UNUSED(call_id);
    Q_UNUSED(call_type);
    emit errorOccurred(QStringLiteral("当前构建未启用 libdatachannel。"));
    return false;
#endif
}

bool CallMediaAdapter::startAnswer(const QString& call_id, const QString& call_type, const QJsonObject& remote_sdp) {
#if defined(ICHAT_WITH_LIBDATACHANNEL)
    close();
    impl_->call_id = call_id;
    impl_->call_type = call_type == "video" ? QStringLiteral("video") : QStringLiteral("audio");

    rtc::Configuration config = makeRtcConfiguration(impl_->ice_servers, impl_->force_relay);
    impl_->peer_connection = std::make_shared<rtc::PeerConnection>(config);

    const QString current_call_id = impl_->call_id;
    impl_->peer_connection->onLocalDescription([this, current_call_id](rtc::Description description) {
        QJsonObject sdp;
        sdp["type"] = QString::fromStdString(description.typeString());
        sdp["sdp"] = QString::fromStdString(std::string(description));
        QMetaObject::invokeMethod(this, [this, current_call_id, sdp]() {
            emit localDescriptionReady(current_call_id, sdp);
        }, Qt::QueuedConnection);
    });
    impl_->peer_connection->onLocalCandidate([this, current_call_id](rtc::Candidate candidate) {
        QJsonObject obj;
        obj["candidate"] = QString::fromStdString(candidate.candidate());
        obj["mid"] = QString::fromStdString(candidate.mid());
        QMetaObject::invokeMethod(this, [this, current_call_id, obj]() {
            emit localCandidateReady(current_call_id, obj);
        }, Qt::QueuedConnection);
    });
    impl_->peer_connection->onStateChange([this](rtc::PeerConnection::State state) {
        QString text = QString::number(static_cast<int>(state));
        QMetaObject::invokeMethod(this, [this, text]() {
            emit stateChanged(text);
        }, Qt::QueuedConnection);
    });
    impl_->peer_connection->onDataChannel([this](std::shared_ptr<rtc::DataChannel> incoming) {
        impl_->control_channel = std::move(incoming);
        QMetaObject::invokeMethod(this, [this]() {
            emit stateChanged(QStringLiteral("datachannel-received"));
        }, Qt::QueuedConnection);
    });

    if (!setRemoteDescription(remote_sdp)) {
        return false;
    }
    impl_->peer_connection->setLocalDescription(rtc::Description::Type::Answer);
    return true;
#else
    Q_UNUSED(call_id);
    Q_UNUSED(call_type);
    Q_UNUSED(remote_sdp);
    emit errorOccurred(QStringLiteral("当前构建未启用 libdatachannel。"));
    return false;
#endif
}

bool CallMediaAdapter::setRemoteDescription(const QJsonObject& sdp) {
#if defined(ICHAT_WITH_LIBDATACHANNEL)
    if (!impl_->peer_connection) {
        emit errorOccurred(QStringLiteral("PeerConnection 尚未创建。"));
        return false;
    }
    const QString sdp_text = sdp["sdp"].toString();
    const QString type = sdp["type"].toString();
    if (sdp_text.isEmpty() || type.isEmpty()) {
        emit errorOccurred(QStringLiteral("远端 SDP 不完整。"));
        return false;
    }
    impl_->peer_connection->setRemoteDescription(
        rtc::Description(sdp_text.toStdString(), type.toStdString()));
    return true;
#else
    Q_UNUSED(sdp);
    return false;
#endif
}

bool CallMediaAdapter::addRemoteCandidate(const QJsonObject& candidate) {
#if defined(ICHAT_WITH_LIBDATACHANNEL)
    if (!impl_->peer_connection) {
        return false;
    }
    const QString candidate_text = candidate["candidate"].toString();
    const QString mid = candidate["mid"].toString(candidate["sdpMid"].toString());
    if (candidate_text.isEmpty() || mid.isEmpty()) {
        return false;
    }
    impl_->peer_connection->addRemoteCandidate(
        rtc::Candidate(candidate_text.toStdString(), mid.toStdString()));
    return true;
#else
    Q_UNUSED(candidate);
    return false;
#endif
}

void CallMediaAdapter::close() {
#if defined(ICHAT_WITH_LIBDATACHANNEL)
    if (impl_->control_channel) {
        impl_->control_channel->close();
        impl_->control_channel.reset();
    }
    if (impl_->peer_connection) {
        impl_->peer_connection->close();
        impl_->peer_connection.reset();
    }
#endif
}
