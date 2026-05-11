/**
 * @file mainwindow.h
 * @brief 主窗口
 */

#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QTreeWidget>
#include <QSplitter>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QHash>
#include <QVector>
#include "tcpclient.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(TcpClient* tcp_client,
                       const QString& user_id,
                       const QString& nickname,
                       QWidget* parent = nullptr);

signals:
    void logout();

private slots:
    // Navigation
    void onNavigationItemClicked(int index);

    // Chat functions
    void onSendClicked();
    void onChatMessageReceived(const QString& from_user_id, const QString& content,
                               const QString& msg_id, qint64 server_timestamp,
                               const QString& server_time);
    void onChatItemClicked(QListWidgetItem* item);
    void onDisconnected();
    void onChatHistoryReceived(const QString& friend_id, const QString& history_json);
    void onOfflineMessageReceived(const QString& from_user_id, const QString& content,
                                  const QString& msg_id, qint64 server_timestamp,
                                  const QString& server_time);
    void onMessageAckReceived(const QString& msg_id, const QString& status, int code, const QString& message);
    void onFriendRemarkUpdateResult(int code, const QString& message,
                                    const QString& friend_id, const QString& remark);
    void onLoadMoreMessages();

    // Contact functions
    void onAddContactClicked();
    void onViewFriendRequestsClicked();
    void onContactItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onFriendListReceived(const QString& json);
    void onFriendRequestReceived(const QString& from_user_id, const QString& from_nickname, const QString& message);
    void onFriendRequestsReceived(const QString& json);

    void onLogoutClicked();
    void onEditContactRemark();

private:
    struct ChatViewMessage {
        QString msg_id;
        QString from;
        QString content;
        QString time;
        QString status;
        qint64 timestamp = 0;
        bool is_mine = false;
    };

    void createNavigationBar();
    void createMessageView();
    void createContactView();
    void createPlaceholderView(QWidget*& widget, const QString& text);
    void appendMessage(const QString& from, const QString& content, bool is_mine,
                       const QString& msg_id = QString(), const QString& status = QString());
    void renderChatMessages();
    QString statusText(const QString& status) const;
    void markSendingMessagesFailed(const QString& reason);
    void addMessageToConversation(const QString& peer_id, const ChatViewMessage& message, bool count_unread);
    void updateConversationItem(const QString& peer_id);
    void refreshConversationList();
    void refreshConversationSelectionStyles();
    QString conversationTitle(const QString& peer_id) const;
    void loadChatList();
    void loadContacts();
    void switchToChatWith(const QString& user_id, const QString& nickname);

private:
    TcpClient* tcp_client_;
    QString user_id_;
    QString user_nickname_;
    QString current_chat_target_;

    // Navigation
    QWidget* navigation_bar_;
    QListWidget* nav_list_;
    QStackedWidget* content_stacked_;

    // Message View
    QWidget* message_view_;
    QSplitter* message_splitter_;
    QListWidget* chat_list_widget_;
    QWidget* chat_interface_panel_;
    QLabel* chat_target_label_;
    QToolButton* chat_more_button_;
    QScrollArea* chat_scroll_area_;
    QWidget* chat_messages_widget_;
    QVBoxLayout* chat_messages_layout_;
    QLineEdit* message_input_;
    QPushButton* send_button_;

    // Contact View
    QWidget* contact_view_;
    QTreeWidget* contact_tree_widget_;
    QPushButton* add_contact_button_;

    // Placeholder views
    QWidget* moments_view_;
    QWidget* settings_view_;

    // Status
    QLabel* status_label_;

    QVector<ChatViewMessage> current_messages_;
    QHash<QString, int> message_index_by_id_;
    QHash<QString, QString> contact_remarks_;

    struct ConversationState {
        QString title;
        QVector<ChatViewMessage> messages;
        int unread = 0;
        QString last_message;
        qint64 last_timestamp = 0;
    };
    QHash<QString, ConversationState> conversations_;
};
