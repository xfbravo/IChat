/**
 * @file mainwindow.cpp
 * @brief 主窗口实现
 */

#include "mainwindow.h"
#include "addcontactdialog.h"
#include <QStatusBar>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
#include <QSize>
#include <QStringList>
#include <QToolButton>
#include <QTimer>
#include <algorithm>

namespace {
qint64 timestampFromText(const QString& time_text) {
    if (time_text.isEmpty()) {
        return QDateTime::currentMSecsSinceEpoch();
    }

    const QStringList formats = {
        "yyyy-MM-dd HH:mm:ss",
        "yyyy-MM-dd hh:mm:ss",
        "yyyy-MM-ddTHH:mm:ss",
        "yyyy-MM-ddThh:mm:ss",
        "hh:mm:ss",
        "HH:mm:ss"
    };

    for (const QString& format : formats) {
        QDateTime dt = QDateTime::fromString(time_text, format);
        if (!dt.isValid()) continue;

        if (format == "hh:mm:ss" || format == "HH:mm:ss") {
            dt.setDate(QDate::currentDate());
        }
        return dt.toMSecsSinceEpoch();
    }

    QDateTime dt = QDateTime::fromString(time_text, Qt::ISODate);
    if (dt.isValid()) {
        return dt.toMSecsSinceEpoch();
    }

    return QDateTime::currentMSecsSinceEpoch();
}

class MessageBubble : public QWidget {
public:
    MessageBubble(const QString& text, bool is_mine, int max_text_width, QWidget* parent = nullptr)
        : QWidget(parent)
        , is_mine_(is_mine)
        , background_(is_mine ? QColor("#95ec69") : QColor("#eeeeee"))
        , border_(is_mine ? QColor("#95ec69") : QColor("#e2e2e2"))
    {
        setAttribute(Qt::WA_TranslucentBackground);

        QLabel* label = new QLabel(text, this);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setMaximumWidth(max_text_width);
        label->setStyleSheet(R"(
            QLabel {
                background: transparent;
                color: #111111;
                font-family: "Microsoft YaHei", sans-serif;
                font-size: 14px;
                line-height: 155%;
            }
        )");

        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setSpacing(0);
        layout->setContentsMargins(is_mine_ ? 12 : 20, 8, is_mine_ ? 20 : 12, 8);
        layout->addWidget(label);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);

        constexpr int tail_width = 8;
        constexpr int radius = 6;
        QRectF bubble_rect = rect().adjusted(
            is_mine_ ? 0.5 : tail_width + 0.5,
            0.5,
            is_mine_ ? -tail_width - 0.5 : -0.5,
            -0.5);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(border_, 1));
        painter.setBrush(background_);

        QPainterPath path;
        path.addRoundedRect(bubble_rect, radius, radius);

        const qreal tail_top = bubble_rect.top() + 13;
        const qreal tail_mid = bubble_rect.top() + 19;
        const qreal tail_bottom = bubble_rect.top() + 25;
        QPainterPath tail;
        if (is_mine_) {
            tail.moveTo(bubble_rect.right() - 1, tail_top);
            tail.lineTo(width() - 1, tail_mid);
            tail.lineTo(bubble_rect.right() - 1, tail_bottom);
        } else {
            tail.moveTo(bubble_rect.left() + 1, tail_top);
            tail.lineTo(1, tail_mid);
            tail.lineTo(bubble_rect.left() + 1, tail_bottom);
        }
        tail.closeSubpath();
        path = path.united(tail);

        painter.drawPath(path);
    }

private:
    bool is_mine_;
    QColor background_;
    QColor border_;
};

