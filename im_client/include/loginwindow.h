/**
 * @file loginwindow.h
 * @brief 登录窗口
 */

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QStatusBar>
#include "tcpclient.h"

class LoginWindow : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param tcp_client TCP客户端指针
     * @param parent 父对象
     */
    explicit LoginWindow(TcpClient* tcp_client, QWidget* parent = nullptr);

signals:
    /**
     * @brief 登录成功信号
     */
    void loginSuccess(const QString& user_id, const QString& nickname);

private slots:
    /**
     * @brief 处理登录按钮点击
     */
    void onLoginClicked();

    /**
     * @brief 处理注册按钮点击
     */
    void onRegisterClicked();

    /**
     * @brief 处理登录响应
     */
    void onLoginResponse(int code, const QString& message,
                        const QString& user_id, const QString& nickname,
                        const QString& token);

    /**
     * @brief 处理注册响应
     */
    void onRegisterResponse(int code, const QString& message, const QString& user_id);

    /**
     * @brief 处理连接成功
     */
    void onConnected();

    /**
     * @brief 处理连接错误
     */
    void onError(const QString& error_string);

    /**
     * @brief 处理连接断开
     */
    void onDisconnected();

    /**
     * @brief 处理连接状态变化
     */
    void onConnectionStatusChanged(bool is_connected);

    /**
     * @brief 切换到登录页面
     */
    void switchToLoginPage();

    /**
     * @brief 切换到注册页面
     */
    void switchToRegisterPage();

private:
    /**
     * @brief 创建登录页面
     */
    void createLoginPage();

    /**
     * @brief 创建注册页面
     */
    void createRegisterPage();

    /**
     * @brief 显示错误提示
     * @param message 错误信息
     * @param is_login_page true 显示在登录页，false 显示在注册页
     */
    void showError(const QString& message, bool is_login_page);

    /**
     * @brief 清除错误提示
     */
    void clearError();

    /**
     * @brief 更新连接状态显示
     */
    void updateConnectionStatus(bool is_connected);

private:
    TcpClient* tcp_client_;

    QStackedWidget* stacked_widget_;
    QLabel* connection_status_label_;

    // 登录页面
    QWidget* login_page_;
    QLineEdit* login_user_id_edit_;
    QLineEdit* login_password_edit_;
    QPushButton* login_button_;
    QPushButton* to_register_button_;
    QLabel* login_error_label_;

    // 注册页面
    QWidget* register_page_;
    QLineEdit* register_phone_edit_;
    QLineEdit* register_nickname_edit_;
    QLineEdit* register_password_edit_;
    QLineEdit* register_confirm_password_edit_;
    QPushButton* register_button_;
    QPushButton* to_login_button_;
    QLabel* register_error_label_;

    QString server_host_;
    quint16 server_port_;
};
