/**
 * @file addcontactdialog.h
 * @brief 添加联系人对话框
 */

#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include "tcpclient.h"

class AddContactDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddContactDialog(TcpClient* tcp_client, QWidget* parent = nullptr);
    ~AddContactDialog() = default;

signals:
    void contactAdded(const QString& user_id);
    void requestSent();

private slots:
    void onSendClicked();
    void onRequestResult(int code, const QString& message);

private:
    TcpClient* tcp_client_;
    QLineEdit* phone_edit_;
    QLineEdit* remark_edit_;
    QPushButton* send_button_;
    QPushButton* cancel_button_;
};