QIcon navIcon(const QString& type) {
    const QColor color("#ecf0f1");
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (type == "message") {
        painter.drawRoundedRect(QRectF(6, 7, 20, 15), 5, 5);
        QPainterPath tail;
        tail.moveTo(12, 22);
        tail.lineTo(9, 27);
        tail.lineTo(17, 22);
        painter.drawPath(tail);
    } else if (type == "contacts") {
        painter.drawEllipse(QRectF(11, 5, 10, 10));
        painter.drawArc(QRectF(7, 15, 18, 14), 20 * 16, 140 * 16);
        painter.drawEllipse(QRectF(4, 10, 7, 7));
        painter.drawEllipse(QRectF(21, 10, 7, 7));
    } else if (type == "moments") {
        painter.drawEllipse(QRectF(7, 7, 18, 18));
        painter.drawEllipse(QRectF(13, 3, 6, 6));
        painter.drawEllipse(QRectF(23, 16, 6, 6));
        painter.drawEllipse(QRectF(5, 21, 6, 6));
    } else if (type == "settings") {
        painter.drawEllipse(QRectF(11, 11, 10, 10));
        for (int i = 0; i < 8; ++i) {
            painter.save();
            painter.translate(16, 16);
            painter.rotate(i * 45);
            painter.drawLine(QPointF(0, -13), QPointF(0, -10));
            painter.restore();
        }
        painter.drawEllipse(QRectF(5, 5, 22, 22));
    }

    return QIcon(pixmap);
}
}

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

    // 加载聊天列表
    loadChatList();

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
    connect(tcp_client_, &TcpClient::chatHistoryReceived,
            this, &MainWindow::onChatHistoryReceived);
    connect(tcp_client_, &TcpClient::offlineMessageReceived,
            this, &MainWindow::onOfflineMessageReceived);
    connect(tcp_client_, &TcpClient::messageAckReceived,
            this, &MainWindow::onMessageAckReceived);
    connect(tcp_client_, &TcpClient::friendRemarkUpdateResult,
            this, &MainWindow::onFriendRemarkUpdateResult);
}

