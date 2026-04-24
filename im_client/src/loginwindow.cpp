/**
 * @file loginwindow.cpp
 * @brief 登录窗口实现
 */

#include "loginwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QDebug>

LoginWindow::LoginWindow(TcpClient* tcp_client, QWidget* parent)
    : QWidget(parent)
    , tcp_client_(tcp_client)
    , server_host_("127.0.0.1")
    , server_port_(8080)
{
    setWindowTitle("IM 客户端 - 登录");
    setMinimumSize(400, 300);
    setStyleSheet(R"(
        QWidget {
            background-color: #f5f5f5;
            font-family: "Microsoft YaHei", sans-serif;
        }
        QLineEdit {
            padding: 8px 12px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 14px;
        }
        QLineEdit:focus {
            border: 1px solid #4CAF50;
        }
        QPushButton {
            padding: 8px 16px;
            border: none;
            border-radius: 4px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton#primary {
            background-color: #4CAF50;
            color: white;
        }
        QPushButton#primary:hover {
            background-color: #45a049;
        }
        QPushButton#primary:pressed {
            background-color: #3d8b40;
        }
        QPushButton#secondary {
            background-color: #2196F3;
            color: white;
        }
        QPushButton#secondary:hover {
            background-color: #1e88e5;
        }
        QLabel#title {
            font-size: 24px;
            font-weight: bold;
            color: #333;
        }
        QLabel#error {
            color: #f44336;
            font-size: 12px;
        }
    )");

    // 创建页面
    stacked_widget_ = new QStackedWidget(this);
    createLoginPage();
    createRegisterPage();

    // 布局
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->addWidget(stacked_widget_);

    // 连接信号槽
    connect(tcp_client_, &TcpClient::connected, this, &LoginWindow::onConnected);
    connect(tcp_client_, &TcpClient::error, this, &LoginWindow::onError);
    connect(tcp_client_, &TcpClient::loginResponse, this, &LoginWindow::onLoginResponse);
    connect(tcp_client_, &TcpClient::registerResponse, this, &LoginWindow::onRegisterResponse);

    // 尝试连接服务器
    tcp_client_->connectToServer(server_host_, server_port_);
}

