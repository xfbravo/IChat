/**
 * @file mainwindow_calls.cpp
 * @brief 一对一实时音视频通话信令状态机和基础窗口
 */

#include "mainwindow.h"
#include "callwebbridge.h"
#include "callmediaadapter.h"
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineView>

namespace {

QString toJsStringLiteral(const QString& value) {
    const QJsonArray wrapped{value};
    const QString json = QString::fromUtf8(QJsonDocument(wrapped).toJson(QJsonDocument::Compact));
    return json.mid(1, json.size() - 2);
}

} // namespace

void MainWindow::onAudioCallClicked() {
    startOutgoingCall(QStringLiteral("audio"));
}

void MainWindow::onVideoCallClicked() {
    startOutgoingCall(QStringLiteral("video"));
}

void MainWindow::startOutgoingCall(const QString& call_type) {
    if (!tcp_client_ || tcp_client_->state() != ClientState::LoggedIn) {
        QMessageBox::warning(this, "无法呼叫", "当前未连接到服务器。");
        return;
    }
    if (call_state_ != CallState::Idle) {
        QMessageBox::information(this, "正在通话", "请先结束当前通话。");
        return;
    }
    if (current_chat_target_.isEmpty() || conversationChatType(current_chat_target_) != "p2p") {
        QMessageBox::information(this, "无法呼叫", "请选择一个好友会话后再发起通话。");
        return;
    }

    QString error_message;
    if (!ensureCallDevices(call_type, &error_message)) {
        QMessageBox::warning(this, "设备不可用", error_message);
        return;
    }

    const QString peer_id = conversationPeerId(current_chat_target_);
    active_call_id_ = Protocol::generateMsgId();
    active_call_peer_id_ = peer_id;
    active_call_type_ = call_type == "video" ? QStringLiteral("video") : QStringLiteral("audio");
    active_call_remote_sdp_ = QJsonObject();
    pending_call_ice_candidates_.clear();
    active_call_incoming_ = false;
    call_state_ = CallState::Outgoing;
    showCallDialog(callPeerName());
    updateCallDialog();
    call_timeout_timer_->start(30000);
    startWebRtcPage();
}

void MainWindow::onCallSignalReceived(const QString& signal_type, const QJsonObject& payload) {
    const QString call_id = payload["call_id"].toString();
    const QString from_user_id = payload["from_user_id"].toString();
    const QString reason = payload["reason"].toString();

    if (signal_type == "invite") {
        if (call_state_ != CallState::Idle) {
            if (tcp_client_) {
                tcp_client_->rejectCall(call_id, from_user_id, QStringLiteral("忙线"));
            }
            return;
        }

        const QString call_type = payload["call_type"].toString("audio") == "video"
            ? QStringLiteral("video")
            : QStringLiteral("audio");
        QString error_message;
        if (!ensureCallDevices(call_type, &error_message)) {
            if (tcp_client_) {
                tcp_client_->rejectCall(call_id, from_user_id, error_message);
            }
            QMessageBox::warning(this, "设备不可用", error_message);
            return;
        }

        active_call_id_ = call_id;
        active_call_peer_id_ = from_user_id;
        active_call_type_ = call_type;
        active_call_remote_sdp_ = payload["sdp"].toObject();
        pending_call_ice_candidates_.clear();
        active_call_incoming_ = true;
        call_state_ = CallState::Ringing;
        showCallDialog(callPeerName());
        updateCallDialog();
        call_timeout_timer_->start(30000);
        return;
    }

    if (!active_call_id_.isEmpty() && call_id != active_call_id_) {
        return;
    }

    if (signal_type == "accept") {
        const QJsonObject remote_sdp = payload["sdp"].toObject();
        applyRemoteDescriptionToWebView(remote_sdp);
        call_state_ = CallState::InCall;
        call_timeout_timer_->stop();
        updateCallDialog();
        return;
    }

    if (signal_type == "reject") {
        finishActiveCall(reason.isEmpty() ? QStringLiteral("对方已拒绝") : reason, false);
    } else if (signal_type == "cancel") {
        finishActiveCall(reason.isEmpty() ? QStringLiteral("对方已取消") : reason, false);
    } else if (signal_type == "hangup") {
        finishActiveCall(reason.isEmpty() ? QStringLiteral("通话已结束") : reason, false);
    } else if (signal_type == "timeout") {
        finishActiveCall(reason.isEmpty() ? QStringLiteral("呼叫超时") : reason, false);
    } else if (signal_type == "ice") {
        addRemoteCandidateToWebView(payload["candidate"].toObject());
    }
}

