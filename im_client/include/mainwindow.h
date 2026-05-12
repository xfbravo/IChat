/**
 * @file mainwindow.h
 * @brief 客户端主窗口的状态与页面入口声明
 *
 * MainWindow 负责协调导航、消息、联系人、朋友圈和“我”四类页面。
 * 具体页面实现拆分在 mainwindow_*.cpp 中，但共享状态仍保留在这里，
 * 避免不同页面之间复制会话列表、当前聊天对象和用户资料。
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

class QPropertyAnimation;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(TcpClient* tcp_client,
                       const QString& user_id,
                       const QString& nickname,
                       const QString& avatar_url = QString(),
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
    void onUploadAvatarClicked();
    void onAvatarUpdateResult(int code, const QString& message, const QString& avatar_url);
    void onChangePasswordClicked();
    void onPasswordChangeResult(int code, const QString& message);

private:
    /**
     * @brief 聊天区渲染用的消息模型
     *
     * 这里保存的是 UI 层需要的字段，不直接暴露协议 JSON。
     * 后续支持图片、语音、视频时，应优先扩展 content_type/媒体元数据，
     * 再让渲染层根据类型选择不同消息气泡。
     */
    struct ChatViewMessage {
        QString msg_id;
        QString from;
        QString content;
        QString time;
        QString status;
        qint64 timestamp = 0;
        bool is_mine = false;
    };

    // 页面创建入口，具体实现分别位于 mainwindow*.cpp。
    void createNavigationBar();
    void createMessageView();
    void createContactView();
    void createSettingsView();
    void createMomentsView();

    // “我”页工具：头像会被裁剪压缩为 data URL 后通过 TcpClient 同步。
    QString encodeAvatarFile(const QString& file_path);
    void updateAvatarPreview();

    // 消息页工具：维护 conversations_ 与当前聊天窗口的同步。
    void appendMessage(const QString& from, const QString& content, bool is_mine,
                       const QString& msg_id = QString(), const QString& status = QString());
    void renderChatMessages(bool scroll_to_bottom = false);
    QWidget* createMessageRow(const ChatViewMessage& message);
    void appendMessageRow(const ChatViewMessage& message);
    void scrollToBottomAnimated();
    void rebuildMessageIndex();
    bool updateRenderedMessageStatus(const QString& msg_id, const QString& status);
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
    // 登录态和当前用户资料。TcpClient 仍然是唯一网络入口。
    TcpClient* tcp_client_;
    QString user_id_;
    QString user_nickname_;
    QString current_avatar_url_;
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
    QPropertyAnimation* chat_scroll_animation_ = nullptr;

    // Contact View
    QWidget* contact_view_;
    QTreeWidget* contact_tree_widget_;
    QPushButton* add_contact_button_;

    // Placeholder views
    QWidget* moments_view_;
    QWidget* settings_view_;

    // Me View
    QStackedWidget* settings_stack_ = nullptr;
    QLabel* me_avatar_label_ = nullptr;
    QLabel* settings_avatar_label_ = nullptr;
    QLabel* settings_avatar_status_label_ = nullptr;
    QPushButton* upload_avatar_button_ = nullptr;
    QLineEdit* old_password_edit_ = nullptr;
    QLineEdit* new_password_edit_ = nullptr;
    QLineEdit* confirm_password_edit_ = nullptr;
    QLabel* password_status_label_ = nullptr;
    QPushButton* change_password_button_ = nullptr;

    // Status
    QLabel* status_label_;

    // 当前右侧聊天窗口正在展示的消息，以及 msg_id 到下标的快速索引。
    QVector<ChatViewMessage> current_messages_;
    QHash<QString, int> message_index_by_id_;

    // 好友备注缓存，优先级高于服务端返回的昵称。
    QHash<QString, QString> contact_remarks_;
    QHash<QString, QString> contact_avatars_;

    /**
     * @brief 单个会话的本地 UI 状态
     *
     * 会话列表、未读数、最近消息和聊天历史都从这里生成。
     * 服务端返回好友列表或聊天记录后，也会合并回这个结构。
     */
    struct ConversationState {
        QString title;
        QVector<ChatViewMessage> messages;
        int unread = 0;
        QString last_message;
        qint64 last_timestamp = 0;
    };
    QHash<QString, ConversationState> conversations_;
};