void MainWindow::createNavigationBar() {
    navigation_bar_ = new QWidget;
    navigation_bar_->setFixedWidth(72);
    navigation_bar_->setStyleSheet(R"(
        QWidget {
            background-color: #2c3e50;
        }
    )");

    QVBoxLayout* layout = new QVBoxLayout(navigation_bar_);
    layout->setContentsMargins(0, 10, 0, 0);
    layout->setSpacing(8);

    // 标题
    QLabel* title = new QLabel("I");
    title->setStyleSheet(R"(
        QLabel {
            color: white;
            font-size: 18px;
            font-weight: bold;
            padding: 8px 0;
        }
    )");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // 导航列表
    nav_list_ = new QListWidget;
    nav_list_->setViewMode(QListView::IconMode);
    nav_list_->setMovement(QListView::Static);
    nav_list_->setResizeMode(QListView::Adjust);
    nav_list_->setFlow(QListView::TopToBottom);
    nav_list_->setIconSize(QSize(28, 28));
    nav_list_->setGridSize(QSize(72, 64));
    nav_list_->setSpacing(2);
    nav_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    nav_list_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    nav_list_->setTextElideMode(Qt::ElideNone);
    nav_list_->setStyleSheet(R"(
        QListWidget {
            background-color: #2c3e50;
            border: none;
            color: white;
            outline: none;
        }
        QListWidget::item {
            padding: 6px 0;
            font-size: 11px;
            color: #dce6ec;
        }
        QListWidget::item:selected {
            background-color: #34495e;
            color: white;
        }
        QListWidget::item:hover {
            background-color: #34495e;
        }
    )");
    const QList<QPair<QString, QString>> nav_items = {
        {"message", "消息"},
        {"contacts", "联系人"},
        {"moments", "朋友圈"},
        {"settings", "设置"}
    };
    for (const auto& item : nav_items) {
        QListWidgetItem* nav_item = new QListWidgetItem(navIcon(item.first), item.second);
        nav_item->setTextAlignment(Qt::AlignCenter);
        nav_item->setSizeHint(QSize(72, 64));
        nav_list_->addItem(nav_item);
    }
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
    chat_list_widget_->setFocusPolicy(Qt::NoFocus);
    chat_list_widget_->setStyleSheet(R"(
        QListWidget {
            border: none;
            background-color: #f5f5f5;
            color: #111111;
            outline: none;
        }
        QListWidget::item {
            padding: 12px 15px;
            border-bottom: 1px solid #e0e0e0;
            color: #111111;
        }
        QListWidget::item:selected {
            background-color: #4CAF50;
            color: #ffffff;
            border: none;
        }
        QListWidget::item:selected:active,
        QListWidget::item:selected:!active {
            background-color: #4CAF50;
            color: #ffffff;
            border: none;
        }
    )");
    connect(chat_list_widget_, &QListWidget::itemClicked,
            this, &MainWindow::onChatItemClicked);

    // 聊天界面面板 (右侧)
    chat_interface_panel_ = new QWidget;
    QVBoxLayout* chat_layout = new QVBoxLayout(chat_interface_panel_);
    chat_layout->setContentsMargins(0, 0, 0, 0);

    // 聊天标题栏
    QWidget* chat_header = new QWidget;
    chat_header->setFixedHeight(54);
    chat_header->setStyleSheet(R"(
        QWidget {
            background-color: #fafafa;
            border-bottom: 1px solid #e0e0e0;
        }
    )");
    QHBoxLayout* chat_header_layout = new QHBoxLayout(chat_header);
    chat_header_layout->setContentsMargins(15, 0, 12, 0);
    chat_header_layout->setSpacing(8);

    chat_target_label_ = new QLabel("选择联系人开始聊天");
    chat_target_label_->setStyleSheet(R"(
        QLabel {
            background-color: transparent;
            border: none;
            font-size: 16px;
            font-weight: bold;
            color: #111111;
        }
    )");
    chat_header_layout->addWidget(chat_target_label_, 1);

    chat_more_button_ = new QToolButton;
    chat_more_button_->setText("⋯");
    chat_more_button_->setToolTip("更多");
    chat_more_button_->setPopupMode(QToolButton::InstantPopup);
    chat_more_button_->setEnabled(false);
    chat_more_button_->setStyleSheet(R"(
        QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 4px;
            color: #333333;
            font-size: 22px;
            font-weight: bold;
            min-width: 32px;
            min-height: 32px;
            padding-bottom: 4px;
        }
        QToolButton:hover {
            background-color: #eeeeee;
        }
        QToolButton:disabled {
            color: #bbbbbb;
        }
        QToolButton::menu-indicator {
            image: none;
            width: 0;
        }
    )");
    QMenu* chat_menu = new QMenu(chat_more_button_);
    chat_menu->setStyleSheet(R"(
        QMenu {
            background-color: #ffffff;
            border: 1px solid #dddddd;
            padding: 6px 0;
        }
        QMenu::item {
            min-height: 32px;
            padding: 8px 28px 8px 18px;
            color: #111111;
            font-size: 14px;
        }
        QMenu::item:selected {
            background-color: #f0f0f0;
        }
    )");
    QAction* edit_remark_action = chat_menu->addAction("修改联系人备注");
    connect(edit_remark_action, &QAction::triggered, this, &MainWindow::onEditContactRemark);
    chat_more_button_->setMenu(chat_menu);
    chat_header_layout->addWidget(chat_more_button_);

    chat_layout->addWidget(chat_header);

    // 聊天显示区
    chat_scroll_area_ = new QScrollArea;
    chat_scroll_area_->setWidgetResizable(true);
    chat_scroll_area_->setFrameShape(QFrame::NoFrame);
    chat_scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chat_scroll_area_->setStyleSheet(R"(
        QScrollArea {
            background-color: #fafafa;
            border: none;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 4px 0;
        }
        QScrollBar::handle:vertical {
            background: #d6d6d6;
            border-radius: 4px;
            min-height: 32px;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }
    )");

    chat_messages_widget_ = new QWidget;
    chat_messages_widget_->setStyleSheet("background-color: #fafafa;");
    chat_messages_layout_ = new QVBoxLayout(chat_messages_widget_);
    chat_messages_layout_->setContentsMargins(16, 12, 16, 12);
    chat_messages_layout_->setSpacing(2);
    chat_messages_layout_->addStretch();
    chat_scroll_area_->setWidget(chat_messages_widget_);
    chat_layout->addWidget(chat_scroll_area_);

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

    QString msg_id = tcp_client_->sendChatMessage(current_chat_target_, "text", message);
    if (msg_id.isEmpty()) {
        appendMessage(user_nickname_, message, true, QString(), "failed");
    } else {
        appendMessage(user_nickname_, message, true, msg_id, "sending");
    }
    message_input_->clear();
}