void MainWindow::onAcceptCallClicked() {
    if (call_state_ != CallState::Ringing || active_call_id_.isEmpty() || active_call_peer_id_.isEmpty()) {
        return;
    }

    QString error_message;
    if (!ensureCallDevices(active_call_type_, &error_message)) {
        if (tcp_client_) {
            tcp_client_->rejectCall(active_call_id_, active_call_peer_id_, error_message);
        }
        finishActiveCall(error_message, false);
        return;
    }

    call_state_ = CallState::Connecting;
    updateCallDialog();
    startWebRtcPage();
}

void MainWindow::onRejectCallClicked() {
    if (call_state_ == CallState::Ringing && tcp_client_) {
        tcp_client_->rejectCall(active_call_id_, active_call_peer_id_, QStringLiteral("已拒绝"));
    } else if (call_state_ == CallState::Outgoing && tcp_client_) {
        tcp_client_->cancelCall(active_call_id_, active_call_peer_id_, QStringLiteral("已取消"));
    }
    finishActiveCall(QStringLiteral("通话已结束"), false);
}

void MainWindow::onHangupCallClicked() {
    finishActiveCall(QStringLiteral("通话已结束"), true);
}

void MainWindow::onCallTimeout() {
    if (call_state_ == CallState::Outgoing && tcp_client_) {
        tcp_client_->timeoutCall(active_call_id_, active_call_peer_id_);
        finishActiveCall(QStringLiteral("对方无人接听"), false);
    } else if (call_state_ == CallState::Ringing && tcp_client_) {
        tcp_client_->timeoutCall(active_call_id_, active_call_peer_id_);
        finishActiveCall(QStringLiteral("呼叫超时"), false);
    }
}

