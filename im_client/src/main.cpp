/**
 * @file main.cpp
 * @brief 程序入口
 */

#include <QApplication>
#include "loginwindow.h"
#include "mainwindow.h"
#include "tcpclient.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 设置应用信息
    app.setApplicationName("IM Client");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("IM");

    // 创建 TCP 客户端
    TcpClient tcp_client;

    // 创建登录窗口
    LoginWindow login_window(&tcp_client);

    // 主窗口指针
    MainWindow* main_window = nullptr;

    // 登录成功后显示主窗口
    QObject::connect(&login_window, &LoginWindow::loginSuccess,
                     [&](const QString& user_id, const QString& nickname) {
        login_window.hide();

        if (main_window) {
            delete main_window;
        }
        main_window = new MainWindow(&tcp_client, user_id, nickname);

        QObject::connect(main_window, &MainWindow::logout, [&]() {
            delete main_window;
            main_window = nullptr;
            login_window.show();
        });

        main_window->show();
    });

    // 显示登录窗口
    login_window.show();

    return app.exec();
}
