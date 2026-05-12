/**
 * @file mainwindow_settings.cpp
 * @brief MainWindow 设置页、头像和密码管理
 */

#include "mainwindow.h"
#include "addcontactdialog.h"
#include <QStatusBar>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QFrame>
#include <QHeaderView>
#include <QDate>
#include <QDateTime>
#include <QStyle>
#include <QTreeWidgetItem>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QScrollArea>
#include <QScrollBar>
#include <QListView>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPen>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QSize>
#include <QStringList>
#include <QToolButton>
#include <QTimer>
#include <QBuffer>
#include <QFileDialog>
#include <QFont>
#include <QImage>
#include <QImageReader>
#include <QIODevice>
#include <algorithm>
#include "mainwindow_helpers.h"

using namespace mainwindow_detail;

void MainWindow::createSettingsView() {
    settings_view_ = new QWidget;
    settings_view_->setObjectName("settingsRoot");
    settings_view_->setStyleSheet(R"(
        QWidget#settingsRoot {
            background-color: #f7f8fa;
            font-family: "Microsoft YaHei", sans-serif;
        }
        QFrame#profilePanel,
        QFrame#passwordPanel {
            background-color: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 6px;
        }
        QLabel#settingsTitle {
            color: #111827;
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#settingsSectionTitle {
            color: #111827;
            font-size: 16px;
            font-weight: 700;
        }
        QLabel#settingsName {
            color: #111827;
            font-size: 18px;
            font-weight: 600;
        }
        QLabel#settingsMeta {
            color: #6b7280;
            font-size: 13px;
        }
        QLabel#settingsStatus {
            color: #6b7280;
            font-size: 12px;
        }
        QLabel#passwordStatus {
            color: #6b7280;
            font-size: 12px;
        }
        QLineEdit#passwordInput {
            min-height: 34px;
            max-width: 340px;
            padding: 6px 10px;
            color: #111827;
            background-color: #ffffff;
            border: 1px solid #d1d5db;
            border-radius: 4px;
            font-size: 14px;
        }
        QLineEdit#passwordInput:focus {
            border-color: #4CAF50;
        }
        QPushButton#avatarUploadButton {
            min-width: 96px;
            padding: 9px 18px;
            color: #ffffff;
            background-color: #4CAF50;
            border: none;
            border-radius: 4px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#avatarUploadButton:hover {
            background-color: #45a049;
        }
        QPushButton#avatarUploadButton:disabled {
            background-color: #c9d7ca;
        }
        QPushButton#passwordChangeButton {
            min-width: 96px;
            padding: 9px 18px;
            color: #ffffff;
            background-color: #2563eb;
            border: none;
            border-radius: 4px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#passwordChangeButton:hover {
            background-color: #1d4ed8;
        }
        QPushButton#passwordChangeButton:disabled {
            background-color: #bfdbfe;
        }
    )");

    QVBoxLayout* outer_layout = new QVBoxLayout(settings_view_);
    outer_layout->setContentsMargins(36, 30, 36, 30);
    outer_layout->setSpacing(18);

    QLabel* title = new QLabel("设置", settings_view_);
    title->setObjectName("settingsTitle");
    outer_layout->addWidget(title);

    QFrame* profile_panel = new QFrame(settings_view_);
    profile_panel->setObjectName("profilePanel");
    QVBoxLayout* panel_layout = new QVBoxLayout(profile_panel);
    panel_layout->setContentsMargins(24, 22, 24, 22);
    panel_layout->setSpacing(18);

    QLabel* section_title = new QLabel("个人资料", profile_panel);
    section_title->setObjectName("settingsSectionTitle");
    panel_layout->addWidget(section_title);

    QWidget* profile_row = new QWidget(profile_panel);
    QHBoxLayout* profile_layout = new QHBoxLayout(profile_row);
    profile_layout->setContentsMargins(0, 0, 0, 0);
    profile_layout->setSpacing(18);

    settings_avatar_label_ = new QLabel(profile_row);
    settings_avatar_label_->setFixedSize(96, 96);
    settings_avatar_label_->setAlignment(Qt::AlignCenter);
    profile_layout->addWidget(settings_avatar_label_, 0, Qt::AlignTop);

    QVBoxLayout* info_layout = new QVBoxLayout;
    info_layout->setContentsMargins(0, 5, 0, 5);
    info_layout->setSpacing(7);

    QLabel* name_label = new QLabel(user_nickname_.isEmpty() ? user_id_ : user_nickname_, profile_row);
    name_label->setObjectName("settingsName");
    info_layout->addWidget(name_label);

    QLabel* id_label = new QLabel(QString("用户ID：%1").arg(user_id_), profile_row);
    id_label->setObjectName("settingsMeta");
    info_layout->addWidget(id_label);

    settings_avatar_status_label_ = new QLabel(profile_row);
    settings_avatar_status_label_->setObjectName("settingsStatus");
    settings_avatar_status_label_->setText(current_avatar_url_.isEmpty() ? "未设置头像" : "头像已同步");
    info_layout->addWidget(settings_avatar_status_label_);
    info_layout->addStretch();

    profile_layout->addLayout(info_layout, 1);

    upload_avatar_button_ = new QPushButton("上传头像", profile_row);
    upload_avatar_button_->setObjectName("avatarUploadButton");
    connect(upload_avatar_button_, &QPushButton::clicked,
            this, &MainWindow::onUploadAvatarClicked);
    profile_layout->addWidget(upload_avatar_button_, 0, Qt::AlignTop);

    panel_layout->addWidget(profile_row);
    outer_layout->addWidget(profile_panel);

    QFrame* password_panel = new QFrame(settings_view_);
    password_panel->setObjectName("passwordPanel");
    QVBoxLayout* password_layout = new QVBoxLayout(password_panel);
    password_layout->setContentsMargins(24, 22, 24, 22);
    password_layout->setSpacing(16);

    QLabel* password_title = new QLabel("修改密码", password_panel);
    password_title->setObjectName("settingsSectionTitle");
    password_layout->addWidget(password_title);

    QFormLayout* form_layout = new QFormLayout;
    form_layout->setContentsMargins(0, 0, 0, 0);
    form_layout->setHorizontalSpacing(14);
    form_layout->setVerticalSpacing(12);
    form_layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    old_password_edit_ = new QLineEdit(password_panel);
    old_password_edit_->setObjectName("passwordInput");
    old_password_edit_->setPlaceholderText("请输入旧密码");
    old_password_edit_->setEchoMode(QLineEdit::Password);

    new_password_edit_ = new QLineEdit(password_panel);
    new_password_edit_->setObjectName("passwordInput");
    new_password_edit_->setPlaceholderText("请输入新密码（至少6位）");
    new_password_edit_->setEchoMode(QLineEdit::Password);

    confirm_password_edit_ = new QLineEdit(password_panel);
    confirm_password_edit_->setObjectName("passwordInput");
    confirm_password_edit_->setPlaceholderText("请确认新密码");
    confirm_password_edit_->setEchoMode(QLineEdit::Password);

    form_layout->addRow("旧密码:", old_password_edit_);
    form_layout->addRow("新密码:", new_password_edit_);
    form_layout->addRow("确认新密码:", confirm_password_edit_);
    password_layout->addLayout(form_layout);

    QWidget* password_action_row = new QWidget(password_panel);
    QHBoxLayout* password_action_layout = new QHBoxLayout(password_action_row);
    password_action_layout->setContentsMargins(0, 0, 0, 0);
    password_action_layout->setSpacing(12);

    password_status_label_ = new QLabel(" ", password_action_row);
    password_status_label_->setObjectName("passwordStatus");
    password_action_layout->addWidget(password_status_label_, 1);

    change_password_button_ = new QPushButton("修改密码", password_action_row);
    change_password_button_->setObjectName("passwordChangeButton");
    connect(change_password_button_, &QPushButton::clicked,
            this, &MainWindow::onChangePasswordClicked);
    password_action_layout->addWidget(change_password_button_, 0, Qt::AlignRight);

    password_layout->addWidget(password_action_row);
    outer_layout->addWidget(password_panel);
    outer_layout->addStretch();

    updateAvatarPreview();
}