void MainWindow::showCallDialog(const QString& title) {
    if (!call_dialog_) {
        call_dialog_ = new QDialog(this);
        call_dialog_->setWindowTitle("IChat 通话");
        call_dialog_->setMinimumSize(520, 420);
        connect(call_dialog_, &QDialog::rejected, this, [this]() {
            if (call_state_ != CallState::Idle) {
                finishActiveCall(QStringLiteral("通话窗口已关闭"), true);
            }
        });
        call_dialog_->setStyleSheet(R"(
            QDialog {
                background-color: #101613;
                color: #f5faf6;
                font-family: "Microsoft YaHei", sans-serif;
            }
            QLabel {
                color: #f5faf6;
                background: transparent;
            }
            QPushButton {
                min-height: 38px;
                padding: 0 18px;
                border-radius: 6px;
                border: none;
                font-size: 14px;
                font-weight: 700;
            }
        )");

        QVBoxLayout* layout = new QVBoxLayout(call_dialog_);
        layout->setContentsMargins(24, 22, 24, 20);
        layout->setSpacing(16);

        call_web_view_ = new QWebEngineView(call_dialog_);
        call_web_view_->setMinimumHeight(240);
        call_web_channel_ = new QWebChannel(call_web_view_);
        call_web_channel_->registerObject(QStringLiteral("callBridge"), call_web_bridge_);
        call_web_view_->page()->setWebChannel(call_web_channel_);
        connect(call_web_view_, &QWebEngineView::loadFinished, this, [this](bool ok) {
            if (!ok && call_status_label_) {
                call_status_label_->setText(QStringLiteral("WebRTC 页面加载失败，请确认 Qt WebEngine 已正确部署。"));
            }
        });
        connect(call_web_view_->page(), &QWebEnginePage::featurePermissionRequested,
                this, [this](const QUrl& security_origin, QWebEnginePage::Feature feature) {
                    if (!call_web_view_) {
                        return;
                    }
                    call_web_view_->page()->setFeaturePermission(
                        security_origin,
                        feature,
                        QWebEnginePage::PermissionGrantedByUser);
                });
        layout->addWidget(call_web_view_, 1);

        call_title_label_ = new QLabel(title, call_dialog_);
        call_title_label_->setAlignment(Qt::AlignCenter);
        call_title_label_->setStyleSheet("QLabel { font-size: 20px; font-weight: 800; }");
        layout->addWidget(call_title_label_);

        call_status_label_ = new QLabel(call_dialog_);
        call_status_label_->setAlignment(Qt::AlignCenter);
        call_status_label_->setWordWrap(true);
        call_status_label_->setStyleSheet("QLabel { color: #c9d8ce; font-size: 13px; }");
        layout->addWidget(call_status_label_);

        QHBoxLayout* actions = new QHBoxLayout;
        actions->setSpacing(10);
        call_accept_button_ = new QPushButton("接听", call_dialog_);
        call_accept_button_->setStyleSheet("QPushButton { background-color: #2f8f46; color: #ffffff; } QPushButton:hover { background-color: #38a653; }");
        connect(call_accept_button_, &QPushButton::clicked, this, &MainWindow::onAcceptCallClicked);
        actions->addWidget(call_accept_button_);

        call_reject_button_ = new QPushButton("拒绝", call_dialog_);
        call_reject_button_->setStyleSheet("QPushButton { background-color: #8f2f35; color: #ffffff; } QPushButton:hover { background-color: #a83b42; }");
        connect(call_reject_button_, &QPushButton::clicked, this, &MainWindow::onRejectCallClicked);
        actions->addWidget(call_reject_button_);

        call_hangup_button_ = new QPushButton("挂断", call_dialog_);
        call_hangup_button_->setStyleSheet("QPushButton { background-color: #8f2f35; color: #ffffff; } QPushButton:hover { background-color: #a83b42; }");
        connect(call_hangup_button_, &QPushButton::clicked, this, &MainWindow::onHangupCallClicked);
        actions->addWidget(call_hangup_button_);
        layout->addLayout(actions);
    }

    call_title_label_->setText(title);
    call_dialog_->show();
    call_dialog_->raise();
    call_dialog_->activateWindow();
}

void MainWindow::startWebRtcPage() {
    if (!call_web_view_ || !call_web_bridge_ || active_call_id_.isEmpty()) {
        return;
    }

    call_web_bridge_->setCallContext(
        active_call_id_,
        active_call_type_,
        active_call_incoming_,
        active_call_remote_sdp_);
    call_web_view_->load(QUrl(QStringLiteral("qrc:/web/call.html")));
}

void MainWindow::applyRemoteDescriptionToWebView(const QJsonObject& sdp) {
    if (!call_web_view_ || sdp.isEmpty()) {
        return;
    }

    const QString type = sdp["type"].toString(QStringLiteral("answer"));
    const QString sdp_text = sdp["sdp"].toString();
    if (sdp_text.isEmpty()) {
        return;
    }

    const QString script = QStringLiteral("window.applyRemoteDescription(%1, %2);")
        .arg(toJsStringLiteral(type), toJsStringLiteral(sdp_text));
    call_web_view_->page()->runJavaScript(script);
}

