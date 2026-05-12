/**
 * @file mainwindow_moments.cpp
 * @brief MainWindow 朋友圈占位页
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

void MainWindow::createMomentsView() {
    moments_view_ = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(moments_view_);

    // 朋友圈后续应在本文件扩展为独立页面，不与单聊消息列表复用状态。
    QLabel* label = new QLabel("朋友圈功能开发中...");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(R"(
        QLabel {
            color: #888888;
            font-size: 16px;
        }
    )");
    layout->addWidget(label);
}
