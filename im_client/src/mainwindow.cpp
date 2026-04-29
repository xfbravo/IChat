/**
 * @file mainwindow.cpp
 * @brief 主窗口实现
 */

#include "mainwindow.h"
#include "addcontactdialog.h"
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QDateTime>
#include <QStyle>
#include <QTreeWidgetItem>
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QScrollArea>
#include <QTimer>

MainWindow::MainWindow(TcpClient* tcp_client,
                      const QString& user_id,
                      const QString& nickname,
                      QWidget* parent)
    : QMainWindow(parent)
    , tcp_client_(tcp_client)
    , user_id_(user_id)
    , user_nickname_(nickname)
{
    setWindowTitle(QString("IChat - %1").arg(nickname));
    setMinimumSize(1000, 700);

    createNavigationBar();
    createMessageView();
    createContactView();
    createPlaceholderView(moments_view_, "朋友圈功能开发中...");
    createPlaceholderView(settings_view_, "设置功能开发中...");

    // 创建内容切换页面
    content_stacked_ = new QStackedWidget;
    content_stacked_->addWidget(message_view_);      // 0 - 消息
    content_stacked_->addWidget(contact_view_);     // 1 - 联系人
    content_stacked_->addWidget(moments_view_);     // 2 - 朋友圈
    content_stacked_->addWidget(settings_view_);    // 3 - 设置
    content_stacked_->setCurrentIndex(0);

    // 主布局：导航栏 + 内容区
    QHBoxLayout* main_layout = new QHBoxLayout;
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);
    main_layout->addWidget(navigation_bar_);
    main_layout->addWidget(content_stacked_, 1);

    QWidget* central = new QWidget;
    central->setLayout(main_layout);
    setCentralWidget(central);

    // 状态栏
    status_label_ = new QLabel("在线");
    statusBar()->addWidget(status_label_);

    // 连接信号槽
    connect(tcp_client_, &TcpClient::chatMessageReceived,
            this, &MainWindow::onChatMessageReceived);
    connect(tcp_client_, &TcpClient::disconnected,
            this, &MainWindow::onDisconnected);
    connect(tcp_client_, &TcpClient::friendListReceived,
            this, &MainWindow::onFriendListReceived);
    connect(tcp_client_, &TcpClient::friendRequestReceived,
            this, &MainWindow::onFriendRequestReceived);
    connect(tcp_client_, &TcpClient::friendRequestsReceived,
            this, &MainWindow::onFriendRequestsReceived);
}

void MainWindow::createNavigationBar() {
    navigation_bar_ = new QWidget;
    navigation_bar_->setFixedWidth(200);
    navigation_bar_->setStyleSheet(R"(
        QWidget {
            background-color: #2c3e50;
        }
    )");

    QVBoxLayout* layout = new QVBoxLayout(navigation_bar_);
    layout->setContentsMargins(0, 0, 0, 0);

    // 标题
    QLabel* title = new QLabel("IChat");
    title->setStyleSheet(R"(
        QLabel {
            color: white;
            font-size: 24px;
            font-weight: bold;
            padding: 20px 0;
        }
    )");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // 导航列表
    nav_list_ = new QListWidget;
    nav_list_->setStyleSheet(R"(
        QListWidget {
            background-color: #2c3e50;
            border: none;
            color: white;
        }
        QListWidget::item {
            padding: 15px 20px;
            font-size: 14px;
        }
        QListWidget::item:selected {
            background-color: #34495e;
        }
        QListWidget::item:hover {
            background-color: #34495e;
        }
    )");
    nav_list_->addItem("消息");
    nav_list_->addItem("联系人");
    nav_list_->addItem("朋友圈");
    nav_list_->addItem("设置");
    nav_list_->setCurrentRow(0);
    connect(nav_list_, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavigationItemClicked);

    layout->addWidget(nav_list_);
    layout->addStretch();
}