void MainWindow::addRemoteCandidateToWebView(const QJsonObject& candidate) {
    if (candidate.isEmpty()) {
        return;
    }

    if (!call_web_view_ || call_state_ == CallState::Ringing) {
        pending_call_ice_candidates_.append(candidate);
        return;
    }

    const QString candidate_json = QString::fromUtf8(
        QJsonDocument(candidate).toJson(QJsonDocument::Compact));
    call_web_view_->page()->runJavaScript(
        QStringLiteral("window.addRemoteCandidate(%1);").arg(candidate_json));
}

void MainWindow::updateCallDialog() {
    if (!call_dialog_) {
        return;
    }

    call_title_label_->setText(callPeerName());
    call_status_label_->setText(callStateText());
    const bool ringing = call_state_ == CallState::Ringing;
    const bool active = call_state_ == CallState::InCall || call_state_ == CallState::Connecting;
    call_accept_button_->setVisible(ringing);
    call_reject_button_->setVisible(ringing || call_state_ == CallState::Outgoing);
    call_reject_button_->setText(call_state_ == CallState::Outgoing ? "取消" : "拒绝");
    call_hangup_button_->setVisible(active);
}

void MainWindow::finishActiveCall(const QString& reason, bool notify_peer) {
    if (notify_peer && tcp_client_ && !active_call_id_.isEmpty() && !active_call_peer_id_.isEmpty()) {
        if (call_state_ == CallState::Outgoing) {
            tcp_client_->cancelCall(active_call_id_, active_call_peer_id_, reason);
        } else if (call_state_ == CallState::Ringing) {
            tcp_client_->rejectCall(active_call_id_, active_call_peer_id_, reason);
        } else {
            tcp_client_->hangupCall(active_call_id_, active_call_peer_id_, reason);
        }
    }

    call_timeout_timer_->stop();
    call_state_ = CallState::Ended;
    if (call_dialog_) {
        call_status_label_->setText(reason);
        call_accept_button_->setVisible(false);
        call_reject_button_->setVisible(false);
        call_hangup_button_->setVisible(false);
        const QString ending_call_id = active_call_id_;
        QTimer::singleShot(900, this, [this, ending_call_id]() {
            if (call_dialog_ && (active_call_id_.isEmpty() || active_call_id_ == ending_call_id)) {
                call_dialog_->hide();
            }
        });
    }

    active_call_id_.clear();
    active_call_peer_id_.clear();
    active_call_type_ = QStringLiteral("audio");
    active_call_remote_sdp_ = QJsonObject();
    pending_call_ice_candidates_.clear();
    active_call_incoming_ = false;
    if (call_web_bridge_) {
        call_web_bridge_->resetCallContext();
    }
    if (call_web_view_) {
        call_web_view_->page()->runJavaScript(QStringLiteral("window.endCall && window.endCall();"));
    }
    if (call_media_adapter_) {
        call_media_adapter_->close();
    }
    call_state_ = CallState::Idle;
}

bool MainWindow::ensureCallDevices(const QString& call_type, QString* error_message) const {
    return CallMediaAdapter::hasRequiredDevices(call_type, error_message);
}

QString MainWindow::callPeerName() const {
    const QString display = contactDisplayName(active_call_peer_id_);
    return display.isEmpty() ? active_call_peer_id_ : display;
}

QString MainWindow::callStateText() const {
    const QString media = active_call_type_ == "video" ? QStringLiteral("视频") : QStringLiteral("语音");
    switch (call_state_) {
        case CallState::Outgoing:
            return QString("正在发起%1通话...").arg(media);
        case CallState::Ringing:
            return QString("%1邀请你进行%2通话").arg(callPeerName(), media);
        case CallState::Connecting:
            return QString("正在建立%1通话...").arg(media);
        case CallState::InCall:
            return QString("%1通话已接通。").arg(media);
        case CallState::Ended:
            return QStringLiteral("通话已结束");
        case CallState::Idle:
        default:
            return QStringLiteral("空闲");
    }
}
