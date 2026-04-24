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
#include <QSplitter>
#include <QLabel>
#include "tcpclient.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param tcp_client TCP客户端指针
     * @param user_id 用户ID
     * @param nickname 昵称
     * @param parent 父对象
     */
    explicit MainWindow(TcpClient* tcp_client,
                       const QString& user_id,
                       const QString& nickname,
                       QWidget* parent = nullptr);

signals:
    /**
     * @brief 登出信号
     */
    void logout();

private slots:
    /**
     * @brief 处理发送消息
     */
    void onSendClicked();

    /**
     * @brief 处理收到消息
     */
    void onChatMessageReceived(const QString& from_user_id, const QString& content);

    /**
     * @brief 处理断开连接
     */
    void onDisconnected();

    /**
     * @brief 处理登出按钮
     */
    void onLogoutClicked();

private:
    /**
     * @brief 创建UI
     */
    void createUI();

    /**
     * @brief 创建聊天区域
     */
    void createChatArea();

    /**
     * @brief 创建好友列表
     */
    void createFriendList();

    /**
     * @brief 添加消息到聊天区域
     */
    void appendMessage(const QString& from, const QString& content, bool is_mine);

    /**
     * @brief 获取当前选中的聊天对象
     */
    QString currentChatTarget() const;

private:
    TcpClient* tcp_client_;
    QString user_id_;
    QString user_nickname_;
    QString current_chat_target_;

    // UI 组件
    QSplitter* splitter_;
    QListWidget* friend_list_widget_;
    QWidget* chat_widget_;
    QTextEdit* chat_display_;
    QLineEdit* message_input_;
    QPushButton* send_button_;
    QLabel* status_label_;
};
