/**
 * @file mainwindow_messages.cpp
 * @brief MainWindow 消息列表、聊天面板和会话渲染
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
            padding: 0;
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
    connect(chat_list_widget_, &QListWidget::itemSelectionChanged,
            this, &MainWindow::refreshConversationSelectionStyles);

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

void MainWindow::onSendClicked() {
    QString message = message_input_->text().trimmed();
    if (message.isEmpty() || current_chat_target_.isEmpty()) {
        return;
    }

    // 目前仅发送文本；后续图片/视频/语音可以复用 sendChatMessage 的 content_type 参数。
    QString msg_id = tcp_client_->sendChatMessage(current_chat_target_, "text", message);
    if (msg_id.isEmpty()) {
        appendMessage(user_nickname_, message, true, QString(), "failed");
    } else {
        appendMessage(user_nickname_, message, true, msg_id, "sending");
    }
    message_input_->clear();
}

void MainWindow::onChatMessageReceived(const QString& from_user_id, const QString& content,
                                       const QString& msg_id, qint64 server_timestamp,
                                       const QString& server_time) {
    // 服务端推送进入后先转换为 UI 消息模型，再统一交给会话缓存处理。
    ChatViewMessage message;
    message.msg_id = msg_id;
    message.from = from_user_id;
    message.content = content;
    message.time = server_time.isEmpty()
        ? QDateTime::currentDateTime().toString("hh:mm:ss")
        : server_time;
    message.timestamp = server_timestamp > 0
        ? server_timestamp
        : timestampFromText(message.time);
    if (message.timestamp <= 0) {
        message.timestamp = QDateTime::currentMSecsSinceEpoch();
    }
    message.is_mine = false;
    addMessageToConversation(from_user_id, message, from_user_id != current_chat_target_);
}

void MainWindow::onChatHistoryReceived(const QString& friend_id, const QString& history_json) {
    QJsonDocument doc = QJsonDocument::fromJson(history_json.toUtf8());
    if (!doc.isArray()) return;

    QJsonArray messages = doc.array();
    if (messages.isEmpty()) return;

    // TcpClient 会把本次历史记录所属好友带回来；为空时兼容旧逻辑使用当前会话。
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
        view_message.timestamp = msg["server_timestamp"].toInteger();
        if (view_message.timestamp <= 0) {
            view_message.timestamp = timestampFromText(view_message.time);
        }
        view_message.is_mine = is_mine;
        addMessageToConversation(peer_id, view_message, false);
    }
}

void MainWindow::onOfflineMessageReceived(const QString& from_user_id, const QString& content,
                                          const QString& msg_id, qint64 server_timestamp,
                                          const QString& server_time) {
    ChatViewMessage message;
    message.msg_id = msg_id;
    message.from = from_user_id;
    message.content = content;
    message.time = server_time.isEmpty()
        ? QDateTime::currentDateTime().toString("hh:mm:ss")
        : server_time;
    message.timestamp = server_timestamp > 0
        ? server_timestamp
        : timestampFromText(message.time);
    if (message.timestamp <= 0) {
        message.timestamp = QDateTime::currentMSecsSinceEpoch();
    }
    message.is_mine = false;
    addMessageToConversation(from_user_id, message, from_user_id != current_chat_target_);
}

void MainWindow::onMessageAckReceived(const QString& msg_id, const QString& status, int code, const QString& message) {
    Q_UNUSED(code);
    if (msg_id.isEmpty()) {
        return;
    }

    // ACK 只带 msg_id，因此需要在所有本地会话中查找对应消息并更新发送状态。
    for (auto it = conversations_.begin(); it != conversations_.end(); ++it) {
        for (ChatViewMessage& view_message : it->messages) {
            if (view_message.msg_id != msg_id) continue;

            view_message.status = status.isEmpty() ? "failed" : status;
            if (view_message.status == "failed" && !message.isEmpty()) {
                view_message.status = QString("failed:%1").arg(message);
            }
            if (it.key() == current_chat_target_) {
                current_messages_ = it->messages;
                rebuildMessageIndex();
                if (!updateRenderedMessageStatus(msg_id, view_message.status)) {
                    renderChatMessages(false);
                }
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
        : user_id;
    switchToChatWith(user_id, nickname);
}

void MainWindow::switchToChatWith(const QString& user_id, const QString& nickname) {
    // 切换会话时先显示本地缓存，再向服务端拉取历史记录补齐。
    current_chat_target_ = user_id;
    QString display_name = contact_remarks_.value(user_id, nickname);
    chat_target_label_->setText(display_name);
    chat_more_button_->setEnabled(true);
    conversations_[user_id].title = display_name;
    conversations_[user_id].unread = 0;
    current_messages_ = conversations_[user_id].messages;
    message_index_by_id_.clear();
    renderChatMessages(true);
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

    // 历史记录、离线消息和实时推送可能重复到达，msg_id 用于本地去重。
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

    // 按服务端时间排序，保证历史消息和新消息混合到达时聊天窗口仍然有序。
    conversation.messages.append(sorted_message);
    std::stable_sort(conversation.messages.begin(), conversation.messages.end(),
                     [](const ChatViewMessage& left, const ChatViewMessage& right) {
                         return left.timestamp < right.timestamp;
                     });

    conversation.last_message = conversation.messages.isEmpty()
        ? QString()
        : conversation.messages.last().content;
    conversation.last_timestamp = conversation.messages.isEmpty()
        ? 0
        : conversation.messages.last().timestamp;
    if (count_unread) {
        ++conversation.unread;
    }

    if (peer_id == current_chat_target_) {
        conversation.unread = 0;
        // 如果新消息正好追加在末尾，只插入一行，避免整屏重绘造成闪动。
        const bool can_append_to_current_view =
            conversation.messages.size() == current_messages_.size() + 1 &&
            !conversation.messages.isEmpty() &&
            conversation.messages.last().msg_id == sorted_message.msg_id;

        current_messages_ = conversation.messages;
        rebuildMessageIndex();

        if (can_append_to_current_view) {
            appendMessageRow(current_messages_.last());
        } else {
            renderChatMessages(true);
        }
    }

    updateConversationItem(peer_id);
}

QString MainWindow::conversationTitle(const QString& peer_id) const {
    auto it = conversations_.constFind(peer_id);
    if (it == conversations_.constEnd()) return peer_id;

    const ConversationState& conversation = it.value();
    return conversation.title.isEmpty() ? peer_id : conversation.title;
}

void MainWindow::updateConversationItem(const QString& peer_id) {
    if (peer_id.isEmpty()) return;

    refreshConversationList();
}

void MainWindow::refreshConversationList() {
    QString selected_peer = current_chat_target_;
    if (chat_list_widget_->currentItem()) {
        selected_peer = chat_list_widget_->currentItem()->data(Qt::UserRole).toString();
    }

    QList<QString> peer_ids = conversations_.keys();
    // 会话列表优先按最近消息倒序，其次按标题排序，保持列表稳定。
    std::stable_sort(peer_ids.begin(), peer_ids.end(),
                     [this](const QString& left, const QString& right) {
                         const ConversationState& left_conversation = conversations_[left];
                         const ConversationState& right_conversation = conversations_[right];
                         if (left_conversation.last_timestamp != right_conversation.last_timestamp) {
                             return left_conversation.last_timestamp > right_conversation.last_timestamp;
                         }
                         return conversationTitle(left).localeAwareCompare(conversationTitle(right)) < 0;
                     });

    chat_list_widget_->clear();

    for (const QString& peer_id : peer_ids) {
        const ConversationState& conversation = conversations_[peer_id];
        QListWidgetItem* item = new QListWidgetItem;
        item->setData(Qt::UserRole, peer_id);
        item->setData(Qt::AccessibleTextRole, conversationTitle(peer_id));
        item->setSizeHint(QSize(0, 64));
        chat_list_widget_->addItem(item);

        ConversationListItemWidget* item_widget = new ConversationListItemWidget(
            conversationTitle(peer_id),
            conversation.last_message,
            conversation.unread,
            chat_list_widget_);
        chat_list_widget_->setItemWidget(item, item_widget);

        if (peer_id == selected_peer) {
            chat_list_widget_->setCurrentItem(item);
        }
    }

    refreshConversationSelectionStyles();
}

void MainWindow::refreshConversationSelectionStyles() {
    for (int i = 0; i < chat_list_widget_->count(); ++i) {
        QListWidgetItem* item = chat_list_widget_->item(i);
        ConversationListItemWidget* item_widget =
            static_cast<ConversationListItemWidget*>(chat_list_widget_->itemWidget(item));
        if (item_widget) {
            item_widget->setSelected(item->isSelected());
        }
    }
}

QWidget* MainWindow::createMessageRow(const ChatViewMessage& message) {
    const int viewport_width = chat_scroll_area_->viewport()
        ? chat_scroll_area_->viewport()->width()
        : chat_scroll_area_->width();
    const int max_text_width = qMax(180, static_cast<int>(viewport_width * 0.55));
    constexpr int avatar_size = 36;

    const QString display_name = message.is_mine
        ? (user_nickname_.isEmpty() ? user_id_ : user_nickname_)
        : conversationTitle(message.from);
    const QString avatar_value = message.is_mine
        ? current_avatar_url_
        : contact_avatars_.value(message.from);
    const QString profile_user_id = message.is_mine ? user_id_ : message.from;

    QWidget* row = new QWidget(chat_messages_widget_);
    row->setProperty("msg_id", message.msg_id);
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

    QWidget* bubble_row = new QWidget(message_column);
    QHBoxLayout* bubble_row_layout = new QHBoxLayout(bubble_row);
    bubble_row_layout->setContentsMargins(0, 0, 0, 0);
    bubble_row_layout->setSpacing(8);

    QToolButton* avatar_button = new QToolButton(bubble_row);
    avatar_button->setFixedSize(avatar_size, avatar_size);
    avatar_button->setIcon(QIcon(avatarPixmapFromValue(avatar_value, display_name, avatar_size)));
    avatar_button->setIconSize(QSize(avatar_size, avatar_size));
    avatar_button->setCursor(Qt::PointingHandCursor);
    avatar_button->setToolTip(QString("查看%1的个人信息").arg(display_name));
    avatar_button->setStyleSheet(R"(
        QToolButton {
            padding: 0;
            border: none;
            border-radius: 18px;
            background: transparent;
        }
        QToolButton:hover {
            background-color: #eeeeee;
        }
    )");
    avatar_button->setEnabled(!profile_user_id.isEmpty());
    connect(avatar_button, &QToolButton::clicked, this, [this, profile_user_id]() {
        showUserProfile(profile_user_id);
    });

    // 后续媒体消息可以在这里按 content_type 切换为图片、视频或文件卡片组件。
    MessageBubble* bubble = new MessageBubble(message.content, message.is_mine, max_text_width, bubble_row);
    if (message.is_mine) {
        bubble_row_layout->addStretch();
        bubble_row_layout->addWidget(bubble, 0, Qt::AlignTop);
        bubble_row_layout->addWidget(avatar_button, 0, Qt::AlignTop);
    } else {
        bubble_row_layout->addWidget(avatar_button, 0, Qt::AlignTop);
        bubble_row_layout->addWidget(bubble, 0, Qt::AlignTop);
        bubble_row_layout->addStretch();
    }
    column_layout->addWidget(bubble_row);

    QString status = statusText(message.status);
    if (!status.isEmpty()) {
        QLabel* status_label = new QLabel(status, message_column);
        status_label->setObjectName("message_status_label");
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

    return row;
}

void MainWindow::appendMessageRow(const ChatViewMessage& message) {
    const int insert_index = qMax(0, chat_messages_layout_->count() - 1);
    chat_messages_layout_->insertWidget(insert_index, createMessageRow(message));
    chat_messages_widget_->adjustSize();
    chat_messages_widget_->updateGeometry();
    chat_messages_layout_->activate();
    scrollToBottomAnimated();
}

void MainWindow::scrollToBottomAnimated() {
    QTimer::singleShot(0, this, [this]() {
        QScrollBar* bar = chat_scroll_area_->verticalScrollBar();
        if (!bar) return;

        const int end_value = bar->maximum();
        const int start_value = bar->value();
        if (start_value >= end_value) {
            bar->setValue(end_value);
            return;
        }

        if (chat_scroll_animation_) {
            chat_scroll_animation_->stop();
            chat_scroll_animation_->deleteLater();
            chat_scroll_animation_ = nullptr;
        }

        QPropertyAnimation* animation = new QPropertyAnimation(bar, "value", this);
        chat_scroll_animation_ = animation;
        animation->setDuration(220);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->setStartValue(start_value);
        animation->setEndValue(end_value);
        connect(animation, &QPropertyAnimation::finished, this, [this, animation, bar]() {
            if (chat_scroll_animation_ == animation) {
                chat_scroll_animation_ = nullptr;
            }
            bar->setValue(bar->maximum());
            animation->deleteLater();
        });
        animation->start();
    });
}

void MainWindow::rebuildMessageIndex() {
    message_index_by_id_.clear();
    for (int i = 0; i < current_messages_.size(); ++i) {
        if (!current_messages_[i].msg_id.isEmpty()) {
            message_index_by_id_[current_messages_[i].msg_id] = i;
        }
    }
}

bool MainWindow::updateRenderedMessageStatus(const QString& msg_id, const QString& status) {
    if (msg_id.isEmpty()) return false;

    // 优先只更新状态标签；找不到已渲染行时由调用者决定是否整屏重绘。
    const QString text = statusText(status);
    for (int i = 0; i < chat_messages_layout_->count(); ++i) {
        QLayoutItem* item = chat_messages_layout_->itemAt(i);
        QWidget* row = item ? item->widget() : nullptr;
        if (!row || row->property("msg_id").toString() != msg_id) {
            continue;
        }

        QLabel* status_label = row->findChild<QLabel*>("message_status_label");
        if (status_label) {
            status_label->setText(text);
            status_label->setVisible(!text.isEmpty());
            return true;
        }
        return text.isEmpty();
    }

    return false;
}

void MainWindow::renderChatMessages(bool scroll_to_bottom) {
    QScrollBar* scroll_bar = chat_scroll_area_->verticalScrollBar();
    const int previous_scroll_value = scroll_bar ? scroll_bar->value() : 0;
    QWidget* viewport = chat_scroll_area_->viewport();

    // 批量重建消息行时暂停绘制，避免聊天记录刷新过程出现闪烁。
    chat_scroll_area_->setUpdatesEnabled(false);
    if (viewport) {
        viewport->setUpdatesEnabled(false);
    }
    chat_messages_widget_->setUpdatesEnabled(false);

    while (QLayoutItem* item = chat_messages_layout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    for (const ChatViewMessage& message : current_messages_) {
        chat_messages_layout_->addWidget(createMessageRow(message));
    }

    chat_messages_layout_->addStretch();
    chat_messages_widget_->adjustSize();
    chat_messages_widget_->updateGeometry();
    chat_messages_layout_->activate();

    // Qt 布局会延迟计算滚动范围，连续补几次滚动位置能避免偶发不到底。
    auto apply_scroll = [this, scroll_to_bottom, previous_scroll_value]() {
        QScrollBar* bar = chat_scroll_area_->verticalScrollBar();
        if (!bar) return;
        bar->setValue(scroll_to_bottom ? bar->maximum() : previous_scroll_value);
    };

    apply_scroll();
    QTimer::singleShot(0, this, [this, apply_scroll]() {
        apply_scroll();
        chat_messages_widget_->setUpdatesEnabled(true);
        if (QWidget* current_viewport = chat_scroll_area_->viewport()) {
            current_viewport->setUpdatesEnabled(true);
            current_viewport->update();
        }
        chat_scroll_area_->setUpdatesEnabled(true);
        chat_scroll_area_->update();
    });
    QTimer::singleShot(50, this, apply_scroll);
    QTimer::singleShot(150, this, apply_scroll);
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
        renderChatMessages(false);
    }
}

void MainWindow::loadChatList() {
    tcp_client_->getFriendList();
}
