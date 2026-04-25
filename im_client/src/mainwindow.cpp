/**
 * @file mainwindow.cpp
 * @brief 主窗口实现
 */

#include "mainwindow.h"
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QDateTime>
#include <QStyle>

MainWindow::MainWindow(TcpClient* tcp_client,
                      const QString& user_id,
                      const QString& nickname,
                      QWidget* parent)
    : QMainWindow(parent)
    , tcp_client_(tcp_client)
    , user_id_(user_id)
    , user_nickname_(nickname)
{
    setWindowTitle(QString("IM 客户端 - %1").arg(nickname));
    setMinimumSize(800, 600);

    createUI();

    // 连接信号槽
    connect(tcp_client_, &TcpClient::chatMessageReceived,
            this, &MainWindow::onChatMessageReceived);
    connect(tcp_client_, &TcpClient::disconnected,
            this, &MainWindow::onDisconnected);
}

void MainWindow::createUI() {
    // 创建右侧聊天区域
    createChatArea();

    // 创建左侧好友列表
    createFriendList();

    // 分割器
    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->addWidget(friend_list_widget_);
    splitter_->addWidget(chat_widget_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 3);

    setCentralWidget(splitter_);

    // 状态栏
    status_label_ = new QLabel("在线", this);
    statusBar()->addWidget(status_label_);
}

void MainWindow::createChatArea() {
    chat_widget_ = new QWidget;

    QVBoxLayout* layout = new QVBoxLayout(chat_widget_);
    layout->setContentsMargins(0, 0, 0, 0);

    // 聊天显示区
    chat_display_ = new QTextEdit(chat_widget_);
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
    layout->addWidget(chat_display_);

    // 输入区
    QWidget* input_widget = new QWidget(chat_widget_);
    QHBoxLayout* input_layout = new QHBoxLayout(input_widget);
    input_layout->setContentsMargins(10, 10, 10, 10);

    message_input_ = new QLineEdit(input_widget);
    message_input_->setPlaceholderText("输入消息...");
    message_input_->setStyleSheet(R"(
        QLineEdit {
            padding: 8px 12px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 14px;
        }
    )");
    connect(message_input_, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);

    send_button_ = new QPushButton("发送", input_widget);
    send_button_->setStyleSheet(R"(
        QPushButton {
            padding: 8px 20px;
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
    connect(send_button_, &QPushButton::clicked, this, &MainWindow::onSendClicked);

    QPushButton* logout_btn = new QPushButton("登出", input_widget);
    logout_btn->setStyleSheet(R"(
        QPushButton {
            padding: 8px 16px;
            background-color: #f44336;
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #da190b;
        }
    )");
    connect(logout_btn, &QPushButton::clicked, this, &MainWindow::onLogoutClicked);

    input_layout->addWidget(message_input_);
    input_layout->addWidget(send_button_);
    input_layout->addWidget(logout_btn);

    layout->addWidget(input_widget);
}

void MainWindow::createFriendList() {
    friend_list_widget_ = new QListWidget;
    friend_list_widget_->setStyleSheet(R"(
        QListWidget {
            border: none;
            background-color: #f5f5f5;
            font-family: "Microsoft YaHei", sans-serif;
        }
        QListWidget::item {
            padding: 10px;
            border-bottom: 1px solid #e0e0e0;
        }
        QListWidget::item:selected {
            background-color: #e8f5e9;
        }
    )");

    // 添加测试好友
    friend_list_widget_->addItem("user_001 - 张三");
    friend_list_widget_->addItem("user_002 - 李四");
    friend_list_widget_->addItem("user_003 - 王五");
}

void MainWindow::onSendClicked() {
    QString message = message_input_->text().trimmed();
    if (message.isEmpty()) {
        return;
    }

    // 默认发送给选中的好友，如果没有选中则发送给第一个
    QString target = currentChatTarget();
    if (target.isEmpty()) {
        if (friend_list_widget_->count() > 0) {
            QListWidgetItem* item = friend_list_widget_->item(0);
            target = item->text().split(" - ").first();
        }
    }

    if (!target.isEmpty()) {
        tcp_client_->sendChatMessage(target, "text", message);
        appendMessage(user_nickname_, message, true);
        message_input_->clear();
    }
}

void MainWindow::onChatMessageReceived(const QString& from_user_id, const QString& content) {
    appendMessage(from_user_id, content, false);
}

void MainWindow::onDisconnected() {
    status_label_->setText("离线");
    message_input_->setEnabled(false);
    send_button_->setEnabled(false);
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

QString MainWindow::currentChatTarget() const {
    QListWidgetItem* item = friend_list_widget_->currentItem();
    if (item) {
        return item->text().split(" - ").first();
    }
    return QString();
}
