/**
 * @file mainwindow.h
 * @brief 主窗口
 */

#pragma once

#include <QMainWindow>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QTreeWidget>
#include <QSplitter>
#include <QLabel>
#include <QStackedWidget>
#include <QWidget>
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
    void onChatMessageReceived(const QString& from_user_id, const QString& content);
    void onChatItemClicked(QListWidgetItem* item);
    void onDisconnected();

    // Contact functions
    void onAddContactClicked();
    void onViewFriendRequestsClicked();
    void onContactItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onFriendListReceived(const QString& json);
    void onFriendRequestReceived(const QString& from_user_id, const QString& from_nickname, const QString& message);
    void onFriendRequestsReceived(const QString& json);

    void onLogoutClicked();

private:
    void createNavigationBar();
    void createMessageView();
    void createContactView();
    void createPlaceholderView(QWidget*& widget, const QString& text);
    void appendMessage(const QString& from, const QString& content, bool is_mine);
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
    QTextEdit* chat_display_;
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
};