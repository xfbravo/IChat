/**
 * @file mainwindow_helpers.h
 * @brief MainWindow 页面拆分后共享的轻量 UI 工具
 */

#pragma once

#include <QByteArray>
#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>

namespace mainwindow_detail {

// 服务端不同接口可能返回完整时间、ISO 时间或仅时分秒，这里统一转为毫秒时间戳。
inline qint64 timestampFromText(const QString& time_text) {
    if (time_text.isEmpty()) {
        return 0;
    }

    const QStringList formats = {
        "yyyy-MM-dd HH:mm:ss",
        "yyyy-MM-dd hh:mm:ss",
        "yyyy-MM-ddTHH:mm:ss",
        "yyyy-MM-ddThh:mm:ss",
        "hh:mm:ss",
        "HH:mm:ss"
    };

    for (const QString& format : formats) {
        QDateTime dt = QDateTime::fromString(time_text, format);
        if (!dt.isValid()) continue;

        if (format == "hh:mm:ss" || format == "HH:mm:ss") {
            dt.setDate(QDate::currentDate());
        }
        return dt.toMSecsSinceEpoch();
    }

    QDateTime dt = QDateTime::fromString(time_text, Qt::ISODate);
    if (dt.isValid()) {
        return dt.toMSecsSinceEpoch();
    }

    return 0;
}

class MessageBubble : public QWidget {
public:
    MessageBubble(const QString& text, bool is_mine, int max_text_width, QWidget* parent = nullptr)
        : QWidget(parent)
        , is_mine_(is_mine)
        , background_(is_mine ? QColor("#d8f0d4") : QColor("#ffffff"))
        , border_(is_mine ? QColor("#b9dfb8") : QColor("#dfe7e1"))
    {
        setAttribute(Qt::WA_TranslucentBackground);

        QLabel* label = new QLabel(text, this);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setMaximumWidth(max_text_width);
        label->setStyleSheet(R"(
            QLabel {
                background: transparent;
                color: #111111;
                font-family: "Microsoft YaHei", sans-serif;
                font-size: 14px;
                line-height: 155%;
            }
        )");

        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setSpacing(0);
        layout->setContentsMargins(is_mine_ ? 12 : 20, 8, is_mine_ ? 20 : 12, 8);
        layout->addWidget(label);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);

        // 用自绘气泡保留微信式小尖角；文本内容仍交给 QLabel 负责换行和选择。
        constexpr int tail_width = 8;
        constexpr int radius = 6;
        QRectF bubble_rect = rect().adjusted(
            is_mine_ ? 0.5 : tail_width + 0.5,
            0.5,
            is_mine_ ? -tail_width - 0.5 : -0.5,
            -0.5);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(border_, 1));
        painter.setBrush(background_);

        QPainterPath path;
        path.addRoundedRect(bubble_rect, radius, radius);

        const qreal tail_top = bubble_rect.top() + 13;
        const qreal tail_mid = bubble_rect.top() + 19;
        const qreal tail_bottom = bubble_rect.top() + 25;
        QPainterPath tail;
        if (is_mine_) {
            tail.moveTo(bubble_rect.right() - 1, tail_top);
            tail.lineTo(width() - 1, tail_mid);
            tail.lineTo(bubble_rect.right() - 1, tail_bottom);
        } else {
            tail.moveTo(bubble_rect.left() + 1, tail_top);
            tail.lineTo(1, tail_mid);
            tail.lineTo(bubble_rect.left() + 1, tail_bottom);
        }
        tail.closeSubpath();
        path = path.united(tail);

        painter.drawPath(path);
    }

private:
    bool is_mine_;
    QColor background_;
    QColor border_;
};

class ConversationListItemWidget : public QWidget {
public:
    ConversationListItemWidget(const QString& title,
                               const QString& last_message,
                               int unread,
                               QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet("background: transparent;");

        title_label_ = new QLabel(title, this);
        title_label_->setTextFormat(Qt::PlainText);
        title_label_->setWordWrap(false);

        last_message_label_ = new QLabel(last_message, this);
        last_message_label_->setTextFormat(Qt::PlainText);
        last_message_label_->setWordWrap(false);

        badge_label_ = new QLabel(unread > 99 ? "99+" : QString::number(unread), this);
        badge_label_->setAlignment(Qt::AlignCenter);
        badge_label_->setFixedSize(unread > 99 ? QSize(30, 22) : QSize(22, 22));

        QVBoxLayout* text_layout = new QVBoxLayout;
        text_layout->setContentsMargins(0, 0, 0, 0);
        text_layout->setSpacing(4);
        text_layout->addWidget(title_label_);
        text_layout->addWidget(last_message_label_);

        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(10);
        layout->addLayout(text_layout, 1);
        layout->addWidget(badge_label_, 0, Qt::AlignRight | Qt::AlignVCenter);

        badge_label_->setVisible(unread > 0);
        setSelected(false);
    }

    void setSelected(bool selected) {
        // QListWidget 的自定义 item widget 不会自动继承选中配色，需要手动同步。
        const QString title_color = selected ? "#ffffff" : "#1d2b22";
        const QString summary_color = selected ? "#e8f4ea" : "#6b756e";
        title_label_->setStyleSheet(QString(
            "QLabel { color: %1; font-size: 14px; font-weight: 700; background: transparent; }"
        ).arg(title_color));
        last_message_label_->setStyleSheet(QString(
            "QLabel { color: %1; font-size: 12px; background: transparent; }"
        ).arg(summary_color));
        badge_label_->setStyleSheet(R"(
            QLabel {
                color: #ffffff;
                background-color: #d94734;
                border-radius: 11px;
                font-size: 11px;
                font-weight: bold;
            }
        )");
    }

private:
    QLabel* title_label_;
    QLabel* last_message_label_;
    QLabel* badge_label_;
};