void LoginWindow::createLoginPage() {
    login_page_ = new QWidget;

    QVBoxLayout* layout = new QVBoxLayout(login_page_);
    layout->setSpacing(20);
    layout->addStretch();

    // 标题
    QLabel* title = new QLabel("IM 客户端", login_page_);
    title->setObjectName("title");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // 错误提示
    login_error_label_ = new QLabel(login_page_);
    login_error_label_->setObjectName("error");
    login_error_label_->setAlignment(Qt::AlignCenter);
    login_error_label_->hide();
    layout->addWidget(login_error_label_);

    // 表单
    QFormLayout* form_layout = new QFormLayout();
    form_layout->setSpacing(12);

    login_user_id_edit_ = new QLineEdit(login_page_);
    login_user_id_edit_->setPlaceholderText("请输入用户ID或手机号");
    login_password_edit_ = new QLineEdit(login_page_);
    login_password_edit_->setPlaceholderText("请输入密码");
    login_password_edit_->setEchoMode(QLineEdit::Password);

    form_layout->addRow("用户ID:", login_user_id_edit_);
    form_layout->addRow("密码:", login_password_edit_);

    layout->addLayout(form_layout);

    // 按钮
    QVBoxLayout* btn_layout = new QVBoxLayout();
    btn_layout->setSpacing(10);

    login_button_ = new QPushButton("登录", login_page_);
    login_button_->setObjectName("primary");
    connect(login_button_, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    btn_layout->addWidget(login_button_);

    to_register_button_ = new QPushButton("没有账号？去注册", login_page_);
    to_register_button_->setCursor(Qt::PointingHandCursor);
    connect(to_register_button_, &QPushButton::clicked, this, &LoginWindow::switchToRegisterPage);
    btn_layout->addWidget(to_register_button_);

    layout->addLayout(btn_layout);
    layout->addStretch();

    stacked_widget_->addWidget(login_page_);
}

void LoginWindow::createRegisterPage() {
    register_page_ = new QWidget;

    QVBoxLayout* layout = new QVBoxLayout(register_page_);
    layout->setSpacing(20);
    layout->addStretch();

    // 标题
    QLabel* title = new QLabel("注册账号", register_page_);
    title->setObjectName("title");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // 错误提示
    register_error_label_ = new QLabel(register_page_);
    register_error_label_->setObjectName("error");
    register_error_label_->setAlignment(Qt::AlignCenter);
    register_error_label_->hide();
    layout->addWidget(register_error_label_);

    // 表单
    QFormLayout* form_layout = new QFormLayout();
    form_layout->setSpacing(12);

    register_phone_edit_ = new QLineEdit(register_page_);
    register_phone_edit_->setPlaceholderText("请输入手机号");

    register_nickname_edit_ = new QLineEdit(register_page_);
    register_nickname_edit_->setPlaceholderText("请输入昵称");

    register_password_edit_ = new QLineEdit(register_page_);
    register_password_edit_->setPlaceholderText("请输入密码（至少6位）");
    register_password_edit_->setEchoMode(QLineEdit::Password);

    register_confirm_password_edit_ = new QLineEdit(register_page_);
    register_confirm_password_edit_->setPlaceholderText("请确认密码");
    register_confirm_password_edit_->setEchoMode(QLineEdit::Password);

    form_layout->addRow("手机号:", register_phone_edit_);
    form_layout->addRow("昵称:", register_nickname_edit_);
    form_layout->addRow("密码:", register_password_edit_);
    form_layout->addRow("确认密码:", register_confirm_password_edit_);

    layout->addLayout(form_layout);

    // 按钮
    QVBoxLayout* btn_layout = new QVBoxLayout();
    btn_layout->setSpacing(10);

    register_button_ = new QPushButton("注册", register_page_);
    register_button_->setObjectName("primary");
    connect(register_button_, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
    btn_layout->addWidget(register_button_);

    to_login_button_ = new QPushButton("已有账号？去登录", register_page_);
    to_login_button_->setCursor(Qt::PointingHandCursor);
    connect(to_login_button_, &QPushButton::clicked, this, &LoginWindow::switchToLoginPage);
    btn_layout->addWidget(to_login_button_);

    layout->addLayout(btn_layout);
    layout->addStretch();

    stacked_widget_->addWidget(register_page_);
}

void LoginWindow::onLoginClicked() {
    clearError();

    QString user_id = login_user_id_edit_->text().trimmed();
    QString password = login_password_edit_->text();

    if (user_id.isEmpty()) {
        showError("请输入用户ID");
        return;
    }

    if (password.isEmpty()) {
        showError("请输入密码");
        return;
    }

    login_button_->setEnabled(false);
    tcp_client_->login(user_id, password);
}

void LoginWindow::onRegisterClicked() {
    clearError();

    QString phone = register_phone_edit_->text().trimmed();
    QString nickname = register_nickname_edit_->text().trimmed();
    QString password = register_password_edit_->text();
    QString confirm = register_confirm_password_edit_->text();

    if (phone.isEmpty()) {
        showError("请输入手机号");
        return;
    }

    if (nickname.isEmpty()) {
        showError("请输入昵称");
        return;
    }

    if (password.length() < 6) {
        showError("密码长度至少6位");
        return;
    }

    if (password != confirm) {
        showError("两次密码不一致");
        return;
    }

    register_button_->setEnabled(false);
    tcp_client_->registerUser(phone, nickname, password);
}

void LoginWindow::onLoginResponse(int code, const QString& message,
                                 const QString& user_id, const QString& nickname,
                                 const QString& token) {
    Q_UNUSED(token);

    login_button_->setEnabled(true);

    if (code == 0) {
        qDebug() << "Login success:" << user_id << nickname;
        emit loginSuccess(user_id, nickname);
    } else {
        showError(message);
    }
}

void LoginWindow::onRegisterResponse(int code, const QString& message, const QString& user_id) {
    Q_UNUSED(user_id);

    register_button_->setEnabled(true);

    if (code == 0) {
        QMessageBox::information(this, "注册成功", "账号注册成功，请登录！");
        switchToLoginPage();

        // 清空注册表单
        register_phone_edit_->clear();
        register_nickname_edit_->clear();
        register_password_edit_->clear();
        register_confirm_password_edit_->clear();
    } else {
        showError(message);
    }
}

void LoginWindow::onConnected() {
    qDebug() << "Connected to server";
    clearError();
}

void LoginWindow::onError(const QString& error_string) {
    qDebug() << "Connection error:" << error_string;
    showError("连接失败：" + error_string);
}

void LoginWindow::switchToLoginPage() {
    stacked_widget_->setCurrentWidget(login_page_);
    setWindowTitle("IM 客户端 - 登录");
}

void LoginWindow::switchToRegisterPage() {
    stacked_widget_->setCurrentWidget(register_page_);
    setWindowTitle("IM 客户端 - 注册");
}

void LoginWindow::showError(const QString& message) {
    // 根据当前页面显示错误
    if (stacked_widget_->currentWidget() == login_page_) {
        login_error_label_->setText(message);
        login_error_label_->show();
    } else {
        register_error_label_->setText(message);
        register_error_label_->show();
    }
}

void LoginWindow::clearError() {
    login_error_label_->hide();
    register_error_label_->hide();
}