void MainWindow::onChatMessageReceived(const QString& from_user_id, const QString& content, const QString& msg_id) {
    ChatViewMessage message;
    message.msg_id = msg_id;
    message.from = from_user_id;
    message.content = content;
    message.time = QDateTime::currentDateTime().toString("hh:mm:ss");
    message.timestamp = QDateTime::currentMSecsSinceEpoch();
    message.is_mine = false;
    addMessageToConversation(from_user_id, message, from_user_id != current_chat_target_);
}

void MainWindow::onChatHistoryReceived(const QString& friend_id, const QString& history_json) {
    QJsonDocument doc = QJsonDocument::fromJson(history_json.toUtf8());
    if (!doc.isArray()) return;

    QJsonArray messages = doc.array();
    if (messages.isEmpty()) return;

    QString target_id = friend_id.isEmpty() ? current_chat_target_ : friend_id;

    for (int i = 0; i < messages.size(); ++i) {
        QJsonObject msg = messages[i].toObject();
        QString from_user_id = msg["from_user_id"].toString();
        QString content = msg["content"].toString();
        bool is_mine = (from_user_id == user_id_);

        QString peer_id = is_mine ? target_id : from_user_id;
        ChatViewMessage view_message;
        view_message.msg_id = msg["msg_id"].toString();
        view_message.from = from_user_id;
        view_message.content = content;
        view_message.time = msg["server_time"].toString();
        view_message.timestamp = timestampFromText(view_message.time);
        view_message.is_mine = is_mine;
        addMessageToConversation(peer_id, view_message, false);
    }
}

void MainWindow::onOfflineMessageReceived(const QString& from_user_id, const QString& content, const QString& msg_id) {
    ChatViewMessage message;
    message.msg_id = msg_id;
    message.from = from_user_id;
    message.content = content;
    message.time = QDateTime::currentDateTime().toString("hh:mm:ss");
    message.timestamp = QDateTime::currentMSecsSinceEpoch();
    message.is_mine = false;
    addMessageToConversation(from_user_id, message, from_user_id != current_chat_target_);
}

void MainWindow::onMessageAckReceived(const QString& msg_id, const QString& status, int code, const QString& message) {
    Q_UNUSED(code);
    if (msg_id.isEmpty()) {
        return;
    }

    for (auto it = conversations_.begin(); it != conversations_.end(); ++it) {
        for (ChatViewMessage& view_message : it->messages) {
            if (view_message.msg_id != msg_id) continue;

            view_message.status = status.isEmpty() ? "failed" : status;
            if (view_message.status == "failed" && !message.isEmpty()) {
                view_message.status = QString("failed:%1").arg(message);
            }
            if (it.key() == current_chat_target_) {
                current_messages_ = it->messages;
                renderChatMessages();
            }
            return;
        }
    }
}

void MainWindow::onLoadMoreMessages() {
    if (current_chat_target_.isEmpty()) return;

    // 获取当前显示的最早消息的时间戳
    // 简化处理：直接请求更多消息
    tcp_client_->getChatHistory(current_chat_target_, 20, QDateTime::currentSecsSinceEpoch());
}

void MainWindow::onChatItemClicked(QListWidgetItem* item) {
    QString user_id = item->data(Qt::UserRole).toString();
    QString nickname = conversations_.contains(user_id) && !conversations_[user_id].title.isEmpty()
        ? conversations_[user_id].title
        : item->text();
    switchToChatWith(user_id, nickname);
}

void MainWindow::switchToChatWith(const QString& user_id, const QString& nickname) {
    current_chat_target_ = user_id;
    QString display_name = contact_remarks_.value(user_id, nickname);
    chat_target_label_->setText(display_name);
    chat_more_button_->setEnabled(true);
    conversations_[user_id].title = display_name;
    conversations_[user_id].unread = 0;
    current_messages_ = conversations_[user_id].messages;
    message_index_by_id_.clear();
    renderChatMessages();
    updateConversationItem(user_id);
    // 加载聊天记录
    tcp_client_->getChatHistory(user_id, 20, 0);
}