inline QIcon lineIcon(const QString& type, const QColor& color) {
    // 图标用 QPainter 生成，避免额外资源文件；新增页面时在这里补图标类型。
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (type == "message") {
        painter.drawRoundedRect(QRectF(6, 7, 20, 15), 5, 5);
        QPainterPath tail;
        tail.moveTo(12, 22);
        tail.lineTo(9, 27);
        tail.lineTo(17, 22);
        painter.drawPath(tail);
    } else if (type == "contacts") {
        painter.drawEllipse(QRectF(11, 5, 10, 10));
        painter.drawArc(QRectF(7, 15, 18, 14), 20 * 16, 140 * 16);
        painter.drawEllipse(QRectF(4, 10, 7, 7));
        painter.drawEllipse(QRectF(21, 10, 7, 7));
    } else if (type == "moments") {
        painter.drawEllipse(QRectF(7, 7, 18, 18));
        painter.drawEllipse(QRectF(13, 3, 6, 6));
        painter.drawEllipse(QRectF(23, 16, 6, 6));
        painter.drawEllipse(QRectF(5, 21, 6, 6));
    } else if (type == "me") {
        painter.drawEllipse(QRectF(11, 6, 10, 10));
        painter.drawArc(QRectF(7, 17, 18, 13), 20 * 16, 140 * 16);
        painter.drawRoundedRect(QRectF(5, 4, 22, 24), 8, 8);
    } else if (type == "favorite") {
        QPainterPath bookmark;
        bookmark.moveTo(9, 6);
        bookmark.lineTo(23, 6);
        bookmark.lineTo(23, 26);
        bookmark.lineTo(16, 21);
        bookmark.lineTo(9, 26);
        bookmark.closeSubpath();
        painter.drawPath(bookmark);
    } else if (type == "account") {
        painter.drawEllipse(QRectF(11, 11, 10, 10));
        for (int i = 0; i < 8; ++i) {
            painter.save();
            painter.translate(16, 16);
            painter.rotate(i * 45);
            painter.drawLine(QPointF(0, -13), QPointF(0, -10));
            painter.restore();
        }
        painter.drawEllipse(QRectF(5, 5, 22, 22));
    } else if (type == "password") {
        painter.drawRoundedRect(QRectF(8, 14, 16, 12), 3, 3);
        painter.drawArc(QRectF(10, 6, 12, 13), 0, 180 * 16);
    } else if (type == "logout") {
        painter.drawRoundedRect(QRectF(6, 7, 13, 18), 3, 3);
        painter.drawLine(QPointF(15, 16), QPointF(27, 16));
        painter.drawLine(QPointF(23, 12), QPointF(27, 16));
        painter.drawLine(QPointF(23, 20), QPointF(27, 16));
    }

    return QIcon(pixmap);
}

inline QIcon navIcon(const QString& type) {
    return lineIcon(type, QColor("#ecf0f1"));
}

inline QPixmap circularAvatarPixmap(const QPixmap& source, int size) {
    if (source.isNull()) {
        return QPixmap();
    }

    // 保持头像铺满圆形区域，多余部分从中心裁剪。
    QPixmap scaled = source.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QRect crop_rect((scaled.width() - size) / 2, (scaled.height() - size) / 2, size, size);
    QPixmap cropped = scaled.copy(crop_rect);

    QPixmap result(size, size);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(QRectF(0.5, 0.5, size - 1, size - 1));
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, cropped);
    painter.setClipping(false);
    painter.setPen(QPen(QColor("#d8dee4"), 1));
    painter.drawEllipse(QRectF(0.5, 0.5, size - 1, size - 1));

    return result;
}

inline QPixmap defaultAvatarPixmap(const QString& display_name, int size) {
    const QString trimmed_name = display_name.trimmed();
    // 没有头像时使用昵称首字母，保证联系人和“我”页始终有稳定头像。
    const QString initial = trimmed_name.isEmpty() ? "I" : trimmed_name.left(1).toUpper();

    QPixmap result(size, size);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#2f6f3e"));
    painter.drawEllipse(QRectF(0.5, 0.5, size - 1, size - 1));

    QFont font = painter.font();
    font.setFamily("Microsoft YaHei");
    font.setPixelSize(qMax(18, size / 3));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, initial);

    return result;
}

inline QPixmap avatarPixmapFromValue(const QString& avatar_url,
                                     const QString& display_name,
                                     int size) {
    QPixmap source;
    const QString value = avatar_url.trimmed();

    // 当前头像既可能是服务端保存的 data URL，也可能是本地路径。
    if (value.startsWith("data:image/", Qt::CaseInsensitive)) {
        const int comma_index = value.indexOf(',');
        if (comma_index > 0) {
            const QByteArray data = QByteArray::fromBase64(value.mid(comma_index + 1).toLatin1());
            source.loadFromData(data);
        }
    } else if (!value.isEmpty()) {
        source.load(value);
    }

    if (source.isNull()) {
        return defaultAvatarPixmap(display_name, size);
    }
    return circularAvatarPixmap(source, size);
}

} // namespace mainwindow_detail
