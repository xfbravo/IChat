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

void MainWindow::createContactView() {
    contact_view_ = new QWidget;

    QVBoxLayout* layout = new QVBoxLayout(contact_view_);
    layout->setContentsMargins(0, 0, 0, 0);

    // 顶部按钮
    QWidget* top_widget = new QWidget;
    top_widget->setFixedHeight(60);
    QHBoxLayout* top_layout = new QHBoxLayout(top_widget);
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

    QPushButton* view_requests_button = new QPushButton("查看好友请求");
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
    contact_tree_widget_->setHeaderHidden(true);
    contact_tree_widget_->setStyleSheet(R"(
        QTreeWidget {
            border: none;
            background-color: white;
            font-size: 14px;
        }
        QTreeWidget::item {
            padding: 10px;
        }
        QTreeWidget::item:hover {
            background-color: #f5f5f5;
        }
    )");
    connect(contact_tree_widget_, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onContactItemDoubleClicked);

    layout->addWidget(contact_tree_widget_);
}

void MainWindow::onAddContactClicked() {
    AddContactDialog dialog(tcp_client_, this);
    dialog.exec();
}

void MainWindow::onContactItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    QString user_id = item->data(0, Qt::UserRole).toString();
    QString nickname = item->text(0);
    if (!user_id.isEmpty()) {
        // 联系人页不单独维护聊天状态，双击后复用消息页的会话切换逻辑。
        switchToChatWith(user_id, nickname);
        nav_list_->setCurrentRow(0); // 切换到消息视图
        content_stacked_->setCurrentWidget(message_view_);
    }
}

void MainWindow::onFriendListReceived(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return;

    QJsonArray friends = doc.array();

    // 好友列表同时驱动联系人树和消息页会话列表，避免两个页面各拉一份数据。
    for (const QJsonValue& value : friends) {
        QJsonObject friend_obj = value.toObject();
        QString friend_id = friend_obj["friend_id"].toString();
        QString nickname = friend_obj["nickname"].toString();
        QString remark = friend_obj["remark"].toString().trimmed();
        QString display_name = contact_remarks_.value(friend_id, remark.isEmpty() ? nickname : remark);
        QString last_message = friend_obj["last_msg_content"].toString();
        QString last_time = friend_obj["last_msg_time"].toString();
        qint64 last_timestamp = friend_obj["last_msg_timestamp"].toInteger();
        if (last_timestamp <= 0) {
            last_timestamp = timestampFromText(last_time);
        }

        ConversationState& conversation = conversations_[friend_id];
        conversation.title = display_name;
        if (last_timestamp > 0
            && (last_timestamp > conversation.last_timestamp || conversation.last_timestamp <= 0)) {
            conversation.last_message = last_message;
            conversation.last_timestamp = last_timestamp;
        }
    }
    refreshConversationList();

    if (!current_chat_target_.isEmpty() && conversations_.contains(current_chat_target_)) {
        chat_target_label_->setText(conversations_[current_chat_target_].title);
    }

    // 更新联系人树（不再调用 getFriendList，避免收到列表后再次请求形成循环）。
    contact_tree_widget_->clear();
    QTreeWidgetItem* contactGroup = new QTreeWidgetItem(contact_tree_widget_);
    contactGroup->setText(0, "联系人");
    contactGroup->setData(0, Qt::UserRole, QString());

    for (int i = 0; i < chat_list_widget_->count(); ++i) {
        QListWidgetItem* chatItem = chat_list_widget_->item(i);
        QString friend_id = chatItem->data(Qt::UserRole).toString();
        QString nickname = conversations_.contains(friend_id) && !conversations_[friend_id].title.isEmpty()
            ? conversations_[friend_id].title
            : friend_id;

        QTreeWidgetItem* friendItem = new QTreeWidgetItem(contactGroup);
        friendItem->setText(0, nickname);
        friendItem->setData(0, Qt::UserRole, friend_id);
        contactGroup->addChild(friendItem);
    }

    contact_tree_widget_->expandAll();
}

void MainWindow::onFriendRequestReceived(const QString& from_user_id,
                                         const QString& from_nickname,
                                         const QString& message) {
    QString info = QString("来自 %1 的好友请求: %2").arg(from_nickname, message);
    QMessageBox::information(this, "好友请求", info);
}

void MainWindow::onViewFriendRequestsClicked() {
    tcp_client_->getFriendRequests();
}

