/**
 * @file mainwindow_moments.cpp
 * @brief MainWindow 朋友圈页面
 */

#include "mainwindow.h"
#include <QBuffer>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QSizePolicy>
#include "mainwindow_helpers.h"

using namespace mainwindow_detail;

namespace {

QPixmap imagePixmapFromDataUrl(const QString& data_url,
                               const QSize& target_size,
                               Qt::AspectRatioMode aspect_mode = Qt::KeepAspectRatioByExpanding) {
    const int comma_index = data_url.indexOf(',');
    if (comma_index <= 0) {
        return QPixmap();
    }

    const QByteArray bytes = QByteArray::fromBase64(data_url.mid(comma_index + 1).toLatin1());
    QPixmap pixmap;
    pixmap.loadFromData(bytes);
    if (pixmap.isNull()) {
        return pixmap;
    }
    return pixmap.scaled(target_size, aspect_mode, Qt::SmoothTransformation);
}

void clearLayout(QLayout* layout) {
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        if (QLayout* child = item->layout()) {
            clearLayout(child);
        }
        delete item;
    }
}

QStringList imageFilters() {
    return {"图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)"};
}

} // namespace

void MainWindow::createMomentsView() {
    moments_view_ = new QWidget;
    moments_view_->setStyleSheet("QWidget { background: #f5f6f7; }");

    QVBoxLayout* root = new QVBoxLayout(moments_view_);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QWidget* top_bar = new QWidget(moments_view_);
    top_bar->setFixedHeight(58);
    top_bar->setStyleSheet(R"(
        QWidget {
            background: #ffffff;
            border-bottom: 1px solid #e8eaed;
        }
    )");

    QHBoxLayout* top_layout = new QHBoxLayout(top_bar);
    top_layout->setContentsMargins(16, 0, 18, 0);
    top_layout->setSpacing(12);

    create_moment_button_ = new QPushButton("+", top_bar);
    create_moment_button_->setFixedSize(34, 34);
    create_moment_button_->setToolTip("发布朋友圈");
    create_moment_button_->setStyleSheet(R"(
        QPushButton {
            background: #1aad19;
            color: white;
            border: none;
            border-radius: 17px;
            font-size: 24px;
            font-weight: 500;
            padding-bottom: 3px;
        }
        QPushButton:hover { background: #159b15; }
        QPushButton:pressed { background: #118411; }
    )");
    connect(create_moment_button_, &QPushButton::clicked,
            this, &MainWindow::onCreateMomentClicked);
    top_layout->addWidget(create_moment_button_, 0, Qt::AlignVCenter);

    moments_title_label_ = new QLabel("朋友圈", top_bar);
    moments_title_label_->setStyleSheet(R"(
        QLabel {
            color: #111827;
            font-size: 20px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
    )");
    top_layout->addWidget(moments_title_label_);
    top_layout->addStretch();

    moments_status_label_ = new QLabel("正在加载...", top_bar);
    moments_status_label_->setStyleSheet(R"(
        QLabel {
            color: #6b7280;
            font-size: 13px;
            background: transparent;
            border: none;
        }
    )");
    top_layout->addWidget(moments_status_label_, 0, Qt::AlignVCenter);
    root->addWidget(top_bar);

    moments_scroll_area_ = new QScrollArea(moments_view_);
    moments_scroll_area_->setWidgetResizable(true);
    moments_scroll_area_->setFrameShape(QFrame::NoFrame);
    moments_scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    moments_feed_widget_ = new QWidget;
    moments_feed_widget_->setStyleSheet("background: #f5f6f7;");
    moments_feed_layout_ = new QVBoxLayout(moments_feed_widget_);
    moments_feed_layout_->setContentsMargins(24, 20, 24, 24);
    moments_feed_layout_->setSpacing(14);
    moments_feed_layout_->addStretch();

    moments_scroll_area_->setWidget(moments_feed_widget_);
    root->addWidget(moments_scroll_area_, 1);
}

void MainWindow::loadMoments() {
    if (moments_status_label_) {
        moments_status_label_->setText("正在加载...");
    }
    if (tcp_client_ && tcp_client_->state() == ClientState::LoggedIn) {
        tcp_client_->getMoments(50, moments_target_user_id_);
    } else if (moments_status_label_) {
        moments_status_label_->setText("离线");
    }
}

void MainWindow::openMomentsFeed(const QString& target_user_id,
                                 const QString& title,
                                 bool allow_create) {
    moments_target_user_id_ = target_user_id.trimmed();
    moments_title_text_ = title.trimmed().isEmpty() ? QStringLiteral("朋友圈") : title.trimmed();
    moments_allow_create_ = allow_create;

    if (moments_title_label_) {
        moments_title_label_->setText(moments_title_text_);
    }
    if (create_moment_button_) {
        create_moment_button_->setVisible(moments_allow_create_);
        create_moment_button_->setEnabled(moments_allow_create_);
    }
    if (content_stacked_ && moments_view_) {
        content_stacked_->setCurrentWidget(moments_view_);
    }
    if (nav_list_) {
        const bool blocked = nav_list_->blockSignals(true);
        nav_list_->setCurrentRow(2);
        nav_list_->blockSignals(blocked);
    }
    loadMoments();
}

void MainWindow::onCreateMomentClicked() {
    QDialog dialog(this);
    dialog.setWindowTitle("发布朋友圈");
    dialog.setMinimumWidth(560);

    QJsonArray images;

    QVBoxLayout* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);

    QPlainTextEdit* content_edit = new QPlainTextEdit(&dialog);
    content_edit->setPlaceholderText("这一刻的想法...");
    content_edit->setFixedHeight(120);
    content_edit->setStyleSheet(R"(
        QPlainTextEdit {
            border: 1px solid #d7dce2;
            border-radius: 6px;
            padding: 10px;
            font-size: 14px;
            background: white;
        }
    )");
    root->addWidget(content_edit);

    QLabel* hint = new QLabel("可发布纯文字、最多九张图片，或文字加图片。图片列表展示缩略图，点击后查看正常大小。", &dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #6b7280; font-size: 12px;");
    root->addWidget(hint);

    QListWidget* media_list = new QListWidget(&dialog);
    media_list->setFixedHeight(120);
    media_list->setStyleSheet(R"(
        QListWidget {
            border: 1px solid #e5e7eb;
            border-radius: 6px;
            background: #fbfbfc;
            color: #374151;
            font-size: 13px;
        }
    )");
    root->addWidget(media_list);

    auto refresh_media_list = [&]() {
        media_list->clear();
        for (int i = 0; i < images.size(); ++i) {
            media_list->addItem(QString("图片 %1").arg(i + 1));
        }
        if (media_list->count() == 0) {
            media_list->addItem("未选择图片");
        }
    };
    refresh_media_list();

    QHBoxLayout* actions = new QHBoxLayout;
    QPushButton* add_images = new QPushButton("添加图片", &dialog);
    QPushButton* clear_media = new QPushButton("清空图片", &dialog);
    actions->addWidget(add_images);
    actions->addWidget(clear_media);
    actions->addStretch();
    root->addLayout(actions);

    connect(add_images, &QPushButton::clicked, &dialog, [&]() {
        const QStringList paths = QFileDialog::getOpenFileNames(
            &dialog, "选择图片", QString(), imageFilters().join(";;"));
        if (paths.isEmpty()) return;
        if (images.size() + paths.size() > 9) {
            QMessageBox::warning(&dialog, "图片过多", "朋友圈图片最多九张。");
            return;
        }
        for (const QString& path : paths) {
            const QJsonObject encoded = encodeMomentImageFile(path);
            if (encoded.isEmpty()) {
                QMessageBox::warning(&dialog, "图片处理失败", QFileInfo(path).fileName() + " 无法读取或过大。");
                return;
            }
            images.append(encoded);
        }
        refresh_media_list();
    });

    connect(clear_media, &QPushButton::clicked, &dialog, [&]() {
        images = QJsonArray();
        refresh_media_list();
    });

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("发布");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        const QString content = content_edit->toPlainText().trimmed();
        if (content.isEmpty() && images.isEmpty()) {
            QMessageBox::warning(&dialog, "内容为空", "请填写文字或选择图片。");
            return;
        }
        if (tcp_client_) {
            tcp_client_->createMoment(content, images);
        }
        dialog.accept();
    });

    dialog.exec();
}

void MainWindow::onMomentCreateResult(int code, const QString& message) {
    if (moments_status_label_) {
        moments_status_label_->setText(message);
    }
    if (code == 0) {
        loadMoments();
        return;
    }
    QMessageBox::warning(this, "发布失败", message);
}

void MainWindow::onMomentsReceived(const QString& moments_json) {
    QJsonDocument doc = QJsonDocument::fromJson(moments_json.toUtf8());
    if (!doc.isArray()) {
        if (moments_status_label_) {
            moments_status_label_->setText("加载失败");
        }
        return;
    }
    renderMoments(doc.array());
}

void MainWindow::renderMoments(const QJsonArray& moments) {
    if (!moments_feed_layout_) return;

    clearLayout(moments_feed_layout_);
    if (moments.isEmpty()) {
        QLabel* empty = new QLabel("暂无朋友圈动态", moments_feed_widget_);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("color: #8a8f98; font-size: 15px; padding: 80px;");
        moments_feed_layout_->addWidget(empty);
        moments_feed_layout_->addStretch();
        if (moments_status_label_) {
            moments_status_label_->setText("0 条动态");
        }
        return;
    }

    for (const QJsonValue& value : moments) {
        if (value.isObject()) {
            moments_feed_layout_->addWidget(createMomentCard(value.toObject()));
        }
    }
    moments_feed_layout_->addStretch();
    if (moments_status_label_) {
        moments_status_label_->setText(QString("%1 条动态").arg(moments.size()));
    }
}

QWidget* MainWindow::createMomentCard(const QJsonObject& moment) {
    QWidget* card = new QWidget(moments_feed_widget_);
    card->setObjectName("momentCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(R"(
        QWidget#momentCard {
            background: #ffffff;
            border: 1px solid #e8eaed;
            border-radius: 8px;
        }
    )");

    QHBoxLayout* row = new QHBoxLayout(card);
    row->setContentsMargins(16, 16, 16, 16);
    row->setSpacing(12);

    const QString nickname = moment["nickname"].toString(moment["user_id"].toString());
    const QString avatar_url = moment["avatar_url"].toString();
    QLabel* avatar = new QLabel(card);
    avatar->setFixedSize(44, 44);
    avatar->setPixmap(avatarPixmapFromValue(avatar_url, nickname, 44));
    avatar->setStyleSheet("border: none; background: transparent;");
    row->addWidget(avatar, 0, Qt::AlignTop);

    QVBoxLayout* body = new QVBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(8);
    row->addLayout(body, 1);

    QLabel* name = new QLabel(nickname, card);
    name->setTextFormat(Qt::PlainText);
    name->setStyleSheet("color: #1f2937; font-size: 15px; font-weight: 700; border: none; background: transparent;");
    body->addWidget(name);

    const QString content = moment["content"].toString();
    if (!content.isEmpty()) {
        QLabel* text = new QLabel(content, card);
        text->setTextFormat(Qt::PlainText);
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        text->setStyleSheet("color: #111827; font-size: 14px; line-height: 150%; border: none; background: transparent;");
        body->addWidget(text);
    }

    const QString media_type = moment["media_type"].toString();
    const QJsonValue media_value = moment["media"];
    if (media_type == "image" && media_value.isArray()) {
        QGridLayout* grid = new QGridLayout;
        grid->setContentsMargins(0, 2, 0, 0);
        grid->setSpacing(6);
        const QJsonArray images = media_value.toArray();
        for (int i = 0; i < images.size(); ++i) {
            const QJsonValue image_value = images.at(i);
            QString thumb_url;
            QString full_url;
            if (image_value.isObject()) {
                const QJsonObject image_obj = image_value.toObject();
                thumb_url = image_obj["thumb_url"].toString();
                full_url = image_obj["image_url"].toString();
            } else {
                thumb_url = image_value.toString();
                full_url = thumb_url;
            }

            QToolButton* image = new QToolButton(card);
            image->setFixedSize(112, 112);
            image->setCursor(Qt::PointingHandCursor);
            image->setToolTip("查看图片");
            image->setIconSize(QSize(112, 112));
            image->setStyleSheet(R"(
                QToolButton {
                    background: #eef0f2;
                    border: none;
                    border-radius: 4px;
                    padding: 0;
                }
                QToolButton:hover {
                    background: #e5e7eb;
                }
            )");
            const QPixmap pixmap = imagePixmapFromDataUrl(thumb_url, QSize(112, 112));
            if (!pixmap.isNull()) {
                image->setIcon(QIcon(pixmap.copy((pixmap.width() - 112) / 2, (pixmap.height() - 112) / 2, 112, 112)));
            } else {
                image->setText("图片");
            }
            connect(image, &QToolButton::clicked, this, [this, full_url]() {
                showMomentImageDialog(full_url);
            });
            grid->addWidget(image, i / 3, i % 3);
        }
        body->addLayout(grid);
    }

    QLabel* time = new QLabel(moment["create_time"].toString(), card);
    time->setStyleSheet("color: #8a8f98; font-size: 12px; border: none; background: transparent;");
    body->addWidget(time);

    return card;
}

QJsonObject MainWindow::encodeMomentImageFile(const QString& file_path) {
    QImageReader reader(file_path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        return QJsonObject();
    }

    const int max_edge = 1280;
    if (image.width() > max_edge || image.height() > max_edge) {
        image = image.scaled(max_edge, max_edge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPG", 78)) {
        return QJsonObject();
    }

    if (bytes.size() > 850 * 1024) {
        bytes.clear();
        buffer.close();
        buffer.setBuffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        QImage smaller = image.scaled(960, 960, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (!smaller.save(&buffer, "JPG", 65) || bytes.size() > 850 * 1024) {
            return QJsonObject();
        }
    }

    QImage thumb = image.scaled(240, 240, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QRect crop((thumb.width() - 240) / 2, (thumb.height() - 240) / 2, 240, 240);
    thumb = thumb.copy(crop);

    QByteArray thumb_bytes;
    QBuffer thumb_buffer(&thumb_bytes);
    thumb_buffer.open(QIODevice::WriteOnly);
    if (!thumb.save(&thumb_buffer, "JPG", 58)) {
        return QJsonObject();
    }

    QJsonObject result;
    result["thumb_url"] = "data:image/jpeg;base64," + QString::fromLatin1(thumb_bytes.toBase64());
    result["image_url"] = "data:image/jpeg;base64," + QString::fromLatin1(bytes.toBase64());
    return result;
}

void MainWindow::showMomentImageDialog(const QString& image_url) {
    const QPixmap pixmap = imagePixmapFromDataUrl(image_url, QSize(900, 700), Qt::KeepAspectRatio);
    if (pixmap.isNull()) {
        QMessageBox::warning(this, "查看图片", "图片无法打开");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("查看图片");
    dialog.resize(qMin(960, pixmap.width() + 40), qMin(760, pixmap.height() + 40));

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);

    QScrollArea* area = new QScrollArea(&dialog);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    QLabel* image = new QLabel(area);
    image->setAlignment(Qt::AlignCenter);
    image->setPixmap(pixmap);
    image->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    area->setWidget(image);
    layout->addWidget(area);

    dialog.exec();
}