void MainWindow::createMessageView() {
    message_view_ = new QWidget;

    QHBoxLayout* main_layout = new QHBoxLayout(message_view_);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // 聊天列表 (左侧)
    chat_list_widget_ = new QListWidget;
    chat_list_widget_->setStyleSheet(R"(
        QListWidget {
            border: none;
            background-color: #f5f5f5;
        }
        QListWidget::item {
            padding: 12px 15px;
            border-bottom: 1px solid #e0e0e0;
        }
        QListWidget::item:selected {
            background-color: #e8f5e9;
        }
    )");
    connect(chat_list_widget_, &QListWidget::itemClicked,
            this, &MainWindow::onChatItemClicked);

    // 聊天界面面板 (右侧)
    chat_interface_panel_ = new QWidget;
    QVBoxLayout* chat_layout = new QVBoxLayout(chat_interface_panel_);
    chat_layout->setContentsMargins(0, 0, 0, 0);

    // 聊天目标标签
    chat_target_label_ = new QLabel("选择联系人开始聊天");
    chat_target_label_->setStyleSheet(R"(
        QLabel {
            background-color: #fafafa;
            padding: 15px;
            font-size: 16px;
            font-weight: bold;
            border-bottom: 1px solid #e0e0e0;
        }
    )");
    chat_layout->addWidget(chat_target_label_);

    // 聊天显示区
    chat_display_ = new QTextEdit;
    chat_display_->setReadOnly(true);
    chat_display_->setStyleSheet(R"(
        QTextEdit {
            background-color: #fafafa;
            border: none;
            padding: 10px;
            font-family: "Microsoft YaHei", sans-serif;
            font-size: 14px;
        }
    )");
    chat_layout->addWidget(chat_display_);

    // 输入区
    QWidget* input_widget = new QWidget;
    QHBoxLayout* input_layout = new QHBoxLayout(input_widget);
    input_layout->setContentsMargins(10, 10, 10, 10);
    input_layout->setSpacing(10);

    message_input_ = new QLineEdit;
    message_input_->setPlaceholderText("输入消息...");
    message_input_->setStyleSheet(R"(
        QLineEdit {
            padding: 10px 15px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 14px;
        }
        QLineEdit:focus {
            border: 1px solid #4CAF50;
        }
    )");
    connect(message_input_, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);

    send_button_ = new QPushButton("发送");
    send_button_->setStyleSheet(R"(
        QPushButton {
            padding: 10px 25px;
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
        QPushButton:disabled {
            background-color: #cccccc;
        }
    )");
    connect(send_button_, &QPushButton::clicked, this, &MainWindow::onSendClicked);

    input_layout->addWidget(message_input_);
    input_layout->addWidget(send_button_);

    chat_layout->addWidget(input_widget);

    // 使用分割器
    message_splitter_ = new QSplitter(Qt::Horizontal);
    message_splitter_->addWidget(chat_list_widget_);
    message_splitter_->addWidget(chat_interface_panel_);
    message_splitter_->setStretchFactor(0, 1);
    message_splitter_->setStretchFactor(1, 3);
    message_splitter_->setHandleWidth(1);

    main_layout->addWidget(message_splitter_);
}

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

void MainWindow::createPlaceholderView(QWidget*& widget, const QString& text) {
    widget = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(widget);
    QLabel* label = new QLabel(text);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(R"(
        QLabel {
            color: #888888;
            font-size: 16px;
        }
    )");
    layout->addWidget(label);
}

void MainWindow::onNavigationItemClicked(int index) {
    if (!content_stacked_) return;

    switch (index) {
        case 0: // 消息
            content_stacked_->setCurrentWidget(message_view_);
            loadChatList();
            break;
        case 1: // 联系人
            content_stacked_->setCurrentWidget(contact_view_);
            loadContacts();
            break;
        case 2: // 朋友圈
            content_stacked_->setCurrentWidget(moments_view_);
            break;
        case 3: // 设置
            content_stacked_->setCurrentWidget(settings_view_);
            break;
    }
}

void MainWindow::onSendClicked() {
    QString message = message_input_->text().trimmed();
    if (message.isEmpty() || current_chat_target_.isEmpty()) {
        return;
    }

    tcp_client_->sendChatMessage(current_chat_target_, "text", message);
    appendMessage(user_nickname_, message, true);
    message_input_->clear();
}

void MainWindow::onChatMessageReceived(const QString& from_user_id, const QString& content) {
    // 更新聊天列表
    bool found = false;
    for (int i = 0; i < chat_list_widget_->count(); ++i) {
        QListWidgetItem* item = chat_list_widget_->item(i);
        QString item_id = item->data(Qt::UserRole).toString();
        if (item_id == from_user_id) {
            found = true;
            break;
        }
    }

    // 如果当前正在和该用户聊天，显示消息
    if (from_user_id == current_chat_target_) {
        appendMessage(from_user_id, content, false);
    }
}

void MainWindow::onChatItemClicked(QListWidgetItem* item) {
    QString user_id = item->data(Qt::UserRole).toString();
    QString nickname = item->text();
    switchToChatWith(user_id, nickname);
}

void MainWindow::switchToChatWith(const QString& user_id, const QString& nickname) {
    current_chat_target_ = user_id;
    chat_target_label_->setText(nickname);
    chat_display_->clear();
}

void MainWindow::onDisconnected() {
    status_label_->setText("离线");
    message_input_->setEnabled(false);
    send_button_->setEnabled(false);
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
        switchToChatWith(user_id, nickname);
        nav_list_->setCurrentRow(0); // 切换到消息视图
        content_stacked_->setCurrentWidget(message_view_);
    }
}

