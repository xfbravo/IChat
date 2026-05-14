/**
 * @file mainwindow_contacts.cpp
 * @brief MainWindow 联系人、好友请求和联系人备注
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
#include <QAbstractItemView>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPen>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QSize>
#include <QSizePolicy>
#include <QStringList>
#include <QToolButton>
#include <QTimer>
#include <QBuffer>
#include <QFileDialog>
#include <QFont>
#include <QImage>
#include <QImageReader>
#include <QIODevice>
#include <QMouseEvent>
#include <algorithm>
#include <functional>
#include "mainwindow_helpers.h"
#include <QDebug>

using namespace mainwindow_detail;

namespace
{
    QString profileTextOrUnset(const QString &text)
    {
        const QString trimmed = text.trimmed();
        return trimmed.isEmpty() ? QStringLiteral("未填写") : trimmed;
    }

    QLabel *createProfileValueLabel(QWidget *parent)
    {
        QLabel *label = new QLabel(parent);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setStyleSheet(R"(
            QLabel {
                color: #111827;
                font-size: 14px;
                background: transparent;
            }
        )");
        return label;
    }

    class ContactListItemWidget : public QWidget
    {
    public:
        ContactListItemWidget(const QString &title,
                              const QString &subtitle,
                              const QString &avatar_url,
                              std::function<void()> avatar_handler,
                              std::function<void()> double_click_handler,
                              QWidget *parent = nullptr)
            : QWidget(parent)
            , double_click_handler_(std::move(double_click_handler))
        {
            setAttribute(Qt::WA_StyledBackground, true);
            setStyleSheet("background: transparent;");

            constexpr int avatar_size = 44;
            QToolButton *avatar_button = new QToolButton(this);
            avatar_button->setFixedSize(avatar_size, avatar_size);
            avatar_button->setIcon(QIcon(avatarPixmapFromValue(avatar_url, title, avatar_size)));
            avatar_button->setIconSize(QSize(avatar_size, avatar_size));
            avatar_button->setCursor(Qt::PointingHandCursor);
            avatar_button->setToolTip("查看个人信息");
            avatar_button->setStyleSheet(R"(
                QToolButton {
                    padding: 0;
                    border: none;
                    border-radius: 22px;
                    background: transparent;
                }
                QToolButton:hover {
                    background-color: #eeeeee;
                }
            )");
            QObject::connect(avatar_button, &QToolButton::clicked, this, [avatar_handler = std::move(avatar_handler)]() {
                if (avatar_handler) {
                    avatar_handler();
                }
            });

            QLabel *title_label = new QLabel(title, this);
            title_label->setTextFormat(Qt::PlainText);
            title_label->setWordWrap(false);
            title_label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            title_label->setStyleSheet(R"(
            QLabel {
                color: #111111;
                font-size: 15px;
                font-weight: 600;
                background: transparent;
            }
        )");

            QLabel *subtitle_label = new QLabel(subtitle, this);
            subtitle_label->setTextFormat(Qt::PlainText);
            subtitle_label->setWordWrap(false);
            subtitle_label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            subtitle_label->setStyleSheet(R"(
            QLabel {
                color: #777777;
                font-size: 12px;
                background: transparent;
            }
        )");

            QVBoxLayout *text_layout = new QVBoxLayout;
            text_layout->setContentsMargins(0, 0, 0, 0);
            text_layout->setSpacing(4);
            text_layout->addWidget(title_label);
            text_layout->addWidget(subtitle_label);

            QHBoxLayout *layout = new QHBoxLayout(this);
            layout->setContentsMargins(16, 10, 16, 10);
            layout->setSpacing(12);
            layout->addWidget(avatar_button, 0, Qt::AlignVCenter);
            layout->addLayout(text_layout, 1);
        }

    protected:
        void mouseDoubleClickEvent(QMouseEvent *event) override
        {
            if (double_click_handler_) {
                double_click_handler_();
                event->accept();
                return;
            }
            QWidget::mouseDoubleClickEvent(event);
        }

    private:
        std::function<void()> double_click_handler_;
    };

} // namespace

void MainWindow::createContactView()
{
    contact_view_ = new QWidget;

    QVBoxLayout *layout = new QVBoxLayout(contact_view_);
    layout->setContentsMargins(0, 0, 0, 0);

    // 顶部按钮
    QWidget *top_widget = new QWidget;
    top_widget->setFixedHeight(60);
    QHBoxLayout *top_layout = new QHBoxLayout(top_widget);
    top_layout->setContentsMargins(20, 0, 16, 0);
    top_layout->setSpacing(10);

    QLabel *title_label = new QLabel("联系人", top_widget);
    title_label->setStyleSheet(R"(
        QLabel {
            color: #111111;
            font-family: "SimHei", "Microsoft YaHei", sans-serif;
            font-size: 20px;
            font-weight: 700;
            background: transparent;
        }
    )");
    top_layout->addWidget(title_label);
    top_layout->addStretch();

    add_contact_button_ = new QPushButton("添加联系人");
    add_contact_button_->setStyleSheet(R"(
        QPushButton {
            padding: 10px 20px;
            background-color: #4CAF50;
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #45a049;
        }
    )");
    connect(add_contact_button_, &QPushButton::clicked,
            this, &MainWindow::onAddContactClicked);
    top_layout->addWidget(add_contact_button_);

    QPushButton *view_requests_button = new QPushButton("查看好友请求");
    view_requests_button->setStyleSheet(R"(
        QPushButton {
            padding: 10px 20px;
            background-color: #2196F3;
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #1976D2;
        }
    )");
    connect(view_requests_button, &QPushButton::clicked,
            this, &MainWindow::onViewFriendRequestsClicked);
    top_layout->addWidget(view_requests_button);

    layout->addWidget(top_widget);

    // 联系人树
    contact_tree_widget_ = new QTreeWidget;
    contact_tree_widget_->setColumnCount(1);
    contact_tree_widget_->setHeaderHidden(true);
    contact_tree_widget_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    contact_tree_widget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contact_tree_widget_->setRootIsDecorated(false);
    contact_tree_widget_->setIndentation(0);
    contact_tree_widget_->setFocusPolicy(Qt::NoFocus);
    contact_tree_widget_->setSelectionMode(QAbstractItemView::SingleSelection);
    contact_tree_widget_->setCursor(Qt::PointingHandCursor);
    contact_tree_widget_->setStyleSheet(R"(
        QTreeWidget {
            border: none;
            background-color: white;
            font-size: 14px;
            outline: none;
        }
        QTreeWidget::item {
            padding: 0;
            border-bottom: 1px solid #eeeeee;
        }
        QTreeWidget::item:hover {
            background-color: #f5f5f5;
        }
        QTreeWidget::item:selected,
        QTreeWidget::item:selected:active,
        QTreeWidget::item:selected:!active {
            background-color: #e8f5e9;
            color: #111111;
        }
    )");
    connect(contact_tree_widget_, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onContactItemDoubleClicked);

    layout->addWidget(contact_tree_widget_);
}

void MainWindow::onAddContactClicked()
{
    AddContactDialog dialog(tcp_client_, this);
    dialog.exec();
}

void MainWindow::onContactItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    QString user_id = item->data(0, Qt::UserRole).toString();
    QString nickname = item->data(0, Qt::AccessibleTextRole).toString();
    if (nickname.isEmpty())
    {
        nickname = conversationTitle(user_id);
    }
    if (!user_id.isEmpty())
    {
        // 联系人页不单独维护聊天状态，双击后复用消息页的会话切换逻辑。
        switchToChatWith(user_id, nickname);
        nav_list_->setCurrentRow(0); // 切换到消息视图
        content_stacked_->setCurrentWidget(message_view_);
    }
}

void MainWindow::showUserProfile(const QString &user_id)
{
    const QString target_id = user_id.trimmed();
    if (target_id.isEmpty())
    {
        return;
    }

    const bool can_request = tcp_client_ && tcp_client_->state() == ClientState::LoggedIn;
    showUserProfileDialog(target_id,
                          cachedUserProfile(target_id),
                          can_request,
                          can_request ? QString("正在获取最新资料...") : QString("当前离线，显示本地资料"));

    if (can_request)
    {
        tcp_client_->getUserProfile(target_id);
    }
}

void MainWindow::showUserProfileDialog(const QString &user_id,
                                       const UserProfileCache &profile,
                                       bool loading,
                                       const QString &status_text)
{
    if (!user_profile_dialog_)
    {
        user_profile_dialog_ = new QDialog(this);
        user_profile_dialog_->setAttribute(Qt::WA_DeleteOnClose, true);
        user_profile_dialog_->setWindowTitle("个人信息");
        user_profile_dialog_->setMinimumWidth(380);
        user_profile_dialog_->setStyleSheet(R"(
            QDialog {
                background-color: #f7f8fa;
                font-family: "Microsoft YaHei", sans-serif;
            }
            QFrame#profileCard,
            QFrame#profileEntryPanel {
                background-color: #ffffff;
                border: 1px solid #e5e7eb;
                border-radius: 6px;
            }
            QLabel#profileName {
                color: #111827;
                font-size: 18px;
                font-weight: 700;
                background: transparent;
            }
            QLabel#profileStatus {
                color: #6b7280;
                font-size: 12px;
                background: transparent;
            }
            QToolButton#profileMomentButton {
                min-height: 54px;
                padding: 0 18px;
                color: #111827;
                background-color: #ffffff;
                border: none;
                border-radius: 4px;
                font-size: 15px;
                text-align: left;
            }
            QToolButton#profileMomentButton:hover {
                background-color: #f3f4f6;
            }
            QPushButton#profileCloseButton {
                min-width: 86px;
                padding: 8px 16px;
                color: #374151;
                background-color: #ffffff;
                border: 1px solid #d1d5db;
                border-radius: 4px;
                font-size: 14px;
                font-weight: 600;
            }
            QPushButton#profileCloseButton:hover {
                background-color: #f3f4f6;
            }
            QPushButton#profileMessageButton {
                min-width: 96px;
                padding: 8px 18px;
                color: #ffffff;
                background-color: #4CAF50;
                border: none;
                border-radius: 4px;
                font-size: 14px;
                font-weight: 600;
            }
            QPushButton#profileMessageButton:hover {
                background-color: #45a049;
            }
        )");

        QVBoxLayout *root_layout = new QVBoxLayout(user_profile_dialog_);
        root_layout->setContentsMargins(18, 18, 18, 18);
        root_layout->setSpacing(14);

        QFrame *card = new QFrame(user_profile_dialog_);
        card->setObjectName("profileCard");
        QVBoxLayout *card_layout = new QVBoxLayout(card);
        card_layout->setContentsMargins(20, 18, 20, 18);
        card_layout->setSpacing(16);

        QWidget *summary = new QWidget(card);
        QHBoxLayout *summary_layout = new QHBoxLayout(summary);
        summary_layout->setContentsMargins(0, 0, 0, 0);
        summary_layout->setSpacing(14);

        view_profile_avatar_label_ = new QLabel(summary);
        view_profile_avatar_label_->setFixedSize(72, 72);
        view_profile_avatar_label_->setAlignment(Qt::AlignCenter);
        summary_layout->addWidget(view_profile_avatar_label_, 0, Qt::AlignTop);

        QVBoxLayout *summary_text_layout = new QVBoxLayout;
        summary_text_layout->setContentsMargins(0, 4, 0, 4);
        summary_text_layout->setSpacing(6);
        view_profile_name_label_ = new QLabel(summary);
        view_profile_name_label_->setObjectName("profileName");
        view_profile_name_label_->setTextFormat(Qt::PlainText);
        summary_text_layout->addWidget(view_profile_name_label_);
        view_profile_id_label_ = createProfileValueLabel(summary);
        summary_text_layout->addWidget(view_profile_id_label_);
        summary_text_layout->addStretch();
        summary_layout->addLayout(summary_text_layout, 1);
        card_layout->addWidget(summary);

        QFormLayout *form_layout = new QFormLayout;
        form_layout->setContentsMargins(0, 0, 0, 0);
        form_layout->setHorizontalSpacing(12);
        form_layout->setVerticalSpacing(10);
        form_layout->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);

        view_profile_nickname_label_ = createProfileValueLabel(card);
        view_profile_remark_label_ = createProfileValueLabel(card);
        view_profile_gender_label_ = createProfileValueLabel(card);
        view_profile_region_label_ = createProfileValueLabel(card);
        view_profile_signature_label_ = createProfileValueLabel(card);
        form_layout->addRow("昵称:", view_profile_nickname_label_);
        form_layout->addRow("备注:", view_profile_remark_label_);
        form_layout->addRow("性别:", view_profile_gender_label_);
        form_layout->addRow("地区:", view_profile_region_label_);
        form_layout->addRow("签名:", view_profile_signature_label_);
        card_layout->addLayout(form_layout);

        view_profile_status_label_ = new QLabel(card);
        view_profile_status_label_->setObjectName("profileStatus");
        view_profile_status_label_->setTextFormat(Qt::PlainText);
        card_layout->addWidget(view_profile_status_label_);
        root_layout->addWidget(card);

        QFrame *entry_panel = new QFrame(user_profile_dialog_);
        entry_panel->setObjectName("profileEntryPanel");
        QVBoxLayout *entry_layout = new QVBoxLayout(entry_panel);
        entry_layout->setContentsMargins(20, 18, 20, 18);
        entry_layout->setSpacing(10);

        QToolButton *moments_button = new QToolButton(entry_panel);
        moments_button->setObjectName("profileMomentButton");
        moments_button->setText("朋友圈");
        moments_button->setIcon(lineIcon("moments", QColor("#4b5563")));
        moments_button->setIconSize(QSize(24, 24));
        moments_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        moments_button->setCursor(Qt::PointingHandCursor);
        moments_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        entry_layout->addWidget(moments_button);
        root_layout->addWidget(entry_panel);

        QWidget *action_row = new QWidget(user_profile_dialog_);
        QHBoxLayout *action_layout = new QHBoxLayout(action_row);
        action_layout->setContentsMargins(0, 0, 0, 0);
        action_layout->setSpacing(10);
        action_layout->addStretch();

        QPushButton *close_button = new QPushButton("关闭", action_row);
        close_button->setObjectName("profileCloseButton");
        connect(close_button, &QPushButton::clicked, user_profile_dialog_, &QDialog::accept);
        action_layout->addWidget(close_button);

        view_profile_message_button_ = new QPushButton("发消息", action_row);
        view_profile_message_button_->setObjectName("profileMessageButton");
        connect(view_profile_message_button_, &QPushButton::clicked, this, [this]()
        {
            const QString target_id = profile_dialog_user_id_;
            if (target_id.isEmpty() || target_id == user_id_)
            {
                return;
            }

            const UserProfileCache profile = cachedUserProfile(target_id);
            QString display_name = profile.display_name.trimmed();
            if (display_name.isEmpty())
            {
                const QString nickname = profile.nickname.trimmed();
                const QString remark = profile.remark.trimmed();
                display_name = remark.isEmpty() ? nickname : remark;
            }
            if (display_name.isEmpty())
            {
                display_name = conversationTitle(target_id);
            }
            if (display_name.isEmpty())
            {
                display_name = target_id;
            }

            switchToChatWith(target_id, display_name);
            if (nav_list_)
            {
                nav_list_->setCurrentRow(0);
            }
            if (content_stacked_ && message_view_)
            {
                content_stacked_->setCurrentWidget(message_view_);
            }
            if (user_profile_dialog_)
            {
                user_profile_dialog_->accept();
            }
        });
        action_layout->addWidget(view_profile_message_button_);
        root_layout->addWidget(action_row);

        connect(user_profile_dialog_, &QDialog::destroyed, this, [this]()
        {
            user_profile_dialog_ = nullptr;
            profile_dialog_user_id_.clear();
            view_profile_avatar_label_ = nullptr;
            view_profile_name_label_ = nullptr;
            view_profile_id_label_ = nullptr;
            view_profile_nickname_label_ = nullptr;
            view_profile_remark_label_ = nullptr;
            view_profile_gender_label_ = nullptr;
            view_profile_region_label_ = nullptr;
            view_profile_signature_label_ = nullptr;
            view_profile_status_label_ = nullptr;
            view_profile_message_button_ = nullptr;
        });
    }

    profile_dialog_user_id_ = user_id;
    updateUserProfileDialog(profile, loading, status_text);
    user_profile_dialog_->show();
    user_profile_dialog_->raise();
    user_profile_dialog_->activateWindow();
}

void MainWindow::updateUserProfileDialog(const UserProfileCache &profile,
                                         bool loading,
                                         const QString &status_text)
{
    if (!user_profile_dialog_)
    {
        return;
    }

    const QString display_name = profile.display_name.trimmed().isEmpty()
        ? (profile.nickname.trimmed().isEmpty() ? profile.user_id : profile.nickname.trimmed())
        : profile.display_name.trimmed();

    if (view_profile_avatar_label_)
    {
        view_profile_avatar_label_->setPixmap(avatarPixmapFromValue(profile.avatar_url, display_name, 72));
    }
    if (view_profile_name_label_)
    {
        view_profile_name_label_->setText(display_name);
    }
    if (view_profile_id_label_)
    {
        view_profile_id_label_->setText(QString("用户ID: %1").arg(profile.user_id));
    }
    if (view_profile_nickname_label_)
    {
        view_profile_nickname_label_->setText(profileTextOrUnset(profile.nickname));
    }
    if (view_profile_remark_label_)
    {
        view_profile_remark_label_->setText(profileTextOrUnset(profile.remark));
    }
    if (view_profile_gender_label_)
    {
        view_profile_gender_label_->setText(profileTextOrUnset(profile.gender));
    }
    if (view_profile_region_label_)
    {
        view_profile_region_label_->setText(profileTextOrUnset(profile.region));
    }
    if (view_profile_signature_label_)
    {
        view_profile_signature_label_->setText(profileTextOrUnset(profile.signature));
    }
    if (view_profile_status_label_)
    {
        if (!status_text.isEmpty())
        {
            view_profile_status_label_->setText(status_text);
        }
        else
        {
            view_profile_status_label_->setText(loading ? "正在获取最新资料..." : "资料已更新");
        }
    }
    if (view_profile_message_button_)
    {
        const bool can_message = !profile.user_id.isEmpty() && profile.user_id != user_id_;
        view_profile_message_button_->setVisible(can_message);
        view_profile_message_button_->setEnabled(can_message);
    }
}

MainWindow::UserProfileCache MainWindow::cachedUserProfile(const QString &user_id) const
{
    UserProfileCache profile = user_profile_cache_.value(user_id);
    profile.user_id = user_id;

    if (user_id == user_id_)
    {
        profile.nickname = user_nickname_;
        profile.display_name = user_nickname_.trimmed().isEmpty() ? user_id_ : user_nickname_.trimmed();
        profile.avatar_url = current_avatar_url_;
        profile.gender = user_gender_;
        profile.region = user_region_;
        profile.signature = user_signature_;
        return profile;
    }

    const QString remark = contact_remarks_.value(user_id).trimmed();
    const QString nickname = contact_nicknames_.value(user_id).trimmed();
    profile.remark = remark;
    if (!nickname.isEmpty())
    {
        profile.nickname = nickname;
    }
    const QString avatar_url = contact_avatars_.value(user_id);
    if (!avatar_url.isEmpty())
    {
        profile.avatar_url = avatar_url;
    }

    if (!profile.remark.trimmed().isEmpty())
    {
        profile.display_name = profile.remark.trimmed();
    }
    else if (!profile.nickname.trimmed().isEmpty())
    {
        profile.display_name = profile.nickname.trimmed();
    }
    else if (profile.display_name.isEmpty())
    {
        profile.display_name = conversationTitle(user_id);
    }
    if (profile.display_name.isEmpty())
    {
        profile.display_name = user_id;
    }
    return profile;
}

void MainWindow::mergeUserProfileCache(const UserProfileCache &profile)
{
    if (profile.user_id.isEmpty())
    {
        return;
    }

    user_profile_cache_[profile.user_id] = profile;
    if (profile.user_id == user_id_)
    {
        if (!profile.nickname.isEmpty())
        {
            user_nickname_ = profile.nickname;
            setWindowTitle(QString("IChat - %1").arg(user_nickname_));
        }
        if (!profile.avatar_url.isEmpty())
        {
            current_avatar_url_ = profile.avatar_url;
        }
        user_gender_ = profile.gender;
        user_region_ = profile.region;
        user_signature_ = profile.signature;
        updateMeProfileText();
        updateAvatarPreview();
        if (!current_messages_.isEmpty())
        {
            renderChatMessages(false);
        }
        return;
    }

    if (!profile.nickname.isEmpty())
    {
        contact_nicknames_[profile.user_id] = profile.nickname;
    }
    if (!profile.avatar_url.isEmpty())
    {
        contact_avatars_[profile.user_id] = profile.avatar_url;
    }

    auto conversation_it = conversations_.find(profile.user_id);
    if (conversation_it != conversations_.end())
    {
        const QString remark = contact_remarks_.value(profile.user_id).trimmed();
        const QString display_name = remark.isEmpty()
            ? (profile.nickname.isEmpty() ? profile.user_id : profile.nickname)
            : remark;
        conversation_it->title = display_name;
        if (profile.user_id == current_chat_target_ && chat_target_label_)
        {
            chat_target_label_->setText(display_name);
        }
    }

    refreshConversationList();
    rebuildContactList();
    if (!current_messages_.isEmpty())
    {
        renderChatMessages(false);
    }
}

void MainWindow::onUserProfileReceived(int code,
                                       const QString &message,
                                       const QString &user_id,
                                       const QString &nickname,
                                       const QString &avatar_url,
                                       const QString &gender,
                                       const QString &region,
                                       const QString &signature)
{
    const QString target_id = user_id.isEmpty() ? profile_dialog_user_id_ : user_id;
    if (target_id.isEmpty())
    {
        return;
    }

    if (code != 0)
    {
        if (target_id == user_id_ && profile_status_label_)
        {
            profile_status_label_->setText(message.isEmpty() ? QString("获取资料失败") : message);
        }
        if (target_id == profile_dialog_user_id_)
        {
            const QString text = message.isEmpty() ? QString("获取资料失败") : message;
            updateUserProfileDialog(cachedUserProfile(target_id), false, text);
        }
        return;
    }

    UserProfileCache profile = cachedUserProfile(target_id);
    profile.user_id = target_id;
    profile.nickname = nickname;
    profile.avatar_url = avatar_url;
    profile.gender = gender;
    profile.region = region;
    profile.signature = signature;
    profile.display_name = profile.remark.trimmed().isEmpty()
        ? (nickname.trimmed().isEmpty() ? target_id : nickname.trimmed())
        : profile.remark.trimmed();

    mergeUserProfileCache(profile);
    if (target_id == user_id_ && profile_status_label_ && !profile_save_pending_)
    {
        profile_status_label_->setText(message.isEmpty() ? QString("资料已更新") : message);
    }
    if (target_id == profile_dialog_user_id_)
    {
        updateUserProfileDialog(profile, false, message.isEmpty() ? QString("资料已更新") : message);
    }
}

void MainWindow::rebuildContactList()
{
    if (!contact_tree_widget_)
        return;

    contact_tree_widget_->clear();

    QList<QString> contact_ids;
    if (chat_list_widget_)
    {
        for (int i = 0; i < chat_list_widget_->count(); ++i)
        {
            QListWidgetItem *chat_item = chat_list_widget_->item(i);
            const QString friend_id = chat_item ? chat_item->data(Qt::UserRole).toString() : QString();
            if (!friend_id.isEmpty())
            {
                contact_ids.append(friend_id);
            }
        }
    }

    if (contact_ids.isEmpty())
    {
        contact_ids = conversations_.keys();
        std::stable_sort(contact_ids.begin(), contact_ids.end(),
                         [this](const QString &left, const QString &right)
                         {
                             return conversationTitle(left).localeAwareCompare(conversationTitle(right)) < 0;
                         });
    }

    for (const QString &friend_id : contact_ids)
    {
        if (friend_id.isEmpty())
            continue;

        const QString saved_remark = contact_remarks_.value(friend_id).trimmed();
        const QString nickname = contact_nicknames_.value(friend_id).trimmed();
        QString title = saved_remark.isEmpty() ? nickname : saved_remark;
        if (title.isEmpty())
        {
            title = conversationTitle(friend_id);
        }
        if (title.isEmpty())
        {
            title = friend_id;
        }

        const QString subtitle = !saved_remark.isEmpty() && !nickname.isEmpty() && nickname != saved_remark
                                     ? QString("昵称: %1").arg(nickname)
                                     : QString("账号: %1").arg(friend_id);

        QTreeWidgetItem *friend_item = new QTreeWidgetItem(contact_tree_widget_);
        friend_item->setText(0, QString());
        friend_item->setData(0, Qt::UserRole, friend_id);
        friend_item->setData(0, Qt::AccessibleTextRole, title);
        friend_item->setToolTip(0, title);
        friend_item->setSizeHint(0, QSize(0, 68));

        ContactListItemWidget *item_widget = new ContactListItemWidget(
            title,
            subtitle,
            contact_avatars_.value(friend_id),
            [this, friend_id]()
            {
                showUserProfile(friend_id);
            },
            [this, friend_id, title]()
            {
                switchToChatWith(friend_id, title);
                nav_list_->setCurrentRow(0);
                content_stacked_->setCurrentWidget(message_view_);
            },
            contact_tree_widget_);
        contact_tree_widget_->setItemWidget(friend_item, 0, item_widget);
    }
}

void MainWindow::onFriendListReceived(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray())
        return;

    QJsonArray friends = doc.array();

    // 好友列表同时驱动联系人树和消息页会话列表，避免两个页面各拉一份数据。
    for (const QJsonValue &value : friends)
    {
        QJsonObject friend_obj = value.toObject();
        QString friend_id = friend_obj["friend_id"].toString();
        QString nickname = friend_obj["nickname"].toString();
        QString remark = friend_obj["remark"].toString().trimmed();
        QString avatar_url = friend_obj["avatar_url"].toString();
        if (!nickname.isEmpty())
        {
            contact_nicknames_[friend_id] = nickname;
        }
        if (remark.isEmpty())
        {
            contact_remarks_.remove(friend_id);
        }
        else
        {
            contact_remarks_[friend_id] = remark;
        }
        const QString fallback_nickname = nickname.isEmpty() ? contact_nicknames_.value(friend_id) : nickname;
        QString display_name = contact_remarks_.value(friend_id, fallback_nickname);
        if (display_name.isEmpty())
        {
            display_name = friend_id;
        }
        QString last_message = friend_obj["last_msg_content"].toString();
        QString last_time = friend_obj["last_msg_time"].toString();
        qint64 last_timestamp = friend_obj["last_msg_timestamp"].toInteger();
        if (last_timestamp <= 0)
        {
            last_timestamp = timestampFromText(last_time);
        }

        ConversationState &conversation = conversations_[friend_id];
        conversation.title = display_name;
        contact_avatars_[friend_id] = avatar_url;
        if (last_timestamp > 0 && (last_timestamp > conversation.last_timestamp || conversation.last_timestamp <= 0))
        {
            conversation.last_message = last_message;
            conversation.last_timestamp = last_timestamp;
        }
    }
    refreshConversationList();

    if (!current_chat_target_.isEmpty() && conversations_.contains(current_chat_target_))
    {
        chat_target_label_->setText(conversations_[current_chat_target_].title);
        renderChatMessages(false);
    }

    // 更新联系人列表（不再调用 getFriendList，避免收到列表后再次请求形成循环）。
    rebuildContactList();
}

void MainWindow::onFriendRequestReceived(const QString &from_user_id,
                                         const QString &from_nickname,
                                         const QString &message)
{
    QString info = QString("来自 %1 的好友请求: %2").arg(from_nickname, message);
    QMessageBox::information(this, "好友请求", info);
}

void MainWindow::onViewFriendRequestsClicked()
{
    tcp_client_->getFriendRequests();
}

void MainWindow::onFriendRequestsReceived(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray())
        return;

    QJsonArray requests = doc.array();
    if (requests.isEmpty())
    {
        QMessageBox::information(this, "好友请求", "没有待处理的好友请求");
        return;
    }

    // 好友请求是一次性处理弹窗；后续如果要做“新的朋友”页面，可从这里拆出独立组件。
    QDialog dialog(this);
    dialog.setWindowTitle("好友请求列表");
    dialog.setMinimumSize(400, 300);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *title = new QLabel(QString("收到 %1 个好友请求").arg(requests.size()));
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(title);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    QWidget *container = new QWidget;
    QVBoxLayout *containerLayout = new QVBoxLayout(container);

    struct RequestItem
    {
        QString request_id;
        QString from_user_id;
        QString from_nickname;
        QString remark;
    };
    QList<RequestItem> requestItems;

    for (const QJsonValue &value : requests)
    {
        QJsonObject obj = value.toObject();
        QString request_id = obj["request_id"].toString();
        QString from_user_id = obj["from_user_id"].toString();
        QString from_nickname = obj["from_nickname"].toString();
        QString remark = obj["remark"].toString();

        RequestItem item;
        item.request_id = request_id;
        item.from_user_id = from_user_id;
        item.from_nickname = from_nickname;
        item.remark = remark;
        requestItems.append(item);

        QFrame *frame = new QFrame;
        frame->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
        frame->setStyleSheet("QFrame { background-color: #f5f5f5; border-radius: 4px; padding: 10px; margin: 5px 0; }");

        QVBoxLayout *frameLayout = new QVBoxLayout(frame);

        QLabel *nameLabel = new QLabel(from_nickname);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");

        QLabel *remarkLabel = new QLabel(remark.isEmpty() ? "无备注" : remark);
        remarkLabel->setStyleSheet("color: #666; font-size: 12px;");

        QHBoxLayout *btnLayout = new QHBoxLayout;
        QPushButton *acceptBtn = new QPushButton("同意");
        acceptBtn->setStyleSheet(R"(
            QPushButton { background-color: #4CAF50; color: white; border: none; padding: 8px 16px; border-radius: 4px; }
            QPushButton:hover { background-color: #45a049; }
        )");
        QPushButton *rejectBtn = new QPushButton("拒绝");
        rejectBtn->setStyleSheet(R"(
            QPushButton { background-color: #f44336; color: white; border: none; padding: 8px 16px; border-radius: 4px; }
            QPushButton:hover { background-color: #e53935; }
        )");

        // 处理后禁用按钮，避免用户在服务端响应前重复提交同一个请求。
        QObject::connect(acceptBtn, &QPushButton::clicked, this, [this, request_id, acceptBtn, rejectBtn, &dialog]()
                         {
            qDebug() << "同意按钮 clicked, request_id:" << request_id;
            tcp_client_->respondFriendRequest(request_id, true);
            acceptBtn->setEnabled(false);
            rejectBtn->setEnabled(false);
            acceptBtn->setText("已同意");
            // 刷新好友列表
            loadChatList();
            // 延迟关闭对话框，让用户看到结果
            QTimer::singleShot(500, &dialog, &QDialog::accept); });
        QObject::connect(rejectBtn, &QPushButton::clicked, this, [this, request_id, acceptBtn, rejectBtn, &dialog]()
                         {
            tcp_client_->respondFriendRequest(request_id, false);
            acceptBtn->setEnabled(false);
            rejectBtn->setEnabled(false);
            rejectBtn->setText("已拒绝");
            QTimer::singleShot(500, &dialog, &QDialog::accept); });

        btnLayout->addStretch();
        btnLayout->addWidget(acceptBtn);
        btnLayout->addWidget(rejectBtn);

        frameLayout->addWidget(nameLabel);
        frameLayout->addWidget(remarkLabel);
        frameLayout->addLayout(btnLayout);

        containerLayout->addWidget(frame);
    }

    containerLayout->addStretch();
    scrollArea->setWidget(container);
    layout->addWidget(scrollArea);

    QPushButton *closeBtn = new QPushButton("关闭");
    closeBtn->setStyleSheet(R"(
        QPushButton { padding: 10px 20px; background-color: #9e9e9e; color: white; border: none; border-radius: 4px; }
        QPushButton:hover { background-color: #757575; }
    )");
    layout->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

void MainWindow::onFriendRemarkUpdateResult(int code, const QString &message,
                                            const QString &friend_id, const QString &remark)
{
    if (code != 0)
    {
        QMessageBox::warning(this, "修改联系人备注", message.isEmpty() ? "修改备注失败" : message);
        return;
    }

    if (friend_id.isEmpty())
    {
        return;
    }

    // 备注更新成功后立即同步到本地缓存、会话标题和联系人树。
    const QString trimmed_remark = remark.trimmed();
    const QString fallback_nickname = contact_nicknames_.value(friend_id).trimmed();
    const QString display_name = trimmed_remark.isEmpty()
                                     ? (fallback_nickname.isEmpty() ? friend_id : fallback_nickname)
                                     : trimmed_remark;
    if (trimmed_remark.isEmpty())
    {
        contact_remarks_.remove(friend_id);
    }
    else
    {
        contact_remarks_[friend_id] = trimmed_remark;
    }
    conversations_[friend_id].title = display_name;
    if (friend_id == current_chat_target_)
    {
        chat_target_label_->setText(display_name);
    }
    updateConversationItem(friend_id);
    rebuildContactList();
}

void MainWindow::loadContacts()
{
    // 如果聊天列表已有数据，直接从 conversations_ 构造联系人树，减少一次网络请求。
    if (chat_list_widget_->count() > 0)
    {
        rebuildContactList();
    }
    else
    {
        // 否则请求服务器获取好友列表
        tcp_client_->getFriendList();
    }
}
