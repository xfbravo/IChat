/**
 * @file mainwindow_me.cpp
 * @brief MainWindow “我”页、头像、个人信息和账号管理
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

void MainWindow::createMeView() {
    me_view_ = new QWidget;
    me_view_->setObjectName("meRoot");
    me_view_->setStyleSheet(R"(
        QWidget#meRoot {
            background-color: #f7f8fa;
            font-family: "Microsoft YaHei", sans-serif;
        }
        QStackedWidget#meStack {
            border: none;
        }
        QScrollArea#meScrollArea {
            background-color: #f7f8fa;
            border: none;
        }
        QWidget#mePage {
            background-color: #f7f8fa;
        }
        QFrame#mePanel,
        QFrame#profilePanel,
        QFrame#passwordPanel,
        QFrame#placeholderPanel {
            background-color: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 6px;
        }
        QLabel#meTitle {
            color: #111827;
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#meSectionTitle {
            color: #111827;
            font-size: 16px;
            font-weight: 700;
        }
        QLabel#meName {
            color: #111827;
            font-size: 18px;
            font-weight: 600;
        }
        QLabel#meMeta {
            color: #6b7280;
            font-size: 13px;
        }
        QLabel#meStatus,
        QLabel#passwordStatus {
            color: #6b7280;
            font-size: 12px;
        }
        QLineEdit#profileInput,
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
        QLineEdit#profileInput:focus,
        QLineEdit#passwordInput:focus {
            border-color: #4CAF50;
        }
        QToolButton#profileSummaryButton,
        QToolButton#meEntryButton,
        QToolButton#dangerEntryButton {
            min-height: 54px;
            padding: 0 18px;
            color: #111827;
            background-color: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 4px;
            font-size: 15px;
            text-align: left;
        }
        QToolButton#profileSummaryButton {
            min-height: 108px;
            font-size: 14px;
            font-weight: 500;
        }
        QToolButton#profileSummaryButton:hover,
        QToolButton#meEntryButton:hover {
            background-color: #f3f4f6;
            border-color: #d1d5db;
        }
        QToolButton#dangerEntryButton {
            color: #dc2626;
            font-weight: 600;
        }
        QToolButton#dangerEntryButton:hover {
            background-color: #fef2f2;
            border-color: #fecaca;
        }
        QPushButton#backButton {
            min-width: 72px;
            padding: 7px 12px;
            color: #374151;
            background-color: #ffffff;
            border: 1px solid #d1d5db;
            border-radius: 4px;
            font-size: 13px;
        }
        QPushButton#backButton:hover {
            background-color: #f3f4f6;
        }
        QPushButton#avatarUploadButton,
        QPushButton#profileSaveButton {
            min-width: 96px;
            padding: 9px 18px;
            color: #ffffff;
            background-color: #4CAF50;
            border: none;
            border-radius: 4px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#avatarUploadButton:hover,
        QPushButton#profileSaveButton:hover {
            background-color: #45a049;
        }
        QPushButton#avatarUploadButton:disabled,
        QPushButton#profileSaveButton:disabled {
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

    auto createPanel = [](QWidget* parent, const QString& object_name) -> QFrame* {
        QFrame* panel = new QFrame(parent);
        panel->setObjectName(object_name);
        return panel;
    };

    QVBoxLayout* root_layout = new QVBoxLayout(me_view_);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    me_stack_ = new QStackedWidget(me_view_);
    me_stack_->setObjectName("meStack");
    root_layout->addWidget(me_stack_);

    QWidget* home_page = new QWidget(me_stack_);
    home_page->setObjectName("mePage");
    QWidget* profile_page = new QWidget(me_stack_);
    profile_page->setObjectName("mePage");
    QWidget* favorite_page = new QWidget(me_stack_);
    favorite_page->setObjectName("mePage");
    QWidget* account_page = new QWidget(me_stack_);
    account_page->setObjectName("mePage");
    QWidget* password_page = new QWidget(me_stack_);
    password_page->setObjectName("mePage");
    QWidget* moments_page = new QWidget(me_stack_);
    moments_page->setObjectName("mePage");

    auto showPage = [this](QWidget* page) {
        if (me_stack_ && page) {
            me_stack_->setCurrentWidget(page);
        }
        updateAvatarPreview();
        updateMeProfileText();
    };

    auto addPageHeader = [this, showPage](QVBoxLayout* layout,
                                          QWidget* parent,
                                          const QString& title_text,
                                          QWidget* back_page) {
        QWidget* header = new QWidget(parent);
        QHBoxLayout* header_layout = new QHBoxLayout(header);
        header_layout->setContentsMargins(0, 0, 0, 0);
        header_layout->setSpacing(12);

        QPushButton* back_button = new QPushButton("返回", header);
        back_button->setObjectName("backButton");
        back_button->setCursor(Qt::PointingHandCursor);
        connect(back_button, &QPushButton::clicked, this, [showPage, back_page]() {
            showPage(back_page);
        });
        header_layout->addWidget(back_button, 0, Qt::AlignLeft);

        QLabel* title = new QLabel(title_text, header);
        title->setObjectName("meTitle");
        header_layout->addWidget(title, 1);

        layout->addWidget(header);
    };

    auto addEntryButton = [this](QVBoxLayout* layout,
                                 QWidget* parent,
                                 const QString& icon_type,
                                 const QString& text,
                                 const QString& object_name,
                                 auto handler) -> QToolButton* {
        QToolButton* button = new QToolButton(parent);
        button->setObjectName(object_name);
        button->setText(text);
        button->setIcon(lineIcon(icon_type, QColor(object_name == "dangerEntryButton" ? "#dc2626" : "#4b5563")));
        button->setIconSize(QSize(24, 24));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QToolButton::clicked, this, handler);
        layout->addWidget(button);
        return button;
    };

    auto addPlaceholderPage = [createPanel, addPageHeader](QWidget* page,
                                                          QWidget* back_page,
                                                          const QString& title_text,
                                                          const QString& feature_text) {
        QVBoxLayout* page_layout = new QVBoxLayout(page);
        page_layout->setContentsMargins(36, 30, 36, 30);
        page_layout->setSpacing(18);
        addPageHeader(page_layout, page, title_text, back_page);

        QFrame* panel = createPanel(page, "placeholderPanel");
        QVBoxLayout* panel_layout = new QVBoxLayout(panel);
        panel_layout->setContentsMargins(24, 22, 24, 22);
        panel_layout->setSpacing(10);

        QLabel* section_title = new QLabel(feature_text, panel);
        section_title->setObjectName("meSectionTitle");
        panel_layout->addWidget(section_title);

        QLabel* status = new QLabel("开发中", panel);
        status->setObjectName("meStatus");
        panel_layout->addWidget(status);

        page_layout->addWidget(panel);
        page_layout->addStretch();
    };

    QVBoxLayout* home_root_layout = new QVBoxLayout(home_page);
    home_root_layout->setContentsMargins(0, 0, 0, 0);
    home_root_layout->setSpacing(0);

    QScrollArea* home_scroll_area = new QScrollArea(home_page);
    home_scroll_area->setObjectName("meScrollArea");
    home_scroll_area->setWidgetResizable(true);
    home_scroll_area->setFrameShape(QFrame::NoFrame);
    home_scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget* home_content = new QWidget(home_scroll_area);
    home_content->setObjectName("mePage");
    QVBoxLayout* home_layout = new QVBoxLayout(home_content);
    home_layout->setContentsMargins(36, 30, 36, 30);
    home_layout->setSpacing(18);

    QLabel* title = new QLabel("我", home_content);
    title->setObjectName("meTitle");
    home_layout->addWidget(title);

    me_profile_button_ = new QToolButton(home_content);
    me_profile_button_->setObjectName("profileSummaryButton");
    me_profile_button_->setIconSize(QSize(72, 72));
    me_profile_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    me_profile_button_->setCursor(Qt::PointingHandCursor);
    connect(me_profile_button_, &QToolButton::clicked, this, [showPage, profile_page]() {
        showPage(profile_page);
    });
    home_layout->addWidget(me_profile_button_);

    QFrame* entry_panel = createPanel(home_content, "mePanel");
    QVBoxLayout* entry_layout = new QVBoxLayout(entry_panel);
    entry_layout->setContentsMargins(20, 18, 20, 20);
    entry_layout->setSpacing(10);
    addEntryButton(entry_layout, entry_panel, "favorite", "收藏", "meEntryButton",
                   [showPage, favorite_page]() { showPage(favorite_page); });
    addEntryButton(entry_layout, entry_panel, "account", "账号设置", "meEntryButton",
                   [showPage, account_page]() { showPage(account_page); });
    addEntryButton(entry_layout, entry_panel, "moments", "我的朋友圈", "meEntryButton",
                   [showPage, moments_page]() { showPage(moments_page); });
    home_layout->addWidget(entry_panel);
    home_layout->addStretch();
    home_scroll_area->setWidget(home_content);
    home_root_layout->addWidget(home_scroll_area);

    QVBoxLayout* profile_layout = new QVBoxLayout(profile_page);
    profile_layout->setContentsMargins(36, 30, 36, 30);
    profile_layout->setSpacing(18);
    addPageHeader(profile_layout, profile_page, "个人信息", home_page);

    QFrame* profile_panel = createPanel(profile_page, "profilePanel");
    QVBoxLayout* profile_panel_layout = new QVBoxLayout(profile_panel);
    profile_panel_layout->setContentsMargins(24, 22, 24, 22);
    profile_panel_layout->setSpacing(18);

    QLabel* profile_title = new QLabel("资料", profile_panel);
    profile_title->setObjectName("meSectionTitle");
    profile_panel_layout->addWidget(profile_title);

    QWidget* avatar_row = new QWidget(profile_panel);
    QHBoxLayout* avatar_layout = new QHBoxLayout(avatar_row);
    avatar_layout->setContentsMargins(0, 0, 0, 0);
    avatar_layout->setSpacing(18);

    profile_avatar_label_ = new QLabel(avatar_row);
    profile_avatar_label_->setFixedSize(96, 96);
    profile_avatar_label_->setAlignment(Qt::AlignCenter);
    avatar_layout->addWidget(profile_avatar_label_, 0, Qt::AlignTop);

    QVBoxLayout* avatar_info_layout = new QVBoxLayout;
    avatar_info_layout->setContentsMargins(0, 5, 0, 5);
    avatar_info_layout->setSpacing(8);

    avatar_status_label_ = new QLabel(avatar_row);
    avatar_status_label_->setObjectName("meStatus");
    avatar_status_label_->setText(current_avatar_url_.isEmpty() ? "未设置头像" : "头像已同步");
    avatar_info_layout->addWidget(avatar_status_label_);

    upload_avatar_button_ = new QPushButton("上传头像", avatar_row);
    upload_avatar_button_->setObjectName("avatarUploadButton");
    connect(upload_avatar_button_, &QPushButton::clicked,
            this, &MainWindow::onUploadAvatarClicked);
    avatar_info_layout->addWidget(upload_avatar_button_, 0, Qt::AlignLeft);
    avatar_info_layout->addStretch();

    avatar_layout->addLayout(avatar_info_layout, 1);
    profile_panel_layout->addWidget(avatar_row);

    QFormLayout* profile_form_layout = new QFormLayout;
    profile_form_layout->setContentsMargins(0, 0, 0, 0);
    profile_form_layout->setHorizontalSpacing(14);
    profile_form_layout->setVerticalSpacing(12);
    profile_form_layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    profile_nickname_edit_ = new QLineEdit(profile_panel);
    profile_nickname_edit_->setObjectName("profileInput");
    profile_nickname_edit_->setMaxLength(64);
    profile_nickname_edit_->setPlaceholderText("请输入昵称");
    profile_nickname_edit_->setText(user_nickname_.isEmpty() ? user_id_ : user_nickname_);

    QLabel* id_value_label = new QLabel(user_id_, profile_panel);
    id_value_label->setObjectName("meMeta");

    profile_form_layout->addRow("昵称:", profile_nickname_edit_);
    profile_form_layout->addRow("用户ID:", id_value_label);
    profile_panel_layout->addLayout(profile_form_layout);

    QWidget* profile_action_row = new QWidget(profile_panel);
    QHBoxLayout* profile_action_layout = new QHBoxLayout(profile_action_row);
    profile_action_layout->setContentsMargins(0, 0, 0, 0);
    profile_action_layout->setSpacing(12);

    profile_status_label_ = new QLabel(" ", profile_action_row);
    profile_status_label_->setObjectName("meStatus");
    profile_action_layout->addWidget(profile_status_label_, 1);

    save_profile_button_ = new QPushButton("保存", profile_action_row);
    save_profile_button_->setObjectName("profileSaveButton");
    connect(save_profile_button_, &QPushButton::clicked,
            this, &MainWindow::onSaveProfileClicked);
    profile_action_layout->addWidget(save_profile_button_, 0, Qt::AlignRight);
    profile_panel_layout->addWidget(profile_action_row);

    profile_layout->addWidget(profile_panel);
    profile_layout->addStretch();

    addPlaceholderPage(favorite_page, home_page, "收藏", "查看收藏");

    QVBoxLayout* account_layout = new QVBoxLayout(account_page);
    account_layout->setContentsMargins(36, 30, 36, 30);
    account_layout->setSpacing(18);
    addPageHeader(account_layout, account_page, "账号设置", home_page);

    QFrame* account_panel = createPanel(account_page, "mePanel");
    QVBoxLayout* account_panel_layout = new QVBoxLayout(account_panel);
    account_panel_layout->setContentsMargins(20, 18, 20, 20);
    account_panel_layout->setSpacing(10);
    addEntryButton(account_panel_layout, account_panel, "password", "修改密码", "meEntryButton",
                   [showPage, password_page]() { showPage(password_page); });
    addEntryButton(account_panel_layout, account_panel, "logout", "退出登录", "dangerEntryButton",
                   [this]() { onLogoutClicked(); });
    account_layout->addWidget(account_panel);
    account_layout->addStretch();

    QVBoxLayout* password_page_layout = new QVBoxLayout(password_page);
    password_page_layout->setContentsMargins(36, 30, 36, 30);
    password_page_layout->setSpacing(18);
    addPageHeader(password_page_layout, password_page, "修改密码", account_page);

    QFrame* password_panel = createPanel(password_page, "passwordPanel");
    QVBoxLayout* password_layout = new QVBoxLayout(password_panel);
    password_layout->setContentsMargins(24, 22, 24, 22);
    password_layout->setSpacing(16);

    QLabel* password_title = new QLabel("账号密码", password_panel);
    password_title->setObjectName("meSectionTitle");
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
    password_page_layout->addWidget(password_panel);
    password_page_layout->addStretch();

    addPlaceholderPage(moments_page, home_page, "我的朋友圈", "我的朋友圈");

    me_stack_->addWidget(home_page);
    me_stack_->addWidget(profile_page);
    me_stack_->addWidget(favorite_page);
    me_stack_->addWidget(account_page);
    me_stack_->addWidget(password_page);
    me_stack_->addWidget(moments_page);
    me_stack_->setCurrentIndex(0);

    updateAvatarPreview();
    updateMeProfileText();
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
    const QString display_name = user_nickname_.isEmpty() ? user_id_ : user_nickname_;
    if (me_profile_button_) {
        me_profile_button_->setIcon(QIcon(avatarPixmapFromValue(current_avatar_url_, display_name, 72)));
    }
    if (profile_avatar_label_) {
        profile_avatar_label_->setPixmap(avatarPixmapFromValue(current_avatar_url_, display_name, 96));
    }
}

void MainWindow::updateMeProfileText() {
    const QString display_name = user_nickname_.isEmpty() ? user_id_ : user_nickname_;
    if (me_profile_button_) {
        me_profile_button_->setText(QString("%1\n用户ID：%2").arg(display_name, user_id_));
    }
    if (profile_nickname_edit_ && !profile_nickname_edit_->hasFocus()) {
        profile_nickname_edit_->setText(display_name);
    }
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
    if (avatar_status_label_) {
        avatar_status_label_->setText("正在同步头像...");
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
        if (avatar_status_label_) {
            avatar_status_label_->setText(error);
        }
        QMessageBox::warning(this, "上传头像", error);
        return;
    }

    if (!avatar_url.isEmpty()) {
        current_avatar_url_ = avatar_url;
    }
    updateAvatarPreview();
    if (!current_messages_.isEmpty()) {
        renderChatMessages(false);
    }
    if (avatar_status_label_) {
        avatar_status_label_->setText("头像已同步");
    }
}

void MainWindow::onSaveProfileClicked() {
    if (!tcp_client_ || tcp_client_->state() != ClientState::LoggedIn) {
        QMessageBox::warning(this, "保存资料", "当前未登录，无法保存资料");
        return;
    }

    const QString nickname = profile_nickname_edit_
        ? profile_nickname_edit_->text().trimmed()
        : QString();

    auto showProfileStatus = [this](const QString& text) {
        if (profile_status_label_) {
            profile_status_label_->setText(text);
        }
    };

    if (nickname.isEmpty()) {
        showProfileStatus("请输入昵称");
        if (profile_nickname_edit_) {
            profile_nickname_edit_->setFocus();
        }
        return;
    }

    if (nickname == user_nickname_ || (user_nickname_.isEmpty() && nickname == user_id_)) {
        showProfileStatus("已保存");
        return;
    }

    if (save_profile_button_) {
        save_profile_button_->setEnabled(false);
    }
    profile_save_pending_ = true;
    showProfileStatus("正在保存...");
    tcp_client_->updateProfile(nickname);

    QTimer::singleShot(10000, this, [this]() {
        if (!profile_save_pending_) {
            return;
        }
        profile_save_pending_ = false;
        if (save_profile_button_) {
            save_profile_button_->setEnabled(true);
        }
        if (profile_status_label_) {
            profile_status_label_->setText("保存超时，请确认服务端已更新");
        }
    });
}

void MainWindow::onProfileUpdateResult(int code, const QString& message, const QString& nickname) {
    profile_save_pending_ = false;
    if (save_profile_button_) {
        save_profile_button_->setEnabled(true);
    }

    const QString result_message = message.isEmpty()
        ? (code == 0 ? "资料已保存" : "资料保存失败")
        : message;

    if (code != 0) {
        if (profile_status_label_) {
            profile_status_label_->setText(result_message);
        }
        QMessageBox::warning(this, "保存资料", result_message);
        return;
    }

    const QString saved_nickname = nickname.isEmpty() && profile_nickname_edit_
        ? profile_nickname_edit_->text().trimmed()
        : nickname;
    if (!saved_nickname.isEmpty()) {
        user_nickname_ = saved_nickname;
        setWindowTitle(QString("IChat - %1").arg(user_nickname_));
    }

    updateMeProfileText();
    updateAvatarPreview();
    if (!current_messages_.isEmpty()) {
        renderChatMessages(false);
    }
    if (profile_status_label_) {
        profile_status_label_->setText(result_message);
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
