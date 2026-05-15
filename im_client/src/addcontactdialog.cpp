/**
 * @file addcontactdialog.cpp
 * @brief 添加联系人对话框实现
 */

#include "addcontactdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>

AddContactDialog::AddContactDialog(TcpClient* tcp_client, QWidget* parent)
    : QDialog(parent)
    , tcp_client_(tcp_client)
{
    setWindowTitle("添加联系人");
    setMinimumWidth(430);
    setStyleSheet(R"(
        QDialog {
            background-color: #eef2ef;
            font-family: "Microsoft YaHei", sans-serif;
        }
        QLabel {
            color: #425247;
            font-size: 13px;
            font-weight: 700;
            background: transparent;
        }
        QLineEdit {
            min-height: 38px;
            padding: 0 12px;
            border: 1px solid #d5dfd8;
            border-radius: 6px;
            background-color: #ffffff;
            color: #17211c;
            font-size: 14px;
        }
        QLineEdit:focus {
            border: 1px solid #2f6f3e;
            background-color: #fbfdfb;
        }
        QPushButton {
            min-height: 36px;
            padding: 0 18px;
            border: none;
            border-radius: 6px;
            font-size: 14px;
            font-weight: 700;
        }
        QPushButton#primary {
            background-color: #2f6f3e;
            color: white;
        }
        QPushButton#primary:hover {
            background-color: #285f36;
        }
        QPushButton#primary:disabled {
            background-color: #c7d4cb;
        }
        QPushButton#secondary {
            background-color: #ffffff;
            color: #425247;
            border: 1px solid #d5dfd8;
        }
        QPushButton#secondary:hover {
            background-color: #eef6ef;
            border-color: #b8cdbc;
        }
    )");

    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(22, 22, 22, 20);
    main_layout->setSpacing(12);

    // 手机号输入
    QLabel* phone_label = new QLabel("手机号", this);
    phone_edit_ = new QLineEdit(this);
    phone_edit_->setPlaceholderText("请输入对方手机号");

    // 备注输入
    QLabel* remark_label = new QLabel("验证信息（可选）", this);
    remark_edit_ = new QLineEdit(this);
    remark_edit_->setPlaceholderText("请输入验证信息");

    main_layout->addWidget(phone_label);
    main_layout->addWidget(phone_edit_);
    main_layout->addWidget(remark_label);
    main_layout->addWidget(remark_edit_);

    // 按钮
    QHBoxLayout* btn_layout = new QHBoxLayout();
    btn_layout->setContentsMargins(0, 8, 0, 0);
    btn_layout->setSpacing(10);
    btn_layout->addStretch();

    cancel_button_ = new QPushButton("取消", this);
    cancel_button_->setObjectName("secondary");
    connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);

    send_button_ = new QPushButton("发送请求", this);
    send_button_->setObjectName("primary");
    connect(send_button_, &QPushButton::clicked, this, &AddContactDialog::onSendClicked);

    btn_layout->addWidget(cancel_button_);
    btn_layout->addWidget(send_button_);

    main_layout->addLayout(btn_layout);

    // 连接信号
    connect(tcp_client_, &TcpClient::friendRequestResult, this, &AddContactDialog::onRequestResult);
}

void AddContactDialog::onSendClicked() {
    QString phone = phone_edit_->text().trimmed();

    if (phone.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入对方手机号");
        return;
    }

    QString remark = remark_edit_->text().trimmed();

    send_button_->setEnabled(false);
    tcp_client_->sendFriendRequest(phone, remark);
}

void AddContactDialog::onRequestResult(int code, const QString& message) {
    send_button_->setEnabled(true);

    if (code == 0) {
        QMessageBox::information(this, "成功", message);
        accept();
    } else {
        QMessageBox::warning(this, "失败", message);
    }
}