void MainWindow::onFriendRequestsReceived(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return;

    QJsonArray requests = doc.array();
    if (requests.isEmpty()) {
        QMessageBox::information(this, "好友请求", "没有待处理的好友请求");
        return;
    }

    // 好友请求是一次性处理弹窗；后续如果要做“新的朋友”页面，可从这里拆出独立组件。
    QDialog dialog(this);
    dialog.setWindowTitle("好友请求列表");
    dialog.setMinimumSize(400, 300);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QLabel* title = new QLabel(QString("收到 %1 个好友请求").arg(requests.size()));
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(title);

    QScrollArea* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    QWidget* container = new QWidget;
    QVBoxLayout* containerLayout = new QVBoxLayout(container);

    struct RequestItem {
        QString request_id;
        QString from_user_id;
        QString from_nickname;
        QString remark;
    };
    QList<RequestItem> requestItems;

    for (const QJsonValue& value : requests) {
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

        QFrame* frame = new QFrame;
        frame->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
        frame->setStyleSheet("QFrame { background-color: #f5f5f5; border-radius: 4px; padding: 10px; margin: 5px 0; }");

        QVBoxLayout* frameLayout = new QVBoxLayout(frame);

        QLabel* nameLabel = new QLabel(from_nickname);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");

        QLabel* remarkLabel = new QLabel(remark.isEmpty() ? "无备注" : remark);
        remarkLabel->setStyleSheet("color: #666; font-size: 12px;");

        QHBoxLayout* btnLayout = new QHBoxLayout;
        QPushButton* acceptBtn = new QPushButton("同意");
        acceptBtn->setStyleSheet(R"(
            QPushButton { background-color: #4CAF50; color: white; border: none; padding: 8px 16px; border-radius: 4px; }
            QPushButton:hover { background-color: #45a049; }
        )");
        QPushButton* rejectBtn = new QPushButton("拒绝");
        rejectBtn->setStyleSheet(R"(
            QPushButton { background-color: #f44336; color: white; border: none; padding: 8px 16px; border-radius: 4px; }
            QPushButton:hover { background-color: #e53935; }
        )");

        // 处理后禁用按钮，避免用户在服务端响应前重复提交同一个请求。
        QObject::connect(acceptBtn, &QPushButton::clicked, this, [this, request_id, acceptBtn, rejectBtn, &dialog]() {
            qDebug() << "同意按钮 clicked, request_id:" << request_id;
            tcp_client_->respondFriendRequest(request_id, true);
            acceptBtn->setEnabled(false);
            rejectBtn->setEnabled(false);
            acceptBtn->setText("已同意");
            // 刷新好友列表
            loadChatList();
            // 延迟关闭对话框，让用户看到结果
            QTimer::singleShot(500, &dialog, &QDialog::accept);
        });
        QObject::connect(rejectBtn, &QPushButton::clicked, this, [this, request_id, acceptBtn, rejectBtn, &dialog]() {
            tcp_client_->respondFriendRequest(request_id, false);
            acceptBtn->setEnabled(false);
            rejectBtn->setEnabled(false);
            rejectBtn->setText("已拒绝");
            QTimer::singleShot(500, &dialog, &QDialog::accept);
        });

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

    QPushButton* closeBtn = new QPushButton("关闭");
    closeBtn->setStyleSheet(R"(
        QPushButton { padding: 10px 20px; background-color: #9e9e9e; color: white; border: none; border-radius: 4px; }
        QPushButton:hover { background-color: #757575; }
    )");
    layout->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

void MainWindow::onFriendRemarkUpdateResult(int code, const QString& message,
                                            const QString& friend_id, const QString& remark) {
    if (code != 0) {
        QMessageBox::warning(this, "修改联系人备注", message.isEmpty() ? "修改备注失败" : message);
        return;
    }

    if (friend_id.isEmpty()) {
        return;
    }

    // 备注更新成功后立即同步到本地缓存、会话标题和联系人树。
    contact_remarks_[friend_id] = remark;
    conversations_[friend_id].title = remark;
    if (friend_id == current_chat_target_) {
        chat_target_label_->setText(remark);
    }
    updateConversationItem(friend_id);
    loadContacts();
}

void MainWindow::loadContacts() {
    // 如果聊天列表已有数据，直接从 conversations_ 构造联系人树，减少一次网络请求。
    if (chat_list_widget_->count() > 0) {
        contact_tree_widget_->clear();
        QTreeWidgetItem* contactGroup = new QTreeWidgetItem(contact_tree_widget_);
        contactGroup->setText(0, "联系人");
        contactGroup->setData(0, Qt::UserRole, QString());

        for (int i = 0; i < chat_list_widget_->count(); ++i) {
            QListWidgetItem* chatItem = chat_list_widget_->item(i);
            QString friend_id = chatItem->data(Qt::UserRole).toString();
            QString nickname = conversations_.contains(friend_id) && !conversations_[friend_id].title.isEmpty()
                ? conversations_[friend_id].title
                : friend_id;

            QTreeWidgetItem* friendItem = new QTreeWidgetItem(contactGroup);
            friendItem->setText(0, nickname);
            friendItem->setData(0, Qt::UserRole, friend_id);
            contactGroup->addChild(friendItem);
        }

        contact_tree_widget_->expandAll();
    } else {
        // 否则请求服务器获取好友列表
        tcp_client_->getFriendList();
    }
}