QString MainWindow::encodeAvatarFile(const QString& file_path) {
    // 头像直接存入协议 JSON，因此先裁成固定尺寸并压缩，控制 data URL 大小。
    QImageReader reader(file_path);
    reader.setAutoTransform(true);

    QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, "上传头像", "无法读取选择的图片");
        return QString();
    }

    constexpr int avatar_size = 256;
    QImage scaled = image.scaled(avatar_size, avatar_size,
                                 Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation);
    QRect crop_rect((scaled.width() - avatar_size) / 2,
                    (scaled.height() - avatar_size) / 2,
                    avatar_size,
                    avatar_size);
    QImage square = scaled.copy(crop_rect).convertToFormat(QImage::Format_RGB32);

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !square.save(&buffer, "JPEG", 86)) {
        QMessageBox::warning(this, "上传头像", "头像压缩失败");
        return QString();
    }

    constexpr int max_avatar_payload = 512 * 1024;
    if (bytes.size() > max_avatar_payload) {
        QMessageBox::warning(this, "上传头像", "头像文件过大");
        return QString();
    }

    return QString("data:image/jpeg;base64,%1").arg(QString::fromLatin1(bytes.toBase64()));
}

void MainWindow::updateAvatarPreview() {
    if (!settings_avatar_label_) {
        return;
    }

    // 本地预览支持 data URL 和本地路径；解析失败时显示首字母默认头像。
    const QString display_name = user_nickname_.isEmpty() ? user_id_ : user_nickname_;
    settings_avatar_label_->setPixmap(avatarPixmapFromValue(current_avatar_url_, display_name, 96));
}