void MainWindow::onFriendListReceived(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return;

    QJsonArray friends = doc.array();

    // 更新聊天列表
    chat_list_widget_->clear();

    for (const QJsonValue& value : friends) {
        QJsonObject friend_obj = value.toObject();
        QString friend_id = friend_obj["friend_id"].toString();
        QString nickname = friend_obj["nickname"].toString();

        // 添加到聊天列表
        QListWidgetItem* item = new QListWidgetItem(nickname);
        item->setData(Qt::UserRole, friend_id);
        chat_list_widget_->addItem(item);
    }

    // 更新联系人树（不再调用getFriendList，避免循环）
    contact_tree_widget_->clear();
    QTreeWidgetItem* contactGroup = new QTreeWidgetItem(contact_tree_widget_);
    contactGroup->setText(0, "联系人");
    contactGroup->setData(0, Qt::UserRole, QString());

    for (int i = 0; i < chat_list_widget_->count(); ++i) {
        QListWidgetItem* chatItem = chat_list_widget_->item(i);
        QString friend_id = chatItem->data(Qt::UserRole).toString();
        QString nickname = chatItem->text();

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

    // 显示好友请求列表对话框
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

        // 连接按钮信号
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

void MainWindow::onLogoutClicked() {
    tcp_client_->disconnectFromServer();
    emit logout();
}

void MainWindow::appendMessage(const QString& from, const QString& content, bool is_mine) {
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString color = is_mine ? "#4CAF50" : "#2196F3";
    QString align = is_mine ? "right" : "left";

    QString html = QString(
        "<div style='text-align: %1; margin: 5px 0;'>"
        "<span style='font-size: 12px; color: #888;'>%2 %3</span>"
        "<br/>"
        "<span style='display: inline-block; padding: 8px 12px; "
        "background-color: %4; color: white; border-radius: 8px; "
        "max-width: 70%%; word-wrap: break-word;'>%5</span>"
        "</div>"
    ).arg(align, is_mine ? "我" : from, time, color, content.toHtmlEscaped());

    chat_display_->append(html);
}

void MainWindow::loadChatList() {
    tcp_client_->getFriendList();
}

void MainWindow::loadContacts() {
    // 如果聊天列表已有数据，直接从它加载
    if (chat_list_widget_->count() > 0) {
        contact_tree_widget_->clear();
        QTreeWidgetItem* contactGroup = new QTreeWidgetItem(contact_tree_widget_);
        contactGroup->setText(0, "联系人");
        contactGroup->setData(0, Qt::UserRole, QString());

        for (int i = 0; i < chat_list_widget_->count(); ++i) {
            QListWidgetItem* chatItem = chat_list_widget_->item(i);
            QString friend_id = chatItem->data(Qt::UserRole).toString();
            QString nickname = chatItem->text();

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
