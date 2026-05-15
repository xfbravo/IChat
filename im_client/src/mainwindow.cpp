/**
 * @file mainwindow.cpp
 * @brief MainWindow 导航壳和全局信号连接
 */

#include "mainwindow.h"
#include "addcontactdialog.h"
#include <QStatusBar>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QFrame>
#include <QHeaderView>
#include <QDate>
#include <QDateTime>
#include <QStyle>
#include <QTreeWidgetItem>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QScrollArea>
#include <QScrollBar>
#include <QListView>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPen>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QSize>
#include <QStringList>
#include <QToolButton>
#include <QTimer>
#include <QBuffer>
#include <QFileDialog>
#include <QFont>
#include <QImage>
#include <QImageReader>
#include <QIODevice>
#include <algorithm>
#include "mainwindow_helpers.h"

using namespace mainwindow_detail;

MainWindow::MainWindow(TcpClient* tcp_client,
                      const QString& user_id,
                      const QString& nickname,
                      const QString& avatar_url,
                      QWidget* parent)
    : QMainWindow(parent)
    , tcp_client_(tcp_client)
    , user_id_(user_id)
    , user_nickname_(nickname)
    , user_gender_(tcp_client ? tcp_client->gender() : QString())
    , user_region_(tcp_client ? tcp_client->region() : QString())
    , user_signature_(tcp_client ? tcp_client->signature() : QString())
    , current_avatar_url_(avatar_url)
{
    if (current_avatar_url_.isEmpty() && tcp_client_) {
        current_avatar_url_ = tcp_client_->avatarUrl();
    }

    setWindowTitle(QString("IChat - %1").arg(nickname));
    setMinimumSize(1000, 700);

    // 页面本身在各自的 mainwindow_*.cpp 中创建，这里只负责组装主框架。
    createNavigationBar();
    createMessageView();
    createContactView();
    createMomentsView();
    createMeView();

    // StackedWidget 的索引必须与左侧 nav_items 顺序保持一致。
    content_stacked_ = new QStackedWidget;
    content_stacked_->addWidget(message_view_);      // 0 - 消息
    content_stacked_->addWidget(contact_view_);     // 1 - 联系人
    content_stacked_->addWidget(moments_view_);     // 2 - 朋友圈
    content_stacked_->addWidget(me_view_);          // 3 - 我
    content_stacked_->setCurrentIndex(0);

    // 主布局：导航栏 + 内容区
    QHBoxLayout* main_layout = new QHBoxLayout;
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);
    main_layout->addWidget(navigation_bar_);
    main_layout->addWidget(content_stacked_, 1);

    QWidget* central = new QWidget;
    central->setLayout(main_layout);
    setCentralWidget(central);

    // 加载聊天列表
    loadChatList();

    // 状态栏
    status_label_ = new QLabel("在线");
    statusBar()->addWidget(status_label_);

    // TcpClient 是网络层和窗口层的边界：窗口只处理信号，不直接解析 socket。
    connect(tcp_client_, &TcpClient::chatMessageReceived,
            this, &MainWindow::onChatMessageReceived);
    connect(tcp_client_, &TcpClient::disconnected,
            this, &MainWindow::onDisconnected);
    connect(tcp_client_, &TcpClient::friendListReceived,
            this, &MainWindow::onFriendListReceived);
    connect(tcp_client_, &TcpClient::groupListReceived,
            this, &MainWindow::onGroupListReceived);
    connect(tcp_client_, &TcpClient::groupCreateResult,
            this, &MainWindow::onGroupCreateResult);
    connect(tcp_client_, &TcpClient::friendRequestReceived,
            this, &MainWindow::onFriendRequestReceived);
    connect(tcp_client_, &TcpClient::friendRequestsReceived,
            this, &MainWindow::onFriendRequestsReceived);
    connect(tcp_client_, &TcpClient::chatHistoryReceived,
            this, &MainWindow::onChatHistoryReceived);
    connect(tcp_client_, &TcpClient::offlineMessageReceived,
            this, &MainWindow::onOfflineMessageReceived);
    connect(tcp_client_, &TcpClient::messageAckReceived,
            this, &MainWindow::onMessageAckReceived);
    connect(tcp_client_, &TcpClient::fileMessageSent,
            this, &MainWindow::onFileMessageSent);
    connect(tcp_client_, &TcpClient::fileTransferProgress,
            this, &MainWindow::onFileTransferProgress);
    connect(tcp_client_, &TcpClient::fileTransferFinished,
            this, &MainWindow::onFileTransferFinished);
    connect(tcp_client_, &TcpClient::friendRemarkUpdateResult,
            this, &MainWindow::onFriendRemarkUpdateResult);
    connect(tcp_client_, &TcpClient::avatarUpdateResult,
            this, &MainWindow::onAvatarUpdateResult);
    connect(tcp_client_, &TcpClient::profileUpdateResult,
            this, &MainWindow::onProfileUpdateResult);
    connect(tcp_client_, &TcpClient::userProfileReceived,
            this, &MainWindow::onUserProfileReceived);
    connect(tcp_client_, &TcpClient::passwordChangeResult,
            this, &MainWindow::onPasswordChangeResult);
    connect(tcp_client_, &TcpClient::momentCreateResult,
            this, &MainWindow::onMomentCreateResult);
    connect(tcp_client_, &TcpClient::momentsReceived,
            this, &MainWindow::onMomentsReceived);
}

