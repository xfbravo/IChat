/**
 * @file callwebbridge.cpp
 * @brief QWebChannel bridge between the Qt call state machine and browser WebRTC.
 */

#include "callwebbridge.h"

CallWebBridge::CallWebBridge(QObject* parent)
    : QObject(parent) {
}

QString CallWebBridge::callId() const {
    return call_id_;
}

QString CallWebBridge::callType() const {
    return call_type_;
}

bool CallWebBridge::incoming() const {
    return incoming_;
}

void CallWebBridge::setIceServers(const QJsonArray& ice_servers) {
    ice_servers_ = ice_servers;
}

void CallWebBridge::setCallContext(const QString& call_id,
                                   const QString& call_type,
                                   bool incoming,
                                   const QJsonObject& remote_sdp) {
    call_id_ = call_id;
    call_type_ = call_type == QStringLiteral("video")
        ? QStringLiteral("video")
        : QStringLiteral("audio");
    incoming_ = incoming;
    remote_sdp_ = remote_sdp;
    emit callContextChanged();
}

void CallWebBridge::resetCallContext() {
    call_id_.clear();
    call_type_ = QStringLiteral("audio");
    incoming_ = false;
    remote_sdp_ = QJsonObject();
    emit callContextChanged();
}

void CallWebBridge::pageReady() {
    if (call_id_.isEmpty()) {
        return;
    }
    emit startRequested(call_id_, call_type_, incoming_, remote_sdp_, ice_servers_);
}

void CallWebBridge::localDescriptionReady(const QString& type, const QString& sdp) {
    if (call_id_.isEmpty() || sdp.isEmpty()) {
        return;
    }

    if (type == QStringLiteral("answer")) {
        emit localAnswerReady(call_id_, sdp);
    } else {
        emit localOfferReady(call_id_, sdp);
    }
}

void CallWebBridge::localCandidateReady(const QJsonObject& candidate) {
    if (call_id_.isEmpty() || candidate.isEmpty()) {
        return;
    }
    emit localIceCandidateReady(call_id_, candidate);
}

void CallWebBridge::mediaError(const QString& message) {
    emit errorOccurred(message.isEmpty()
        ? QStringLiteral("WebRTC 媒体初始化失败。")
        : message);
}

void CallWebBridge::mediaStateChanged(const QString& state) {
    if (state == QStringLiteral("media-started") && !call_id_.isEmpty()) {
        emit mediaStarted(call_id_);
        return;
    }
    emit stateChanged(state);
}
