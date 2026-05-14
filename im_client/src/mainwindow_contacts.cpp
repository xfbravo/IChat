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
#include <QDebug>

using namespace mainwindow_detail;

namespace
{

    class ContactListItemWidget : public QWidget
    {
    public:
        ContactListItemWidget(const QString &title,
                              const QString &subtitle,
                              const QString &avatar_url,
                              QWidget *parent = nullptr)
            : QWidget(parent)
        {
            setAttribute(Qt::WA_StyledBackground, true);
            setAttribute(Qt::WA_TransparentForMouseEvents, true);
            setStyleSheet("background: transparent;");

            constexpr int avatar_size = 44;
            QLabel *avatar_label = new QLabel(this);
            avatar_label->setFixedSize(avatar_size, avatar_size);
            avatar_label->setPixmap(avatarPixmapFromValue(avatar_url, title, avatar_size));
            avatar_label->setAlignment(Qt::AlignCenter);

            QLabel *title_label = new QLabel(title, this);
            title_label->setTextFormat(Qt::PlainText);
            title_label->setWordWrap(false);
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
            layout->addWidget(avatar_label, 0, Qt::AlignVCenter);
            layout->addLayout(text_layout, 1);
        }
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