void MainWindow::onDisconnected() {
    status_label_->setText("离线");
    message_input_->setEnabled(false);
    send_button_->setEnabled(false);
    markSendingMessagesFailed("连接已断开");
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
        QString remark = friend_obj["remark"].toString().trimmed();
        QString display_name = contact_remarks_.value(friend_id, remark.isEmpty() ? nickname : remark);

        // 添加到聊天列表
        conversations_[friend_id].title = display_name;
        QListWidgetItem* item = new QListWidgetItem(conversationTitle(friend_id));
        item->setData(Qt::UserRole, friend_id);
        chat_list_widget_->addItem(item);
    }

    if (!current_chat_target_.isEmpty() && conversations_.contains(current_chat_target_)) {
        chat_target_label_->setText(conversations_[current_chat_target_].title);
    }

    // 更新联系人树（不再调用getFriendList，避免循环）
    contact_tree_widget_->clear();
    QTreeWidgetItem* contactGroup = new QTreeWidgetItem(contact_tree_widget_);
    contactGroup->setText(0, "联系人");
    contactGroup->setData(0, Qt::UserRole, QString());

    for (int i = 0; i < chat_list_widget_->count(); ++i) {
        QListWidgetItem* chatItem = chat_list_widget_->item(i);
        QString friend_id = chatItem->data(Qt::UserRole).toString();
        QString nickname = conversations_.contains(friend_id) && !conversations_[friend_id].title.isEmpty()
            ? conversations_[friend_id].title
            : chatItem->text();

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

    tcp_client_->updateFriendRemark(current_chat_target_, new_remark);
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

    contact_remarks_[friend_id] = remark;
    conversations_[friend_id].title = remark;
    if (friend_id == current_chat_target_) {
        chat_target_label_->setText(remark);
    }
    updateConversationItem(friend_id);
    loadContacts();
}

void MainWindow::appendMessage(const QString& from, const QString& content, bool is_mine,
                               const QString& msg_id, const QString& status) {
    ChatViewMessage message;
    message.msg_id = msg_id;
    message.from = from;
    message.content = content;
    message.time = QDateTime::currentDateTime().toString("hh:mm:ss");
    message.timestamp = QDateTime::currentMSecsSinceEpoch();
    message.status = status;
    message.is_mine = is_mine;

    QString peer_id = is_mine ? current_chat_target_ : from;
    addMessageToConversation(peer_id, message, false);
}

void MainWindow::addMessageToConversation(const QString& peer_id, const ChatViewMessage& message, bool count_unread) {
    if (peer_id.isEmpty()) return;

    ConversationState& conversation = conversations_[peer_id];
    if (conversation.title.isEmpty()) {
        conversation.title = peer_id;
    }

    if (!message.msg_id.isEmpty()) {
        for (const ChatViewMessage& existing : conversation.messages) {
            if (existing.msg_id == message.msg_id) {
                updateConversationItem(peer_id);
                return;
            }
        }
    }

    ChatViewMessage sorted_message = message;
    if (sorted_message.timestamp <= 0) {
        sorted_message.timestamp = timestampFromText(sorted_message.time);
    }

    conversation.messages.append(sorted_message);
    std::stable_sort(conversation.messages.begin(), conversation.messages.end(),
                     [](const ChatViewMessage& left, const ChatViewMessage& right) {
                         return left.timestamp < right.timestamp;
                     });

    conversation.last_message = conversation.messages.isEmpty()
        ? QString()
        : conversation.messages.last().content;
    if (count_unread) {
        ++conversation.unread;
    }

    if (peer_id == current_chat_target_) {
        conversation.unread = 0;
        current_messages_ = conversation.messages;
        message_index_by_id_.clear();
        for (int i = 0; i < current_messages_.size(); ++i) {
            if (!current_messages_[i].msg_id.isEmpty()) {
                message_index_by_id_[current_messages_[i].msg_id] = i;
            }
        }
        renderChatMessages();
    }

    updateConversationItem(peer_id);
}

