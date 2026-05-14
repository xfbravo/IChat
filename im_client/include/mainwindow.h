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
#include <QComboBox>
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
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QVector>
#include "tcpclient.h"

class QPropertyAnimation;
class QDialog;

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
                               const QString& content_type,
                               const QString& msg_id, qint64 server_timestamp,
                               const QString& server_time);
    void onAttachFileClicked();
    void onFileMessageSent(const QString& to_user_id, const QString& content_type,
                           const QString& content, const QString& msg_id);
    void onFileTransferProgress(const QString& transfer_id, const QString& file_name,
                                qint64 transferred, qint64 total, bool upload);
    void onFileTransferFinished(const QString& transfer_id, const QString& file_name,
                                const QString& save_path, bool upload, bool success,
                                const QString& message);
    void onChatItemClicked(QListWidgetItem* item);
    void onDisconnected();
    void onChatHistoryReceived(const QString& friend_id, const QString& history_json);
    void onOfflineMessageReceived(const QString& from_user_id, const QString& content,
                                  const QString& content_type,
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
    void onSaveProfileClicked();
    void onProfileUpdateResult(int code,
                               const QString& message,
                               const QString& nickname,
                               const QString& gender,
                               const QString& region,
                               const QString& signature);
    void onUserProfileReceived(int code,
                               const QString& message,
                               const QString& user_id,
                               const QString& nickname,
                               const QString& avatar_url,
                               const QString& gender,
                               const QString& region,
                               const QString& signature);
    void onChangePasswordClicked();
    void onPasswordChangeResult(int code, const QString& message);
    void onCreateMomentClicked();
    void onMomentCreateResult(int code, const QString& message);
    void onMomentsReceived(const QString& moments_json);

private:
    /**
     * @brief 头像点击后展示的只读资料缓存
     *
     * 数据来源可能是联系人列表、当前登录用户资料或服务端按用户ID查询结果。
     */
    struct UserProfileCache {
        QString user_id;
        QString display_name;
        QString nickname;
        QString remark;
        QString avatar_url;
        QString gender;
        QString region;
        QString signature;
    };

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
        QString content_type = QStringLiteral("text");
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
    void createMeView();
    void createMomentsView();
    void loadMoments();
    void openMomentsFeed(const QString& target_user_id = QString(),
                         const QString& title = QStringLiteral("朋友圈"),
                         bool allow_create = true);
    void renderMoments(const QJsonArray& moments);
    QWidget* createMomentCard(const QJsonObject& moment);
    QJsonObject encodeMomentImageFile(const QString& file_path);
    void showMomentImageDialog(const QString& image_url);

    // “我”页工具：头像会被裁剪压缩为 data URL 后通过 TcpClient 同步。
    QString encodeAvatarFile(const QString& file_path);
    void updateAvatarPreview();
    void updateMeProfileText();

    // 头像点击入口：聊天、联系人和后续朋友圈/群聊头像都复用这里。
    void showUserProfile(const QString& user_id);
    void showUserProfileDialog(const QString& user_id,
                               const UserProfileCache& profile,
                               bool loading,
                               const QString& status_text = QString());
    void updateUserProfileDialog(const UserProfileCache& profile,
                                 bool loading,
                                 const QString& status_text = QString());
    UserProfileCache cachedUserProfile(const QString& user_id) const;
    void mergeUserProfileCache(const UserProfileCache& profile);

    // 消息页工具：维护 conversations_ 与当前聊天窗口的同步。
    void appendMessage(const QString& from, const QString& content, bool is_mine,
                       const QString& msg_id = QString(), const QString& status = QString(),
                       const QString& content_type = QStringLiteral("text"));
    void renderChatMessages(bool scroll_to_bottom = false);
    QWidget* createMessageRow(const ChatViewMessage& message);
    QWidget* createFileMessageCard(const ChatViewMessage& message, int max_width);
    QWidget* createImageMessageBubble(const ChatViewMessage& message, int max_width);
    QWidget* createVideoMessageBubble(const ChatViewMessage& message, int max_width);
    QString fileMessageTitle(const QString& content) const;
    QString humanFileSize(qint64 size) const;
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
    void rebuildContactList();
    void loadChatList();
    void loadContacts();
    void switchToChatWith(const QString& user_id, const QString& nickname);

private:
    // 登录态和当前用户资料。TcpClient 仍然是唯一网络入口。
    TcpClient* tcp_client_;
    QString user_id_;
    QString user_nickname_;
    QString user_gender_;
    QString user_region_;
    QString user_signature_;
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
    QToolButton* attach_file_button_ = nullptr;
    QPushButton* send_button_;
    QPropertyAnimation* chat_scroll_animation_ = nullptr;

    // Contact View
    QWidget* contact_view_;
    QTreeWidget* contact_tree_widget_;
    QPushButton* add_contact_button_;

    // Top-level views
    QWidget* moments_view_;
    QWidget* me_view_;

    // Moments View
    QScrollArea* moments_scroll_area_ = nullptr;
    QWidget* moments_feed_widget_ = nullptr;
    QVBoxLayout* moments_feed_layout_ = nullptr;
    QLabel* moments_title_label_ = nullptr;
    QLabel* moments_status_label_ = nullptr;
    QPushButton* create_moment_button_ = nullptr;
    QString moments_target_user_id_;
    QString moments_title_text_ = QStringLiteral("朋友圈");
    bool moments_allow_create_ = true;

    // Me View
    QStackedWidget* me_stack_ = nullptr;
    QToolButton* me_profile_button_ = nullptr;
    QLabel* profile_avatar_label_ = nullptr;
    QLabel* avatar_status_label_ = nullptr;
    QLineEdit* profile_nickname_edit_ = nullptr;
    QComboBox* profile_gender_combo_ = nullptr;
    QLineEdit* profile_region_edit_ = nullptr;
    QLineEdit* profile_signature_edit_ = nullptr;
    QLabel* profile_status_label_ = nullptr;
    QPushButton* save_profile_button_ = nullptr;
    bool profile_save_pending_ = false;
    QPushButton* upload_avatar_button_ = nullptr;
    QLineEdit* old_password_edit_ = nullptr;
    QLineEdit* new_password_edit_ = nullptr;
    QLineEdit* confirm_password_edit_ = nullptr;
    QLabel* password_status_label_ = nullptr;
    QPushButton* change_password_button_ = nullptr;

    // 头像点击后打开的只读个人资料弹窗。
    QDialog* user_profile_dialog_ = nullptr;
    QString profile_dialog_user_id_;
    QLabel* view_profile_avatar_label_ = nullptr;
    QLabel* view_profile_name_label_ = nullptr;
    QLabel* view_profile_id_label_ = nullptr;
    QLabel* view_profile_nickname_label_ = nullptr;
    QLabel* view_profile_remark_label_ = nullptr;
    QLabel* view_profile_gender_label_ = nullptr;
    QLabel* view_profile_region_label_ = nullptr;
    QLabel* view_profile_signature_label_ = nullptr;
    QLabel* view_profile_status_label_ = nullptr;
    QPushButton* view_profile_message_button_ = nullptr;

    // Status
    QLabel* status_label_;

    // 当前右侧聊天窗口正在展示的消息，以及 msg_id 到下标的快速索引。
    QVector<ChatViewMessage> current_messages_;
    QHash<QString, int> message_index_by_id_;

    // 好友备注缓存，优先级高于服务端返回的昵称。
    QHash<QString, QString> contact_remarks_;
    QHash<QString, QString> contact_nicknames_;
    QHash<QString, QString> contact_avatars_;
    QHash<QString, UserProfileCache> user_profile_cache_;

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