void MainWindow::onEditContactRemark() {
    if (current_chat_target_.isEmpty()) {
        return;
    }

    const QString old_remark = conversations_.value(current_chat_target_).title;
    bool ok = false;
    QString new_remark = QInputDialog::getText(this,
                                               "修改联系人备注",
                                               "备注:",
                                               QLineEdit::Normal,
                                               old_remark,
                                               &ok).trimmed();
    if (!ok || new_remark.isEmpty()) {
        return;
    }

    // 备注属于联系人关系，服务端确认成功后再更新本地缓存。
    tcp_client_->updateFriendRemark(current_chat_target_, new_remark);
}

void MainWindow::onUploadAvatarClicked() {
    if (!tcp_client_ || tcp_client_->state() != ClientState::LoggedIn) {
        QMessageBox::warning(this, "上传头像", "当前未登录，无法同步头像");
        return;
    }

    const QString file_path = QFileDialog::getOpenFileName(
        this,
        "选择头像",
        QString(),
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*)");
    if (file_path.isEmpty()) {
        return;
    }

    const QString avatar_url = encodeAvatarFile(file_path);
    if (avatar_url.isEmpty()) {
        return;
    }

    // 先乐观更新预览，让用户立即看到选择结果；失败时会回滚到 TcpClient 中的旧头像。
    current_avatar_url_ = avatar_url;
    updateAvatarPreview();

    if (upload_avatar_button_) {
        upload_avatar_button_->setEnabled(false);
    }
    if (settings_avatar_status_label_) {
        settings_avatar_status_label_->setText("正在同步头像...");
    }

    tcp_client_->updateAvatar(avatar_url);
}

void MainWindow::onAvatarUpdateResult(int code, const QString& message, const QString& avatar_url) {
    if (upload_avatar_button_) {
        upload_avatar_button_->setEnabled(true);
    }

    if (code != 0) {
        // 同步失败时恢复已登录资料中的头像，避免 UI 显示未保存的数据。
        current_avatar_url_ = tcp_client_ ? tcp_client_->avatarUrl() : QString();
        updateAvatarPreview();
        const QString error = message.isEmpty() ? "头像同步失败" : message;
        if (settings_avatar_status_label_) {
            settings_avatar_status_label_->setText(error);
        }
        QMessageBox::warning(this, "上传头像", error);
        return;
    }

    if (!avatar_url.isEmpty()) {
        current_avatar_url_ = avatar_url;
    }
    updateAvatarPreview();
    if (settings_avatar_status_label_) {
        settings_avatar_status_label_->setText("头像已同步");
    }
}

void MainWindow::onChangePasswordClicked() {
    if (!tcp_client_ || tcp_client_->state() != ClientState::LoggedIn) {
        QMessageBox::warning(this, "修改密码", "当前未登录，无法修改密码");
        return;
    }

    const QString old_password = old_password_edit_ ? old_password_edit_->text() : QString();
    const QString new_password = new_password_edit_ ? new_password_edit_->text() : QString();
    const QString confirm_password = confirm_password_edit_ ? confirm_password_edit_->text() : QString();

    // 校验失败直接在页面内提示，只有通过本地校验后才发起网络请求。
    auto showPasswordStatus = [this](const QString& text) {
        if (password_status_label_) {
            password_status_label_->setText(text);
        }
    };

    if (old_password.isEmpty()) {
        showPasswordStatus("请输入旧密码");
        if (old_password_edit_) old_password_edit_->setFocus();
        return;
    }
    if (new_password.isEmpty()) {
        showPasswordStatus("请输入新密码");
        if (new_password_edit_) new_password_edit_->setFocus();
        return;
    }
    if (new_password.length() < 6) {
        showPasswordStatus("新密码长度至少6位");
        if (new_password_edit_) new_password_edit_->setFocus();
        return;
    }
    if (new_password != confirm_password) {
        showPasswordStatus("两次新密码不一致");
        if (confirm_password_edit_) confirm_password_edit_->setFocus();
        return;
    }
    if (old_password == new_password) {
        showPasswordStatus("新密码不能与旧密码相同");
        if (new_password_edit_) new_password_edit_->setFocus();
        return;
    }

    if (change_password_button_) {
        change_password_button_->setEnabled(false);
    }
    showPasswordStatus("正在修改密码...");
    tcp_client_->changePassword(old_password, new_password);
}

void MainWindow::onPasswordChangeResult(int code, const QString& message) {
    if (change_password_button_) {
        change_password_button_->setEnabled(true);
    }

    const QString result_message = message.isEmpty()
        ? (code == 0 ? "密码已更新" : "修改密码失败")
        : message;

    if (password_status_label_) {
        password_status_label_->setText(result_message);
    }

    if (code != 0) {
        QMessageBox::warning(this, "修改密码", result_message);
        return;
    }

    // 修改成功后清空敏感输入，避免密码继续留在界面控件中。
    if (old_password_edit_) old_password_edit_->clear();
    if (new_password_edit_) new_password_edit_->clear();
    if (confirm_password_edit_) confirm_password_edit_->clear();
    QMessageBox::information(this, "修改密码", result_message);
}