QString MainWindow::conversationTitle(const QString& peer_id) const {
    auto it = conversations_.constFind(peer_id);
    if (it == conversations_.constEnd()) return peer_id;

    const ConversationState& conversation = it.value();
    QString title = conversation.title.isEmpty() ? peer_id : conversation.title;
    if (conversation.unread > 0) {
        title = QString("%1 (%2)").arg(title).arg(conversation.unread);
    }
    if (!conversation.last_message.isEmpty()) {
        title = QString("%1\n%2").arg(title, conversation.last_message);
    }
    return title;
}

void MainWindow::updateConversationItem(const QString& peer_id) {
    if (peer_id.isEmpty()) return;

    for (int i = 0; i < chat_list_widget_->count(); ++i) {
        QListWidgetItem* item = chat_list_widget_->item(i);
        if (item->data(Qt::UserRole).toString() == peer_id) {
            item->setText(conversationTitle(peer_id));
            return;
        }
    }

    QListWidgetItem* item = new QListWidgetItem(conversationTitle(peer_id));
    item->setData(Qt::UserRole, peer_id);
    chat_list_widget_->addItem(item);
}

void MainWindow::renderChatMessages() {
    while (QLayoutItem* item = chat_messages_layout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const int viewport_width = chat_scroll_area_->viewport()
        ? chat_scroll_area_->viewport()->width()
        : chat_scroll_area_->width();
    const int max_text_width = qMax(180, static_cast<int>(viewport_width * 0.55));

    for (const ChatViewMessage& message : current_messages_) {
        QWidget* row = new QWidget(chat_messages_widget_);
        QHBoxLayout* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 4, 0, 4);
        row_layout->setSpacing(8);

        QWidget* message_column = new QWidget(row);
        QVBoxLayout* column_layout = new QVBoxLayout(message_column);
        column_layout->setContentsMargins(0, 0, 0, 0);
        column_layout->setSpacing(4);

        QLabel* meta_label = new QLabel(message.time, message_column);
        meta_label->setStyleSheet("QLabel { color: #888888; font-size: 12px; }");
        meta_label->setAlignment(message.is_mine ? Qt::AlignRight : Qt::AlignLeft);
        column_layout->addWidget(meta_label);

        MessageBubble* bubble = new MessageBubble(message.content, message.is_mine, max_text_width, message_column);
        column_layout->addWidget(bubble, 0, message.is_mine ? Qt::AlignRight : Qt::AlignLeft);

        QString status = statusText(message.status);
        if (!status.isEmpty()) {
            QLabel* status_label = new QLabel(status, message_column);
            status_label->setStyleSheet("QLabel { color: #888888; font-size: 12px; }");
            status_label->setAlignment(Qt::AlignRight);
            column_layout->addWidget(status_label);
        }

        if (message.is_mine) {
            row_layout->addStretch();
            row_layout->addWidget(message_column, 0, Qt::AlignRight);
        } else {
            row_layout->addWidget(message_column, 0, Qt::AlignLeft);
            row_layout->addStretch();
        }

        chat_messages_layout_->addWidget(row);
    }

    chat_messages_layout_->addStretch();
    QTimer::singleShot(0, this, [this]() {
        chat_scroll_area_->verticalScrollBar()->setValue(chat_scroll_area_->verticalScrollBar()->maximum());
    });
}

QString MainWindow::statusText(const QString& status) const {
    if (status == "sending") return "发送中";
    if (status == "sent") return "已发送";
    if (status == "delivered") return "已送达";
    if (status == "read") return "已读";
    if (status == "failed") return "发送失败";
    if (status.startsWith("failed:")) return QString("发送失败：%1").arg(status.mid(7));
    return QString();
}

void MainWindow::markSendingMessagesFailed(const QString& reason) {
    bool changed = false;
    for (auto it = conversations_.begin(); it != conversations_.end(); ++it) {
        for (ChatViewMessage& message : it->messages) {
            if (message.is_mine && message.status == "sending") {
                message.status = reason.isEmpty() ? "failed" : QString("failed:%1").arg(reason);
                changed = true;
            }
        }
    }
    if (changed) {
        if (!current_chat_target_.isEmpty()) {
            current_messages_ = conversations_[current_chat_target_].messages;
        }
        renderChatMessages();
    }
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
            QString nickname = conversations_.contains(friend_id) && !conversations_[friend_id].title.isEmpty()
                ? conversations_[friend_id].title
                : chatItem->text();

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