void MainWindow::createNavigationBar() {
    navigation_bar_ = new QWidget;
    navigation_bar_->setFixedWidth(76);
    navigation_bar_->setStyleSheet(R"(
        QWidget {
            background-color: #16221c;
            font-family: "Microsoft YaHei", sans-serif;
        }
    )");

    QVBoxLayout* layout = new QVBoxLayout(navigation_bar_);
    layout->setContentsMargins(0, 14, 0, 12);
    layout->setSpacing(10);

    // 标题
    QLabel* title = new QLabel("I");
    title->setStyleSheet(R"(
        QLabel {
            color: #16221c;
            background-color: #e7efe8;
            border: 1px solid #d2ded5;
            border-radius: 12px;
            font-size: 18px;
            font-weight: 800;
        }
    )");
    title->setFixedSize(38, 38);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title, 0, Qt::AlignHCenter);

    // 导航列表
    nav_list_ = new QListWidget;
    nav_list_->setViewMode(QListView::IconMode);
    nav_list_->setMovement(QListView::Static);
    nav_list_->setResizeMode(QListView::Adjust);
    nav_list_->setFlow(QListView::TopToBottom);
    nav_list_->setIconSize(QSize(26, 26));
    nav_list_->setGridSize(QSize(76, 62));
    nav_list_->setSpacing(4);
    nav_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    nav_list_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    nav_list_->setTextElideMode(Qt::ElideNone);
    nav_list_->setStyleSheet(R"(
        QListWidget {
            background-color: #16221c;
            border: none;
            color: #d6e1d9;
            outline: none;
        }
        QListWidget::item {
            margin: 3px 8px;
            padding: 7px 0;
            border-radius: 8px;
            font-size: 11px;
            font-weight: 600;
            color: #d6e1d9;
        }
        QListWidget::item:selected {
            background-color: #2f6f3e;
            color: #ffffff;
        }
        QListWidget::item:hover {
            background-color: #24362d;
        }
    )");
    // 新增页面时同时更新这里、content_stacked_ 添加顺序和 onNavigationItemClicked。
    const QList<QPair<QString, QString>> nav_items = {
        {"message", "消息"},
        {"contacts", "联系人"},
        {"moments", "朋友圈"},
        {"me", "设置"}
    };
    for (const auto& item : nav_items) {
        QListWidgetItem* nav_item = new QListWidgetItem(navIcon(item.first), item.second);
        nav_item->setTextAlignment(Qt::AlignCenter);
        nav_item->setSizeHint(QSize(76, 62));
        nav_list_->addItem(nav_item);
    }
    nav_list_->setCurrentRow(0);
    connect(nav_list_, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavigationItemClicked);

    layout->addWidget(nav_list_);
    layout->addStretch();
}

void MainWindow::onNavigationItemClicked(int index) {
    if (!content_stacked_) return;
    hideSearchResults();

    // 进入页面时按需刷新数据，避免联系人/消息列表长时间停留在旧状态。
    switch (index) {
        case 0: // 消息
            content_stacked_->setCurrentWidget(message_view_);
            loadChatList();
            break;
        case 1: // 联系人
            content_stacked_->setCurrentWidget(contact_view_);
            loadContacts();
            break;
        case 2: // 朋友圈
            content_stacked_->setCurrentWidget(moments_view_);
            openMomentsFeed();
            break;
        case 3: // 我
            content_stacked_->setCurrentWidget(me_view_);
            if (me_stack_) {
                me_stack_->setCurrentIndex(0);
            }
            updateAvatarPreview();
            updateMeProfileText();
            if (profile_status_label_) {
                profile_status_label_->setText("正在同步资料...");
            }
            if (tcp_client_ && tcp_client_->state() == ClientState::LoggedIn) {
                tcp_client_->getUserProfile(user_id_);
            }
            break;
    }
}

void MainWindow::onLogoutClicked() {
    const QMessageBox::StandardButton choice = QMessageBox::question(
        this,
        "退出登录",
        "确定要退出当前账号吗？",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        return;
    }

    tcp_client_->disconnectFromServer();
    emit logout();
}
