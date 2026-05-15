/**
 * @file mainwindow_messages.cpp
 * @brief MainWindow 消息列表、聊天面板和会话渲染
 */

#include "mainwindow.h"
#include "addcontactdialog.h"
#include <QStatusBar>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
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
#include <QApplication>
#include <QListView>
#include <QAbstractItemView>
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
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <algorithm>
#include <functional>
#include "mainwindow_helpers.h"

using namespace mainwindow_detail;

namespace {

QPixmap pixmapFromDataUrl(const QString& data_url, const QSize& target_size) {
    const int comma = data_url.indexOf(',');
    const QByteArray payload = comma >= 0
        ? data_url.mid(comma + 1).toLatin1()
        : data_url.toLatin1();
    QImage image;
    if (!image.loadFromData(QByteArray::fromBase64(payload))) {
        return QPixmap();
    }
    return QPixmap::fromImage(image.scaled(target_size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QJsonObject messageContentObject(const QString& content) {
    const QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
    return doc.isObject() ? doc.object() : QJsonObject();
}

int fieldMatchScore(const QString& query, const QString& field) {
    const QString normalized_query = query.trimmed().toCaseFolded();
    const QString normalized_field = field.trimmed().toCaseFolded();
    if (normalized_query.isEmpty() || normalized_field.isEmpty()) {
        return 0;
    }
    if (normalized_field == normalized_query) {
        return 300;
    }
    if (normalized_field.startsWith(normalized_query)) {
        return 200;
    }
    if (normalized_field.contains(normalized_query)) {
        return 100;
    }
    return 0;
}

QIcon createMenuActionIcon(const QString& type) {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#2f6f3e"), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (type == "add_friend") {
        painter.drawEllipse(QRectF(7, 6, 10, 10));
        painter.drawArc(QRectF(4, 17, 16, 11), 15 * 16, 150 * 16);
        painter.drawLine(QPointF(23, 9), QPointF(23, 21));
        painter.drawLine(QPointF(17, 15), QPointF(29, 15));
    } else if (type == "group") {
        painter.drawEllipse(QRectF(11, 5, 10, 10));
        painter.drawEllipse(QRectF(4, 10, 8, 8));
        painter.drawEllipse(QRectF(20, 10, 8, 8));
        painter.drawArc(QRectF(7, 18, 18, 10), 10 * 16, 160 * 16);
    }

    return QIcon(pixmap);
}

QPixmap selectionBoxPixmap(bool checked, int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF box(1.5, 1.5, size - 3, size - 3);
    painter.setPen(QPen(checked ? QColor("#4CAF50") : QColor("#b8c4bc"), 1.6));
    painter.setBrush(checked ? QColor("#4CAF50") : QColor("#ffffff"));
    painter.drawRoundedRect(box, 4, 4);

    if (checked) {
        painter.setPen(QPen(Qt::white, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath tick;
        tick.moveTo(size * 0.28, size * 0.52);
        tick.lineTo(size * 0.44, size * 0.68);
        tick.lineTo(size * 0.74, size * 0.34);
        painter.drawPath(tick);
    }

    return pixmap;
}

class SearchResultItemWidget : public QWidget {
public:
    SearchResultItemWidget(const QString& title,
                           const QString& subtitle,
                           const QString& avatar_value,
                           QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet("background: transparent;");

        constexpr int avatar_size = 36;
        QLabel* avatar_label = new QLabel(this);
        avatar_label->setFixedSize(avatar_size, avatar_size);
        avatar_label->setPixmap(avatarPixmapFromValue(avatar_value, title, avatar_size));

        QLabel* title_label = new QLabel(title, this);
        title_label->setTextFormat(Qt::PlainText);
        title_label->setWordWrap(false);
        title_label->setStyleSheet(R"(
            QLabel {
                color: #111111;
                font-size: 14px;
                font-weight: 600;
                background: transparent;
            }
        )");

        QLabel* subtitle_label = new QLabel(subtitle, this);
        subtitle_label->setTextFormat(Qt::PlainText);
        subtitle_label->setWordWrap(false);
        subtitle_label->setStyleSheet(R"(
            QLabel {
                color: #777777;
                font-size: 12px;
                background: transparent;
            }
        )");

        QVBoxLayout* text_layout = new QVBoxLayout;
        text_layout->setContentsMargins(0, 0, 0, 0);
        text_layout->setSpacing(3);
        text_layout->addWidget(title_label);
        text_layout->addWidget(subtitle_label);

        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 7, 10, 7);
        layout->setSpacing(10);
        layout->addWidget(avatar_label, 0, Qt::AlignVCenter);
        layout->addLayout(text_layout, 1);
    }
};

class SelectableContactItemWidget : public QWidget {
public:
    SelectableContactItemWidget(const QString& title,
                                const QString& subtitle,
                                const QString& avatar_value,
                                std::function<void(bool)> toggled_handler,
                                QWidget* parent = nullptr)
        : QWidget(parent)
        , toggled_handler_(std::move(toggled_handler))
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setCursor(Qt::PointingHandCursor);
        setStyleSheet(R"(
            QWidget {
                background: transparent;
            }
            QWidget:hover {
                background-color: #f6fbf6;
            }
        )");

        check_indicator_ = new QLabel(this);
        check_indicator_->setFixedSize(22, 22);
        check_indicator_->setPixmap(selectionBoxPixmap(false, 22));
        check_indicator_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        constexpr int avatar_size = 38;
        QLabel* avatar_label = new QLabel(this);
        avatar_label->setFixedSize(avatar_size, avatar_size);
        avatar_label->setPixmap(avatarPixmapFromValue(avatar_value, title, avatar_size));
        avatar_label->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        QLabel* title_label = new QLabel(title, this);
        title_label->setTextFormat(Qt::PlainText);
        title_label->setWordWrap(false);
        title_label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        title_label->setStyleSheet(R"(
            QLabel {
                color: #111111;
                font-size: 14px;
                font-weight: 600;
                background: transparent;
            }
        )");

        QLabel* subtitle_label = new QLabel(subtitle, this);
        subtitle_label->setTextFormat(Qt::PlainText);
        subtitle_label->setWordWrap(false);
        subtitle_label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        subtitle_label->setStyleSheet(R"(
            QLabel {
                color: #777777;
                font-size: 12px;
                background: transparent;
            }
        )");

        QVBoxLayout* text_layout = new QVBoxLayout;
        text_layout->setContentsMargins(0, 0, 0, 0);
        text_layout->setSpacing(3);
        text_layout->addWidget(title_label);
        text_layout->addWidget(subtitle_label);

        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(10);
        layout->addWidget(check_indicator_, 0, Qt::AlignVCenter);
        layout->addWidget(avatar_label, 0, Qt::AlignVCenter);
        layout->addLayout(text_layout, 1);
    }

    bool isChecked() const {
        return checked_;
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        checked_ = !checked_;
        if (check_indicator_) {
            check_indicator_->setPixmap(selectionBoxPixmap(checked_, 22));
        }
        if (toggled_handler_) {
            toggled_handler_(checked_);
        }
        event->accept();
    }

private:
    QLabel* check_indicator_ = nullptr;
    bool checked_ = false;
    std::function<void(bool)> toggled_handler_;
};

} // namespace

void MainWindow::createMessageView() {
    message_view_ = new QWidget;

    QHBoxLayout* main_layout = new QHBoxLayout(message_view_);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // 聊天列表 (左侧)
    conversation_panel_ = new QWidget;
    conversation_panel_->setStyleSheet("QWidget { background-color: #f5f5f5; }");
    QVBoxLayout* conversation_layout = new QVBoxLayout(conversation_panel_);
    conversation_layout->setContentsMargins(0, 0, 0, 0);
    conversation_layout->setSpacing(0);

    QWidget* conversation_search_bar = new QWidget(conversation_panel_);
    conversation_search_bar->setFixedHeight(54);
    conversation_search_bar->setStyleSheet(R"(
        QWidget {
            background-color: #f5f5f5;
            border-bottom: 1px solid #e0e0e0;
        }
    )");
    QHBoxLayout* conversation_search_layout = new QHBoxLayout(conversation_search_bar);
    conversation_search_layout->setContentsMargins(10, 8, 10, 8);
    conversation_search_layout->setSpacing(8);

    conversation_search_edit_ = new QLineEdit(conversation_search_bar);
    conversation_search_edit_->setPlaceholderText("搜索对话");
    conversation_search_edit_->setClearButtonEnabled(true);
    conversation_search_edit_->setStyleSheet(R"(
        QLineEdit {
            min-height: 34px;
            padding: 0 10px;
            border: 1px solid #dddddd;
            border-radius: 4px;
            background-color: #ffffff;
            color: #111111;
            font-size: 14px;
        }
        QLineEdit:focus {
            border: 1px solid #4CAF50;
        }
    )");
    conversation_search_edit_->installEventFilter(this);
    connect(conversation_search_edit_, &QLineEdit::textChanged,
            this, &MainWindow::onConversationSearchTextChanged);
    conversation_search_layout->addWidget(conversation_search_edit_, 1);

    QToolButton* conversation_add_button = new QToolButton(conversation_search_bar);
    conversation_add_button->setText("+");
    conversation_add_button->setToolTip("新建");
    conversation_add_button->setCursor(Qt::PointingHandCursor);
    conversation_add_button->setFixedSize(34, 34);
    conversation_add_button->setStyleSheet(R"(
        QToolButton {
            border: 1px solid #dddddd;
            border-radius: 4px;
            background-color: #ffffff;
            color: #333333;
            font-size: 22px;
            padding-bottom: 3px;
        }
        QToolButton:hover {
            background-color: #eeeeee;
        }
        QToolButton::menu-indicator {
            image: none;
            width: 0;
        }
    )");
    conversation_add_button->setPopupMode(QToolButton::InstantPopup);
    QMenu* create_menu = new QMenu(conversation_add_button);
    create_menu->setStyleSheet(R"(
        QMenu {
            background-color: #ffffff;
            border: 1px solid #d9e2dc;
            border-radius: 6px;
            padding: 8px 0;
        }
        QMenu::item {
            min-width: 132px;
            min-height: 30px;
            padding: 8px 28px 8px 12px;
            margin: 3px 6px;
            color: #111111;
            font-size: 14px;
            border-radius: 5px;
        }
        QMenu::item:selected {
            background-color: #e8f5e9;
            color: #111111;
        }
        QMenu::icon {
            padding-left: 6px;
            padding-right: 10px;
        }
    )");
    QAction* add_friend_action = create_menu->addAction("添加好友");
    QAction* create_group_action = create_menu->addAction("发起群聊");
    add_friend_action->setIcon(createMenuActionIcon("add_friend"));
    create_group_action->setIcon(createMenuActionIcon("group"));
    add_friend_action->setIconVisibleInMenu(true);
    create_group_action->setIconVisibleInMenu(true);
    connect(add_friend_action, &QAction::triggered, this, &MainWindow::onAddContactClicked);
    connect(create_group_action, &QAction::triggered, this, &MainWindow::openCreateGroupDialog);
    conversation_add_button->setMenu(create_menu);
    conversation_search_layout->addWidget(conversation_add_button);
    conversation_layout->addWidget(conversation_search_bar);

    chat_list_widget_ = new QListWidget;
    chat_list_widget_->setFocusPolicy(Qt::NoFocus);
    chat_list_widget_->setStyleSheet(R"(
        QListWidget {
            border: none;
            background-color: #f5f5f5;
            color: #111111;
            outline: none;
        }
        QListWidget::item {
            padding: 0;
            border-bottom: 1px solid #e0e0e0;
            color: #111111;
        }
        QListWidget::item:selected {
            background-color: #4CAF50;
            color: #ffffff;
            border: none;
        }
        QListWidget::item:selected:active,
        QListWidget::item:selected:!active {
            background-color: #4CAF50;
            color: #ffffff;
            border: none;
        }
    )");
    connect(chat_list_widget_, &QListWidget::itemClicked,
            this, &MainWindow::onChatItemClicked);
    connect(chat_list_widget_, &QListWidget::itemSelectionChanged,
            this, &MainWindow::refreshConversationSelectionStyles);
    conversation_layout->addWidget(chat_list_widget_, 1);

    conversation_search_results_ = new QListWidget(message_view_);
    conversation_search_results_->setFocusPolicy(Qt::NoFocus);
    conversation_search_results_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    conversation_search_results_->setSelectionMode(QAbstractItemView::SingleSelection);
    conversation_search_results_->installEventFilter(this);
    conversation_search_results_->hide();
    conversation_search_results_->setStyleSheet(R"(
        QListWidget {
            border: 1px solid #dcdcdc;
            background-color: #ffffff;
            color: #111111;
            outline: none;
        }
        QListWidget::item {
            padding: 8px 10px;
            border-bottom: 1px solid #eeeeee;
        }
        QListWidget::item:hover,
        QListWidget::item:selected {
            background-color: #e8f5e9;
            color: #111111;
        }
        QListWidget::item:disabled {
            color: #999999;
            background-color: #ffffff;
        }
    )");
    connect(conversation_search_results_, &QListWidget::itemClicked,
            this, &MainWindow::onSearchResultClicked);

    // 聊天界面面板 (右侧)
    chat_interface_panel_ = new QWidget;
    QVBoxLayout* chat_layout = new QVBoxLayout(chat_interface_panel_);
    chat_layout->setContentsMargins(0, 0, 0, 0);

    // 聊天标题栏
    QWidget* chat_header = new QWidget;
    chat_header->setFixedHeight(54);
    chat_header->setStyleSheet(R"(
        QWidget {
            background-color: #fafafa;
            border-bottom: 1px solid #e0e0e0;
        }
    )");
    QHBoxLayout* chat_header_layout = new QHBoxLayout(chat_header);
    chat_header_layout->setContentsMargins(15, 0, 12, 0);
    chat_header_layout->setSpacing(8);

    chat_target_label_ = new QLabel("选择联系人开始聊天");
    chat_target_label_->setStyleSheet(R"(
        QLabel {
            background-color: transparent;
            border: none;
            font-size: 16px;
            font-weight: bold;
            color: #111111;
        }
    )");
    chat_header_layout->addWidget(chat_target_label_, 1);

    chat_more_button_ = new QToolButton;
    chat_more_button_->setText("⋯");
    chat_more_button_->setToolTip("更多");
    chat_more_button_->setPopupMode(QToolButton::InstantPopup);
    chat_more_button_->setEnabled(false);
    chat_more_button_->setStyleSheet(R"(
        QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 4px;
            color: #333333;
            font-size: 22px;
            font-weight: bold;
            min-width: 32px;
            min-height: 32px;
            padding-bottom: 4px;
        }
        QToolButton:hover {
            background-color: #eeeeee;
        }
        QToolButton:disabled {
            color: #bbbbbb;
        }
        QToolButton::menu-indicator {
            image: none;
            width: 0;
        }
    )");
    QMenu* chat_menu = new QMenu(chat_more_button_);
    chat_menu->setStyleSheet(R"(
        QMenu {
            background-color: #ffffff;
            border: 1px solid #dddddd;
            padding: 6px 0;
        }
        QMenu::item {
            min-height: 32px;
            padding: 8px 28px 8px 18px;
            color: #111111;
            font-size: 14px;
        }
        QMenu::item:selected {
            background-color: #f0f0f0;
        }
    )");
    QAction* edit_remark_action = chat_menu->addAction("修改联系人备注");
    connect(edit_remark_action, &QAction::triggered, this, &MainWindow::onEditContactRemark);
    chat_more_button_->setMenu(chat_menu);
    chat_header_layout->addWidget(chat_more_button_);

    chat_layout->addWidget(chat_header);

    // 聊天显示区
    chat_scroll_area_ = new QScrollArea;
    chat_scroll_area_->setWidgetResizable(true);
    chat_scroll_area_->setFrameShape(QFrame::NoFrame);
    chat_scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chat_scroll_area_->setStyleSheet(R"(
        QScrollArea {
            background-color: #fafafa;
            border: none;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 4px 0;
        }
        QScrollBar::handle:vertical {
            background: #d6d6d6;
            border-radius: 4px;
            min-height: 32px;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }
    )");

    chat_messages_widget_ = new QWidget;
    chat_messages_widget_->setStyleSheet("background-color: #fafafa;");
    chat_messages_layout_ = new QVBoxLayout(chat_messages_widget_);
    chat_messages_layout_->setContentsMargins(16, 12, 16, 12);
    chat_messages_layout_->setSpacing(2);
    chat_messages_layout_->addStretch();
    chat_scroll_area_->setWidget(chat_messages_widget_);
    chat_layout->addWidget(chat_scroll_area_);

    // 输入区
    QWidget* input_widget = new QWidget;
    QHBoxLayout* input_layout = new QHBoxLayout(input_widget);
    input_layout->setContentsMargins(10, 10, 10, 10);
    input_layout->setSpacing(10);

    message_input_ = new QLineEdit;
    message_input_->setPlaceholderText("输入消息...");
    message_input_->setStyleSheet(R"(
        QLineEdit {
            padding: 10px 15px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 14px;
        }
        QLineEdit:focus {
            border: 1px solid #4CAF50;
        }
    )");
    connect(message_input_, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);

    attach_file_button_ = new QToolButton;
    attach_file_button_->setText("+");
    attach_file_button_->setToolTip("发送文件");
    attach_file_button_->setEnabled(false);
    attach_file_button_->setCursor(Qt::PointingHandCursor);
    attach_file_button_->setStyleSheet(R"(
        QToolButton {
            min-width: 38px;
            min-height: 38px;
            border: 1px solid #dddddd;
            border-radius: 4px;
            background-color: #ffffff;
            color: #333333;
            font-size: 24px;
            padding-bottom: 3px;
        }
        QToolButton:hover {
            background-color: #f2f2f2;
        }
        QToolButton:disabled {
            color: #bbbbbb;
            background-color: #f7f7f7;
        }
    )");
    connect(attach_file_button_, &QToolButton::clicked, this, &MainWindow::onAttachFileClicked);

    send_button_ = new QPushButton("发送");
    send_button_->setStyleSheet(R"(
        QPushButton {
            padding: 10px 25px;
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
        QPushButton:disabled {
            background-color: #cccccc;
        }
    )");
    connect(send_button_, &QPushButton::clicked, this, &MainWindow::onSendClicked);

    input_layout->addWidget(message_input_);
    input_layout->addWidget(attach_file_button_);
    input_layout->addWidget(send_button_);

    chat_layout->addWidget(input_widget);

    // 使用分割器
    message_splitter_ = new QSplitter(Qt::Horizontal);
    message_splitter_->addWidget(conversation_panel_);
    message_splitter_->addWidget(chat_interface_panel_);
    message_splitter_->setStretchFactor(0, 1);
    message_splitter_->setStretchFactor(1, 3);
    message_splitter_->setHandleWidth(1);

    main_layout->addWidget(message_splitter_);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    const bool is_search_widget =
        watched == conversation_search_edit_ ||
        watched == conversation_search_results_ ||
        watched == contact_search_edit_ ||
        watched == contact_search_results_;

    if (is_search_widget && event->type() == QEvent::FocusOut) {
        QTimer::singleShot(0, this, [this]() {
            QWidget* focused = QApplication::focusWidget();
            const bool focus_in_conversation_search =
                focused == conversation_search_edit_ || focused == conversation_search_results_;
            const bool focus_in_contact_search =
                focused == contact_search_edit_ || focused == contact_search_results_;
            if (!focus_in_conversation_search && !focus_in_contact_search) {
                hideSearchResults();
            }
        });
    }

    if ((watched == conversation_search_edit_ || watched == contact_search_edit_) &&
        event->type() == QEvent::KeyPress) {
        QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
        if (key_event->key() == Qt::Key_Escape) {
            hideSearchResults();
            return true;
        }
    }

    if (watched == conversation_search_edit_ && event->type() == QEvent::Resize &&
        conversation_search_results_ && conversation_search_results_->isVisible()) {
        positionSearchResults(conversation_search_edit_, conversation_search_results_);
    }
    if (watched == contact_search_edit_ && event->type() == QEvent::Resize &&
        contact_search_results_ && contact_search_results_->isVisible()) {
        positionSearchResults(contact_search_edit_, contact_search_results_);
    }

    return QMainWindow::eventFilter(watched, event);
}

QList<QString> MainWindow::sortedConversationIds() const {
    QList<QString> peer_ids = conversations_.keys();
    // 会话列表优先按最近消息倒序，其次按标题排序，搜索定位也复用这份原始顺序。
    std::stable_sort(peer_ids.begin(), peer_ids.end(),
                     [this](const QString& left, const QString& right) {
                         const ConversationState& left_conversation = conversations_.constFind(left).value();
                         const ConversationState& right_conversation = conversations_.constFind(right).value();
                         if (left_conversation.last_timestamp != right_conversation.last_timestamp) {
                             return left_conversation.last_timestamp > right_conversation.last_timestamp;
                         }
                         return conversationTitle(left).localeAwareCompare(conversationTitle(right)) < 0;
                     });
    return peer_ids;
}

QList<QString> MainWindow::sortedContactIds() const {
    QList<QString> contact_ids;

    auto append_contact = [this, &contact_ids](const QString& raw_id) {
        const QString friend_id = raw_id.trimmed();
        if (friend_id.isEmpty() || friend_id == user_id_ || contact_ids.contains(friend_id)) {
            return;
        }
        contact_ids.append(friend_id);
    };

    for (auto it = contact_nicknames_.constBegin(); it != contact_nicknames_.constEnd(); ++it) {
        append_contact(it.key());
    }
    for (auto it = contact_remarks_.constBegin(); it != contact_remarks_.constEnd(); ++it) {
        append_contact(it.key());
    }
    for (auto it = contact_avatars_.constBegin(); it != contact_avatars_.constEnd(); ++it) {
        append_contact(it.key());
    }
    for (auto it = conversations_.constBegin(); it != conversations_.constEnd(); ++it) {
        if (!isGroupConversation(it.key())) {
            append_contact(conversationPeerId(it.key()));
        }
    }

    std::stable_sort(contact_ids.begin(), contact_ids.end(),
                     [this](const QString& left, const QString& right) {
                         const QString left_name = contactDisplayName(left).toCaseFolded();
                         const QString right_name = contactDisplayName(right).toCaseFolded();
                         const int name_compare = left_name.localeAwareCompare(right_name);
                         if (name_compare != 0) {
                             return name_compare < 0;
                         }
                         return left.localeAwareCompare(right) < 0;
                     });
    return contact_ids;
}

QString MainWindow::contactDisplayName(const QString& user_id) const {
    const QString remark = contact_remarks_.value(user_id).trimmed();
    if (!remark.isEmpty()) {
        return remark;
    }

    const QString nickname = contact_nicknames_.value(user_id).trimmed();
    if (!nickname.isEmpty()) {
        return nickname;
    }

    const QString key = conversationKey("p2p", user_id);
    auto it = conversations_.constFind(key);
    if (it != conversations_.constEnd() && !it->title.trimmed().isEmpty()) {
        return it->title.trimmed();
    }
    return user_id;
}

QString MainWindow::contactSubtitle(const QString& user_id) const {
    const QString remark = contact_remarks_.value(user_id).trimmed();
    const QString nickname = contact_nicknames_.value(user_id).trimmed();
    if (!remark.isEmpty() && !nickname.isEmpty() && nickname != remark) {
        return QString("昵称: %1").arg(nickname);
    }
    return QString("账号: %1").arg(user_id);
}

QString MainWindow::conversationKey(const QString& chat_type, const QString& peer_id) const {
    const QString normalized_type = chat_type == "group" ? QStringLiteral("group") : QStringLiteral("p2p");
    return QString("%1:%2").arg(normalized_type, peer_id);
}

QString MainWindow::conversationPeerId(const QString& conversation_key) const {
    const int colon = conversation_key.indexOf(':');
    return colon >= 0 ? conversation_key.mid(colon + 1) : conversation_key;
}

QString MainWindow::conversationChatType(const QString& conversation_key) const {
    return conversation_key.startsWith("group:") ? QStringLiteral("group") : QStringLiteral("p2p");
}

bool MainWindow::isGroupConversation(const QString& conversation_key) const {
    return conversationChatType(conversation_key) == "group";
}

void MainWindow::populateSearchResults(QListWidget* results_widget,
                                       const QString& query,
                                       bool keep_conversation_order) {
    if (!results_widget) {
        return;
    }

    results_widget->clear();
    const QString keyword = query.trimmed();
    if (keyword.isEmpty()) {
        results_widget->hide();
        return;
    }

    QList<QString> user_ids = keep_conversation_order ? sortedConversationIds() : QList<QString>();
    if (!keep_conversation_order) {
        const QList<QHash<QString, QString>> contact_maps = {
            contact_remarks_,
            contact_nicknames_,
            contact_avatars_
        };
        for (const QHash<QString, QString>& map : contact_maps) {
            for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
                if (!it.key().isEmpty() && !user_ids.contains(it.key())) {
                    user_ids.append(it.key());
                }
            }
        }
        for (auto it = user_profile_cache_.constBegin(); it != user_profile_cache_.constEnd(); ++it) {
            if (!it.key().isEmpty() && it.key() != user_id_ && !user_ids.contains(it.key())) {
                user_ids.append(it.key());
            }
        }
    }

    struct SearchCandidate {
        QString conversation_key;
        QString peer_id;
        QString chat_type;
        QString display_name;
        QString subtitle;
        QString avatar_value;
        int score = 0;
        int order = 0;
    };

    QList<SearchCandidate> candidates;
    for (int i = 0; i < user_ids.size(); ++i) {
        const QString candidate_key = user_ids.at(i);
        const QString chat_type = keep_conversation_order ? conversationChatType(candidate_key) : QStringLiteral("p2p");
        const QString target_id = keep_conversation_order ? conversationPeerId(candidate_key) : candidate_key;
        if (target_id.isEmpty() || target_id == user_id_) {
            continue;
        }

        const QString display_name = chat_type == "group"
            ? conversationTitle(candidate_key)
            : contactDisplayName(target_id);
        const QString remark = chat_type == "group" ? QString() : contact_remarks_.value(target_id);
        const QString nickname = chat_type == "group" ? QString() : contact_nicknames_.value(target_id);
        const QString title = chat_type == "group" ? group_names_.value(target_id) : conversationTitle(candidate_key);
        int score = 0;
        for (const QString& field : {display_name, remark, nickname, title, target_id}) {
            score = qMax(score, fieldMatchScore(keyword, field));
        }
        if (score <= 0) {
            continue;
        }

        SearchCandidate candidate;
        candidate.conversation_key = chat_type == "group" ? conversationKey("group", target_id) : conversationKey("p2p", target_id);
        candidate.peer_id = target_id;
        candidate.chat_type = chat_type;
        candidate.display_name = display_name;
        candidate.subtitle = chat_type == "group"
            ? QString("群聊 · %1人").arg(group_member_counts_.value(target_id, 0))
            : contactSubtitle(target_id);
        candidate.avatar_value = chat_type == "group"
            ? group_avatars_.value(target_id)
            : contact_avatars_.value(target_id);
        candidate.score = score;
        candidate.order = i;
        candidates.append(candidate);
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [keep_conversation_order](const SearchCandidate& left,
                                               const SearchCandidate& right) {
                         if (left.score != right.score) {
                             return left.score > right.score;
                         }
                         if (keep_conversation_order) {
                             return left.order < right.order;
                         }
                         const int name_compare = left.display_name.localeAwareCompare(right.display_name);
                         if (name_compare != 0) {
                             return name_compare < 0;
                         }
                         return left.peer_id.localeAwareCompare(right.peer_id) < 0;
                     });

    if (candidates.isEmpty()) {
        QListWidgetItem* empty_item = new QListWidgetItem("无匹配结果");
        empty_item->setFlags(Qt::NoItemFlags);
        empty_item->setSizeHint(QSize(0, 44));
        results_widget->addItem(empty_item);
        return;
    }

    for (const SearchCandidate& candidate : candidates) {
        QListWidgetItem* item = new QListWidgetItem;
        item->setData(Qt::UserRole, candidate.conversation_key);
        item->setData(Qt::AccessibleTextRole, candidate.display_name);
        item->setSizeHint(QSize(0, 58));
        item->setToolTip(QString("%1 (%2)").arg(candidate.display_name, candidate.peer_id));
        results_widget->addItem(item);
        SearchResultItemWidget* item_widget = new SearchResultItemWidget(
            candidate.display_name,
            candidate.subtitle,
            candidate.avatar_value,
            results_widget);
        results_widget->setItemWidget(item, item_widget);
    }
}

void MainWindow::positionSearchResults(QLineEdit* anchor, QListWidget* results_widget) {
    if (!anchor || !results_widget || results_widget->count() == 0) {
        return;
    }

    QWidget* parent = results_widget->parentWidget();
    if (!parent) {
        return;
    }

    QPoint top_left = anchor->mapTo(parent, QPoint(0, anchor->height() + 4));
    const int row_height = results_widget->sizeHintForRow(0) > 0 ? results_widget->sizeHintForRow(0) : 54;
    const int desired_height = qMin(260, qMax(44, row_height * qMin(results_widget->count(), 5) + 2));
    const int target_width = qMax(anchor->width() + 96, static_cast<int>(anchor->width() * 1.45));
    const int desired_width = qMin(qMax(anchor->width(), parent->width() - 8), target_width);
    int left = top_left.x();
    if (left + desired_width > parent->width()) {
        left = qMax(0, parent->width() - desired_width - 4);
    }
    results_widget->setGeometry(left, top_left.y(), desired_width, desired_height);
    results_widget->raise();
    results_widget->show();
}

void MainWindow::hideSearchResults() {
    if (conversation_search_results_) {
        conversation_search_results_->hide();
    }
    if (contact_search_results_) {
        contact_search_results_->hide();
    }
}

void MainWindow::openSearchResultConversation(const QString& conversation_key, const QString& display_name) {
    if (conversation_key.isEmpty()) {
        return;
    }

    hideSearchResults();
    if (conversation_search_edit_) {
        conversation_search_edit_->clear();
    }
    if (contact_search_edit_) {
        contact_search_edit_->clear();
    }

    const QString peer_id = conversationPeerId(conversation_key);
    const QString title = display_name.trimmed().isEmpty()
        ? (isGroupConversation(conversation_key) ? conversationTitle(conversation_key) : contactDisplayName(peer_id))
        : display_name.trimmed();
    switchToConversation(conversation_key, title);
    if (nav_list_) {
        nav_list_->setCurrentRow(0);
    }
    if (content_stacked_ && message_view_) {
        content_stacked_->setCurrentWidget(message_view_);
    }

    for (int i = 0; chat_list_widget_ && i < chat_list_widget_->count(); ++i) {
        QListWidgetItem* item = chat_list_widget_->item(i);
        if (item && item->data(Qt::UserRole).toString() == conversation_key) {
            chat_list_widget_->setCurrentItem(item);
            chat_list_widget_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            break;
        }
    }
}

void MainWindow::onConversationSearchTextChanged(const QString& text) {
    populateSearchResults(conversation_search_results_, text, true);
    positionSearchResults(conversation_search_edit_, conversation_search_results_);
}

void MainWindow::onContactSearchTextChanged(const QString& text) {
    populateSearchResults(contact_search_results_, text, false);
    positionSearchResults(contact_search_edit_, contact_search_results_);
}

void MainWindow::onSearchResultClicked(QListWidgetItem* item) {
    if (!item || !item->flags().testFlag(Qt::ItemIsEnabled)) {
        return;
    }

    const QString user_id = item->data(Qt::UserRole).toString();
    const QString display_name = item->data(Qt::AccessibleTextRole).toString();
    openSearchResultConversation(user_id, display_name);
}

void MainWindow::openCreateGroupDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("发起群聊");
    dialog.setMinimumSize(420, 520);
    dialog.setStyleSheet(R"(
        QDialog { background-color: #ffffff; }
        QListWidget { border: 1px solid #e5e7eb; background: #ffffff; outline: none; }
        QListWidget::item { border-bottom: 1px solid #eeeeee; }
        QPushButton {
            padding: 8px 18px;
            border: none;
            border-radius: 4px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#primary { background-color: #4CAF50; color: #ffffff; }
        QPushButton#primary:disabled { background-color: #cccccc; }
        QPushButton#secondary { background-color: #f3f4f6; color: #374151; }
    )");

    QVBoxLayout* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);

    QLabel* title = new QLabel("选择联系人", &dialog);
    title->setStyleSheet("QLabel { color: #111111; font-size: 18px; font-weight: 700; }");
    root->addWidget(title);

    QListWidget* member_list = new QListWidget(&dialog);
    member_list->setSelectionMode(QAbstractItemView::NoSelection);
    member_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    const QList<QString> contact_ids = sortedContactIds();

    for (const QString& friend_id : contact_ids) {
        if (friend_id.isEmpty()) {
            continue;
        }
        QListWidgetItem* item = new QListWidgetItem(member_list);
        item->setData(Qt::UserRole, friend_id);
        item->setData(Qt::UserRole + 1, false);
        item->setFlags((item->flags() | Qt::ItemIsEnabled) & ~Qt::ItemIsUserCheckable);
        item->setSizeHint(QSize(0, 60));

        SelectableContactItemWidget* item_widget = new SelectableContactItemWidget(
            contactDisplayName(friend_id),
            contactSubtitle(friend_id),
            contact_avatars_.value(friend_id),
            [item](bool checked) {
                item->setData(Qt::UserRole + 1, checked);
            },
            member_list);
        member_list->setItemWidget(item, item_widget);
    }
    root->addWidget(member_list, 1);

    QLabel* count_label = new QLabel("已选择 0 人", &dialog);
    count_label->setStyleSheet("QLabel { color: #6b7280; font-size: 13px; }");
    root->addWidget(count_label);

    QHBoxLayout* actions = new QHBoxLayout;
    actions->addStretch();
    QPushButton* cancel_button = new QPushButton("取消", &dialog);
    cancel_button->setObjectName("secondary");
    QPushButton* create_button = new QPushButton("创建群聊", &dialog);
    create_button->setObjectName("primary");
    create_button->setEnabled(false);
    actions->addWidget(cancel_button);
    actions->addWidget(create_button);
    root->addLayout(actions);

    auto selectedMembers = [member_list]() {
        QStringList ids;
        for (int i = 0; i < member_list->count(); ++i) {
            QListWidgetItem* item = member_list->item(i);
            if (item && item->data(Qt::UserRole + 1).toBool()) {
                ids.append(item->data(Qt::UserRole).toString());
            }
        }
        return ids;
    };

    connect(member_list, &QListWidget::itemChanged, &dialog, [count_label, create_button, selectedMembers]() {
        const int count = selectedMembers().size();
        count_label->setText(QString("已选择 %1 人").arg(count));
        create_button->setEnabled(count > 0);
    });
    connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(create_button, &QPushButton::clicked, &dialog, [this, &dialog, selectedMembers]() {
        const QString initiator = user_nickname_.trimmed().isEmpty() ? user_id_ : user_nickname_.trimmed();
        const QString group_name = QString("%1发起的群聊%2")
            .arg(initiator, QDate::currentDate().toString("yy-MM-dd"));
        tcp_client_->createGroup(group_name, selectedMembers());
        dialog.accept();
    });

    if (member_list->count() == 0) {
        QMessageBox::information(this, "发起群聊", "暂无可选择的联系人");
        return;
    }

    dialog.exec();
}

void MainWindow::onSendClicked() {
    QString message = message_input_->text().trimmed();
    if (message.isEmpty() || current_chat_target_.isEmpty()) {
        return;
    }

    const QString peer_id = conversationPeerId(current_chat_target_);
    const QString chat_type = conversationChatType(current_chat_target_);
    QString msg_id = tcp_client_->sendChatMessage(peer_id, "text", message, chat_type);
    if (msg_id.isEmpty()) {
        appendMessage(user_nickname_, message, true, QString(), "failed");
    } else {
        appendMessage(user_nickname_, message, true, msg_id, "sending");
    }
    message_input_->clear();
}

void MainWindow::onChatMessageReceived(const QString& from_user_id, const QString& content,
                                       const QString& content_type,
                                       const QString& msg_id, qint64 server_timestamp,
                                       const QString& server_time,
                                       const QString& to_user_id,
                                       const QString& chat_type) {
    // 服务端推送进入后先转换为 UI 消息模型，再统一交给会话缓存处理。
    ChatViewMessage message;
    message.msg_id = msg_id;
    message.from = from_user_id;
    message.content_type = content_type.isEmpty() ? QStringLiteral("text") : content_type;
    message.content = content;
    message.time = server_time.isEmpty()
        ? QDateTime::currentDateTime().toString("hh:mm:ss")
        : server_time;
    message.timestamp = server_timestamp > 0
        ? server_timestamp
        : timestampFromText(message.time);
    if (message.timestamp <= 0) {
        message.timestamp = QDateTime::currentMSecsSinceEpoch();
    }
    message.is_mine = false;
    const QString conversation_key = chat_type == "group"
        ? conversationKey("group", to_user_id)
        : conversationKey("p2p", from_user_id);
    addMessageToConversation(conversation_key, message, conversation_key != current_chat_target_);
}

void MainWindow::onAttachFileClicked() {
    if (current_chat_target_.isEmpty()) {
        QMessageBox::information(this, "发送文件", "请先选择一个聊天对象");
        return;
    }

    const QStringList file_paths = QFileDialog::getOpenFileNames(
        this,
        "选择要发送的文件",
        QString(),
        "所有文件 (*.*)");
    if (file_paths.isEmpty()) {
        return;
    }

    tcp_client_->sendFiles(conversationPeerId(current_chat_target_),
                           file_paths,
                           conversationChatType(current_chat_target_));
}

void MainWindow::onFileMessageSent(const QString& to_user_id, const QString& chat_type, const QString& content_type,
                                   const QString& content, const QString& msg_id) {
    if (to_user_id.isEmpty()) {
        return;
    }

    ChatViewMessage message;
    message.msg_id = msg_id;
    message.from = user_nickname_;
    message.content_type = content_type.isEmpty() ? QStringLiteral("file") : content_type;
    message.content = content;
    message.time = QDateTime::currentDateTime().toString("hh:mm:ss");
    message.timestamp = QDateTime::currentMSecsSinceEpoch();
    message.status = "sending";
    message.is_mine = true;
    addMessageToConversation(conversationKey(chat_type, to_user_id), message, false);
}

void MainWindow::onFileTransferProgress(const QString& transfer_id, const QString& file_name,
                                        qint64 transferred, qint64 total, bool upload) {
    Q_UNUSED(transfer_id);
    const QString action = upload ? "上传" : "下载";
    const QString total_text = total > 0 ? humanFileSize(total) : QString("未知大小");
    statusBar()->showMessage(QString("%1 %2：%3 / %4")
                                 .arg(action, file_name, humanFileSize(transferred), total_text),
                             2500);
}

void MainWindow::onFileTransferFinished(const QString& transfer_id, const QString& file_name,
                                        const QString& save_path, bool upload, bool success,
                                        const QString& message) {
    Q_UNUSED(transfer_id);
    const QString action = upload ? "上传" : "下载";
    const QString detail = save_path.isEmpty() ? message : QString("%1：%2").arg(message, save_path);
    statusBar()->showMessage(QString("%1%2 %3：%4")
                                 .arg(action, success ? "完成" : "失败", file_name, detail),
                             5000);
    if (!success) {
        QMessageBox::warning(this, QString("%1文件").arg(action), detail);
    }
}

void MainWindow::onChatHistoryReceived(const QString& peer_id, const QString& chat_type, const QString& history_json) {
    QJsonDocument doc = QJsonDocument::fromJson(history_json.toUtf8());
    if (!doc.isArray()) return;

    QJsonArray messages = doc.array();
    if (messages.isEmpty()) return;

    // TcpClient 会把本次历史记录所属好友带回来；为空时兼容旧逻辑使用当前会话。
    const QString conversation_key = conversationKey(chat_type, peer_id.isEmpty() ? conversationPeerId(current_chat_target_) : peer_id);

    for (int i = 0; i < messages.size(); ++i) {
        QJsonObject msg = messages[i].toObject();
        QString from_user_id = msg["from_user_id"].toString();
        QString content = msg["content"].toString();
        QString content_type = msg["content_type"].toString("text");
        bool is_mine = (from_user_id == user_id_);

        ChatViewMessage view_message;
        view_message.msg_id = msg["msg_id"].toString();
        view_message.from = from_user_id;
        view_message.content_type = content_type;
        view_message.content = content;
        view_message.time = msg["server_time"].toString();
        view_message.timestamp = msg["server_timestamp"].toInteger();
        if (view_message.timestamp <= 0) {
            view_message.timestamp = timestampFromText(view_message.time);
        }
        view_message.is_mine = is_mine;
        addMessageToConversation(conversation_key, view_message, false);
    }
}

void MainWindow::onOfflineMessageReceived(const QString& from_user_id, const QString& content,
                                          const QString& content_type,
                                          const QString& msg_id, qint64 server_timestamp,
                                          const QString& server_time,
                                          const QString& to_user_id,
                                          const QString& chat_type) {
    ChatViewMessage message;
    message.msg_id = msg_id;
    message.from = from_user_id;
    message.content_type = content_type.isEmpty() ? QStringLiteral("text") : content_type;
    message.content = content;
    message.time = server_time.isEmpty()
        ? QDateTime::currentDateTime().toString("hh:mm:ss")
        : server_time;
    message.timestamp = server_timestamp > 0
        ? server_timestamp
        : timestampFromText(message.time);
    if (message.timestamp <= 0) {
        message.timestamp = QDateTime::currentMSecsSinceEpoch();
    }
    message.is_mine = false;
    const QString conversation_key = chat_type == "group"
        ? conversationKey("group", to_user_id)
        : conversationKey("p2p", from_user_id);
    addMessageToConversation(conversation_key, message, conversation_key != current_chat_target_);
}

void MainWindow::onMessageAckReceived(const QString& msg_id, const QString& status, int code, const QString& message) {
    Q_UNUSED(code);
    if (msg_id.isEmpty()) {
        return;
    }

    // ACK 只带 msg_id，因此需要在所有本地会话中查找对应消息并更新发送状态。
    for (auto it = conversations_.begin(); it != conversations_.end(); ++it) {
        for (ChatViewMessage& view_message : it->messages) {
            if (view_message.msg_id != msg_id) continue;

            view_message.status = status.isEmpty() ? "failed" : status;
            if (view_message.status == "failed" && !message.isEmpty()) {
                view_message.status = QString("failed:%1").arg(message);
            }
            if (it.key() == current_chat_target_) {
                current_messages_ = it->messages;
                rebuildMessageIndex();
                if (!updateRenderedMessageStatus(msg_id, view_message.status)) {
                    renderChatMessages(false);
                }
            }
            return;
        }
    }
}

void MainWindow::onLoadMoreMessages() {
    if (current_chat_target_.isEmpty()) return;

    // 获取当前显示的最早消息的时间戳
    // 简化处理：直接请求更多消息
    tcp_client_->getChatHistory(conversationPeerId(current_chat_target_),
                                20,
                                QDateTime::currentSecsSinceEpoch(),
                                conversationChatType(current_chat_target_));
}

void MainWindow::onChatItemClicked(QListWidgetItem* item) {
    QString conversation_key = item->data(Qt::UserRole).toString();
    QString title = conversations_.contains(conversation_key) && !conversations_[conversation_key].title.isEmpty()
        ? conversations_[conversation_key].title
        : conversationTitle(conversation_key);
    switchToConversation(conversation_key, title);
}

void MainWindow::switchToConversation(const QString& conversation_key, const QString& title) {
    // 切换会话时先显示本地缓存，再向服务端拉取历史记录补齐。
    if (conversation_key.isEmpty()) {
        return;
    }
    current_chat_target_ = conversation_key;
    const QString peer_id = conversationPeerId(conversation_key);
    const QString chat_type = conversationChatType(conversation_key);
    QString display_name = chat_type == "group"
        ? (title.trimmed().isEmpty() ? group_names_.value(peer_id, peer_id) : title)
        : contact_remarks_.value(peer_id, title);
    chat_target_label_->setText(display_name);
    chat_more_button_->setEnabled(true);
    attach_file_button_->setEnabled(true);
    conversations_[conversation_key].peer_id = peer_id;
    conversations_[conversation_key].chat_type = chat_type;
    conversations_[conversation_key].title = display_name;
    conversations_[conversation_key].unread = 0;
    current_messages_ = conversations_[conversation_key].messages;
    message_index_by_id_.clear();
    renderChatMessages(true);
    updateConversationItem(conversation_key);
    // 加载聊天记录
    tcp_client_->getChatHistory(peer_id, 20, 0, chat_type);
}

void MainWindow::switchToChatWith(const QString& user_id, const QString& nickname) {
    switchToConversation(conversationKey("p2p", user_id), nickname);
}

void MainWindow::onDisconnected() {
    status_label_->setText("离线");
    message_input_->setEnabled(false);
    attach_file_button_->setEnabled(false);
    send_button_->setEnabled(false);
    markSendingMessagesFailed("连接已断开");
}

void MainWindow::appendMessage(const QString& from, const QString& content, bool is_mine,
                               const QString& msg_id, const QString& status,
                               const QString& content_type) {
    ChatViewMessage message;
    message.msg_id = msg_id;
    message.from = from;
    message.content_type = content_type.isEmpty() ? QStringLiteral("text") : content_type;
    message.content = content;
    message.time = QDateTime::currentDateTime().toString("hh:mm:ss");
    message.timestamp = QDateTime::currentMSecsSinceEpoch();
    message.status = status;
    message.is_mine = is_mine;

    QString conversation_key = is_mine ? current_chat_target_ : conversationKey("p2p", from);
    addMessageToConversation(conversation_key, message, false);
}

void MainWindow::addMessageToConversation(const QString& peer_id, const ChatViewMessage& message, bool count_unread) {
    if (peer_id.isEmpty()) return;

    const QString conversation_key = peer_id.contains(':') ? peer_id : conversationKey("p2p", peer_id);
    ConversationState& conversation = conversations_[conversation_key];
    conversation.peer_id = conversationPeerId(conversation_key);
    conversation.chat_type = conversationChatType(conversation_key);
    if (conversation.title.isEmpty()) {
        conversation.title = conversationTitle(conversation_key);
    }

    // 历史记录、离线消息和实时推送可能重复到达，msg_id 用于本地去重。
    if (!message.msg_id.isEmpty()) {
        for (const ChatViewMessage& existing : conversation.messages) {
            if (existing.msg_id == message.msg_id) {
                updateConversationItem(conversation_key);
                return;
            }
        }
    }

    ChatViewMessage sorted_message = message;
    if (sorted_message.timestamp <= 0) {
        sorted_message.timestamp = timestampFromText(sorted_message.time);
    }

    // 按服务端时间排序，保证历史消息和新消息混合到达时聊天窗口仍然有序。
    conversation.messages.append(sorted_message);
    std::stable_sort(conversation.messages.begin(), conversation.messages.end(),
                     [](const ChatViewMessage& left, const ChatViewMessage& right) {
                         return left.timestamp < right.timestamp;
                     });

    conversation.last_message = conversation.messages.isEmpty()
        ? QString()
        : ((conversation.messages.last().content_type == "file"
            || conversation.messages.last().content_type == "image"
            || conversation.messages.last().content_type == "video")
               ? QString("[%1] %2").arg(
                     conversation.messages.last().content_type == "image" ? QStringLiteral("图片")
                     : conversation.messages.last().content_type == "video" ? QStringLiteral("视频")
                     : QStringLiteral("文件"),
                     fileMessageTitle(conversation.messages.last().content))
               : conversation.messages.last().content);
    conversation.last_timestamp = conversation.messages.isEmpty()
        ? 0
        : conversation.messages.last().timestamp;
    if (count_unread) {
        ++conversation.unread;
    }

    if (conversation_key == current_chat_target_) {
        conversation.unread = 0;
        // 如果新消息正好追加在末尾，只插入一行，避免整屏重绘造成闪动。
        const bool can_append_to_current_view =
            conversation.messages.size() == current_messages_.size() + 1 &&
            !conversation.messages.isEmpty() &&
            conversation.messages.last().msg_id == sorted_message.msg_id;

        current_messages_ = conversation.messages;
        rebuildMessageIndex();

        if (can_append_to_current_view) {
            appendMessageRow(current_messages_.last());
        } else {
            renderChatMessages(true);
        }
    }

    updateConversationItem(conversation_key);
}

QString MainWindow::conversationTitle(const QString& peer_id) const {
    const QString conversation_key = peer_id.contains(':') ? peer_id : conversationKey("p2p", peer_id);
    const QString raw_peer_id = conversationPeerId(conversation_key);
    if (isGroupConversation(conversation_key)) {
        return group_names_.value(raw_peer_id, raw_peer_id);
    }

    auto it = conversations_.constFind(conversation_key);
    if (it == conversations_.constEnd()) {
        return contactDisplayName(raw_peer_id);
    }

    const ConversationState& conversation = it.value();
    return conversation.title.isEmpty() ? contactDisplayName(raw_peer_id) : conversation.title;
}

void MainWindow::updateConversationItem(const QString& peer_id) {
    if (peer_id.isEmpty()) return;

    refreshConversationList();
}

void MainWindow::refreshConversationList() {
    QString selected_peer = current_chat_target_;
    if (chat_list_widget_->currentItem()) {
        selected_peer = chat_list_widget_->currentItem()->data(Qt::UserRole).toString();
    }

    const QList<QString> peer_ids = sortedConversationIds();

    chat_list_widget_->clear();

    for (const QString& peer_id : peer_ids) {
        const ConversationState& conversation = conversations_[peer_id];
        QListWidgetItem* item = new QListWidgetItem;
        item->setData(Qt::UserRole, peer_id);
        item->setData(Qt::AccessibleTextRole, conversationTitle(peer_id));
        item->setSizeHint(QSize(0, 64));
        chat_list_widget_->addItem(item);

        ConversationListItemWidget* item_widget = new ConversationListItemWidget(
            conversationTitle(peer_id),
            conversation.last_message,
            conversation.unread,
            chat_list_widget_);
        chat_list_widget_->setItemWidget(item, item_widget);

        if (peer_id == selected_peer) {
            chat_list_widget_->setCurrentItem(item);
        }
    }

    refreshConversationSelectionStyles();
    if (conversation_search_edit_ && !conversation_search_edit_->text().trimmed().isEmpty()) {
        onConversationSearchTextChanged(conversation_search_edit_->text());
    }
}

void MainWindow::refreshConversationSelectionStyles() {
    for (int i = 0; i < chat_list_widget_->count(); ++i) {
        QListWidgetItem* item = chat_list_widget_->item(i);
        ConversationListItemWidget* item_widget =
            static_cast<ConversationListItemWidget*>(chat_list_widget_->itemWidget(item));
        if (item_widget) {
            item_widget->setSelected(item->isSelected());
        }
    }
}

QString MainWindow::humanFileSize(qint64 size) const {
    const QStringList units = {"B", "KB", "MB", "GB"};
    double value = static_cast<double>(qMax<qint64>(0, size));
    int unit_index = 0;
    while (value >= 1024.0 && unit_index < units.size() - 1) {
        value /= 1024.0;
        ++unit_index;
    }
    return unit_index == 0
        ? QString("%1 %2").arg(static_cast<qint64>(value)).arg(units[unit_index])
        : QString("%1 %2").arg(value, 0, 'f', 1).arg(units[unit_index]);
}

QString MainWindow::fileMessageTitle(const QString& content) const {
    const QString name = messageContentObject(content)["file_name"].toString();
    if (!name.isEmpty()) {
        return name;
    }
    return QStringLiteral("文件");
}

QWidget* MainWindow::createFileMessageCard(const ChatViewMessage& message, int max_width) {
    QJsonObject file = messageContentObject(message.content);

    const QString file_id = file["file_id"].toString();
    const QString file_name = file["file_name"].toString("文件");
    const qint64 file_size = static_cast<qint64>(file["file_size"].toDouble());
    const QString mime_type = file["mime_type"].toString("application/octet-stream");

    QFrame* card = new QFrame;
    card->setMaximumWidth(max_width);
    card->setMinimumWidth(qMin(max_width, 280));
    card->setStyleSheet(QString(R"(
        QFrame {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
    )").arg(message.is_mine ? "#dff5df" : "#ffffff",
           message.is_mine ? "#b9e4bb" : "#dddddd"));

    QHBoxLayout* layout = new QHBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);

    QLabel* icon_label = new QLabel("FILE", card);
    icon_label->setFixedSize(42, 42);
    icon_label->setAlignment(Qt::AlignCenter);
    icon_label->setStyleSheet(R"(
        QLabel {
            background-color: #4CAF50;
            color: #ffffff;
            border-radius: 4px;
            font-size: 11px;
            font-weight: bold;
        }
    )");
    layout->addWidget(icon_label, 0, Qt::AlignTop);

    QWidget* text_box = new QWidget(card);
    QVBoxLayout* text_layout = new QVBoxLayout(text_box);
    text_layout->setContentsMargins(0, 0, 0, 0);
    text_layout->setSpacing(3);

    QLabel* name_label = new QLabel(file_name, text_box);
    name_label->setWordWrap(true);
    name_label->setStyleSheet("QLabel { color: #111111; font-size: 14px; font-weight: bold; border: none; background: transparent; }");
    text_layout->addWidget(name_label);

    QLabel* meta_label = new QLabel(QString("%1 · %2").arg(humanFileSize(file_size), mime_type), text_box);
    meta_label->setWordWrap(true);
    meta_label->setStyleSheet("QLabel { color: #777777; font-size: 12px; border: none; background: transparent; }");
    text_layout->addWidget(meta_label);

    layout->addWidget(text_box, 1);

    QPushButton* download_button = new QPushButton("下载", card);
    download_button->setEnabled(!file_id.isEmpty());
    download_button->setCursor(Qt::PointingHandCursor);
    download_button->setStyleSheet(R"(
        QPushButton {
            padding: 7px 12px;
            background-color: #ffffff;
            color: #2e7d32;
            border: 1px solid #b7d7b8;
            border-radius: 4px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #f4fbf4;
        }
        QPushButton:disabled {
            color: #aaaaaa;
            border-color: #dddddd;
        }
    )");
    connect(download_button, &QPushButton::clicked, this, [this, file_id, file_name]() {
        const QString save_path = QFileDialog::getSaveFileName(this, "保存文件", file_name);
        if (save_path.isEmpty()) {
            return;
        }
        tcp_client_->downloadFile(file_id, file_name, save_path);
    });
    layout->addWidget(download_button, 0, Qt::AlignVCenter);

    return card;
}

QWidget* MainWindow::createImageMessageBubble(const ChatViewMessage& message, int max_width) {
    const QJsonObject file = messageContentObject(message.content);
    const QString preview = file["preview_data_url"].toString();
    const QString file_name = file["file_name"].toString("图片");
    const QPixmap pixmap = pixmapFromDataUrl(preview, QSize(max_width, 420));

    QLabel* image_label = new QLabel;
    image_label->setToolTip(file_name);
    image_label->setAlignment(Qt::AlignCenter);
    image_label->setStyleSheet("QLabel { background: #eef0f2; border-radius: 6px; color: #777777; }");
    if (!pixmap.isNull()) {
        image_label->setPixmap(pixmap);
        image_label->setFixedSize(pixmap.size());
    } else {
        image_label->setText("图片");
        image_label->setFixedSize(qMin(max_width, 260), 160);
    }
    return image_label;
}

QWidget* MainWindow::createVideoMessageBubble(const ChatViewMessage& message, int max_width) {
    const QJsonObject file = messageContentObject(message.content);
    const QString file_id = file["file_id"].toString();
    const QString file_name = file["file_name"].toString("视频");
    const QString poster = file["poster_data_url"].toString();
    const QPixmap poster_pixmap = pixmapFromDataUrl(poster, QSize(max_width, 360));

    QFrame* frame = new QFrame;
    const QSize bubble_size = poster_pixmap.isNull()
        ? QSize(qMin(max_width, 300), 170)
        : poster_pixmap.size();
    frame->setFixedSize(bubble_size);
    frame->setStyleSheet("QFrame { background: #111111; border-radius: 6px; }");

    QGridLayout* layout = new QGridLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel* poster_label = new QLabel(frame);
    poster_label->setFixedSize(bubble_size);
    poster_label->setAlignment(Qt::AlignCenter);
    poster_label->setStyleSheet("QLabel { color: #dddddd; border-radius: 6px; background: #111111; }");
    if (!poster_pixmap.isNull()) {
        poster_label->setPixmap(poster_pixmap);
    } else {
        poster_label->setText("视频");
    }
    layout->addWidget(poster_label, 0, 0);

    QToolButton* play_button = new QToolButton(frame);
    play_button->setCursor(Qt::PointingHandCursor);
    play_button->setToolTip("下载视频");
    play_button->setText("▶");
    play_button->setFixedSize(64, 64);
    play_button->setStyleSheet(R"(
        QToolButton {
            background-color: rgba(0, 0, 0, 145);
            color: #ffffff;
            border: none;
            border-radius: 32px;
            font-size: 34px;
            padding-left: 5px;
        }
        QToolButton:hover {
            background-color: rgba(0, 0, 0, 185);
        }
        QToolButton:disabled {
            color: #999999;
        }
    )");
    play_button->setEnabled(!file_id.isEmpty());
    connect(play_button, &QToolButton::clicked, this, [this, file_id, file_name]() {
        const QString save_path = QFileDialog::getSaveFileName(this, "保存视频", file_name);
        if (save_path.isEmpty()) {
            return;
        }
        tcp_client_->downloadFile(file_id, file_name, save_path);
    });
    layout->addWidget(play_button, 0, 0, Qt::AlignCenter);
    return frame;
}

QWidget* MainWindow::createMessageRow(const ChatViewMessage& message) {
    const int viewport_width = chat_scroll_area_->viewport()
        ? chat_scroll_area_->viewport()->width()
        : chat_scroll_area_->width();
    const int max_text_width = qMax(180, static_cast<int>(viewport_width * 0.55));
    constexpr int avatar_size = 36;

    const QString display_name = message.is_mine
        ? (user_nickname_.isEmpty() ? user_id_ : user_nickname_)
        : conversationTitle(message.from);
    const QString avatar_value = message.is_mine
        ? current_avatar_url_
        : contact_avatars_.value(message.from);
    const QString profile_user_id = message.is_mine ? user_id_ : message.from;

    QWidget* row = new QWidget(chat_messages_widget_);
    row->setProperty("msg_id", message.msg_id);
    QHBoxLayout* row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(0, 4, 0, 4);
    row_layout->setSpacing(8);

    QWidget* message_column = new QWidget(row);
    QVBoxLayout* column_layout = new QVBoxLayout(message_column);
    column_layout->setContentsMargins(0, 0, 0, 0);
    column_layout->setSpacing(4);

    QLabel* meta_label = new QLabel(message.time, message_column);
    meta_label->setStyleSheet("QLabel { color: #888888; font-size: 12px; }");
    meta_label->setAlignment(message.is_mine ? Qt::AlignRight : Qt::AlignLeft);
    column_layout->addWidget(meta_label);

    QWidget* bubble_row = new QWidget(message_column);
    QHBoxLayout* bubble_row_layout = new QHBoxLayout(bubble_row);
    bubble_row_layout->setContentsMargins(0, 0, 0, 0);
    bubble_row_layout->setSpacing(8);

    QToolButton* avatar_button = new QToolButton(bubble_row);
    avatar_button->setFixedSize(avatar_size, avatar_size);
    avatar_button->setIcon(QIcon(avatarPixmapFromValue(avatar_value, display_name, avatar_size)));
    avatar_button->setIconSize(QSize(avatar_size, avatar_size));
    avatar_button->setCursor(Qt::PointingHandCursor);
    avatar_button->setToolTip(QString("查看%1的个人信息").arg(display_name));
    avatar_button->setStyleSheet(R"(
        QToolButton {
            padding: 0;
            border: none;
            border-radius: 18px;
            background: transparent;
        }
        QToolButton:hover {
            background-color: #eeeeee;
        }
    )");
    avatar_button->setEnabled(!profile_user_id.isEmpty());
    connect(avatar_button, &QToolButton::clicked, this, [this, profile_user_id]() {
        showUserProfile(profile_user_id);
    });

    QWidget* bubble = nullptr;
    if (message.content_type == "image") {
        bubble = createImageMessageBubble(message, max_text_width);
    } else if (message.content_type == "video") {
        bubble = createVideoMessageBubble(message, max_text_width);
    } else if (message.content_type == "file") {
        bubble = createFileMessageCard(message, max_text_width);
    } else {
        bubble = new MessageBubble(message.content, message.is_mine, max_text_width, bubble_row);
    }
    if (message.is_mine) {
        bubble_row_layout->addStretch();
        bubble_row_layout->addWidget(bubble, 0, Qt::AlignTop);
        bubble_row_layout->addWidget(avatar_button, 0, Qt::AlignTop);
    } else {
        bubble_row_layout->addWidget(avatar_button, 0, Qt::AlignTop);
        bubble_row_layout->addWidget(bubble, 0, Qt::AlignTop);
        bubble_row_layout->addStretch();
    }
    column_layout->addWidget(bubble_row);

    QString status = statusText(message.status);
    if (!status.isEmpty()) {
        QLabel* status_label = new QLabel(status, message_column);
        status_label->setObjectName("message_status_label");
        status_label->setStyleSheet("QLabel { color: #888888; font-size: 12px; }");
        status_label->setAlignment(Qt::AlignRight);
        column_layout->addWidget(status_label);
    }

    if (message.is_mine) {
        row_layout->addStretch();
        row_layout->addWidget(message_column, 0, Qt::AlignRight);
    } else {
        row_layout->addWidget(message_column, 0, Qt::AlignLeft);
        row_layout->addStretch();
    }

    return row;
}

void MainWindow::appendMessageRow(const ChatViewMessage& message) {
    const int insert_index = qMax(0, chat_messages_layout_->count() - 1);
    chat_messages_layout_->insertWidget(insert_index, createMessageRow(message));
    chat_messages_widget_->adjustSize();
    chat_messages_widget_->updateGeometry();
    chat_messages_layout_->activate();
    scrollToBottomAnimated();
}

void MainWindow::scrollToBottomAnimated() {
    QTimer::singleShot(0, this, [this]() {
        QScrollBar* bar = chat_scroll_area_->verticalScrollBar();
        if (!bar) return;

        const int end_value = bar->maximum();
        const int start_value = bar->value();
        if (start_value >= end_value) {
            bar->setValue(end_value);
            return;
        }

        if (chat_scroll_animation_) {
            chat_scroll_animation_->stop();
            chat_scroll_animation_->deleteLater();
            chat_scroll_animation_ = nullptr;
        }

        QPropertyAnimation* animation = new QPropertyAnimation(bar, "value", this);
        chat_scroll_animation_ = animation;
        animation->setDuration(220);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->setStartValue(start_value);
        animation->setEndValue(end_value);
        connect(animation, &QPropertyAnimation::finished, this, [this, animation, bar]() {
            if (chat_scroll_animation_ == animation) {
                chat_scroll_animation_ = nullptr;
            }
            bar->setValue(bar->maximum());
            animation->deleteLater();
        });
        animation->start();
    });
}

void MainWindow::rebuildMessageIndex() {
    message_index_by_id_.clear();
    for (int i = 0; i < current_messages_.size(); ++i) {
        if (!current_messages_[i].msg_id.isEmpty()) {
            message_index_by_id_[current_messages_[i].msg_id] = i;
        }
    }
}

bool MainWindow::updateRenderedMessageStatus(const QString& msg_id, const QString& status) {
    if (msg_id.isEmpty()) return false;

    // 优先只更新状态标签；找不到已渲染行时由调用者决定是否整屏重绘。
    const QString text = statusText(status);
    for (int i = 0; i < chat_messages_layout_->count(); ++i) {
        QLayoutItem* item = chat_messages_layout_->itemAt(i);
        QWidget* row = item ? item->widget() : nullptr;
        if (!row || row->property("msg_id").toString() != msg_id) {
            continue;
        }

        QLabel* status_label = row->findChild<QLabel*>("message_status_label");
        if (status_label) {
            status_label->setText(text);
            status_label->setVisible(!text.isEmpty());
            return true;
        }
        return text.isEmpty();
    }

    return false;
}

void MainWindow::renderChatMessages(bool scroll_to_bottom) {
    QScrollBar* scroll_bar = chat_scroll_area_->verticalScrollBar();
    const int previous_scroll_value = scroll_bar ? scroll_bar->value() : 0;
    QWidget* viewport = chat_scroll_area_->viewport();

    // 批量重建消息行时暂停绘制，避免聊天记录刷新过程出现闪烁。
    chat_scroll_area_->setUpdatesEnabled(false);
    if (viewport) {
        viewport->setUpdatesEnabled(false);
    }
    chat_messages_widget_->setUpdatesEnabled(false);

    while (QLayoutItem* item = chat_messages_layout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    for (const ChatViewMessage& message : current_messages_) {
        chat_messages_layout_->addWidget(createMessageRow(message));
    }

    chat_messages_layout_->addStretch();
    chat_messages_widget_->adjustSize();
    chat_messages_widget_->updateGeometry();
    chat_messages_layout_->activate();

    // Qt 布局会延迟计算滚动范围，连续补几次滚动位置能避免偶发不到底。
    auto apply_scroll = [this, scroll_to_bottom, previous_scroll_value]() {
        QScrollBar* bar = chat_scroll_area_->verticalScrollBar();
        if (!bar) return;
        bar->setValue(scroll_to_bottom ? bar->maximum() : previous_scroll_value);
    };

    apply_scroll();
    QTimer::singleShot(0, this, [this, apply_scroll]() {
        apply_scroll();
        chat_messages_widget_->setUpdatesEnabled(true);
        if (QWidget* current_viewport = chat_scroll_area_->viewport()) {
            current_viewport->setUpdatesEnabled(true);
            current_viewport->update();
        }
        chat_scroll_area_->setUpdatesEnabled(true);
        chat_scroll_area_->update();
    });
    QTimer::singleShot(50, this, apply_scroll);
    QTimer::singleShot(150, this, apply_scroll);
}

QString MainWindow::statusText(const QString& status) const {
    if (status == "sending") return "发送中";
    if (status == "sent") return "已发送";
    if (status == "delivered") return "已送达";
    if (status == "read") return "已读";
    if (status == "failed") return "发送失败";
    if (status.startsWith("failed:")) return QString("发送失败：%1").arg(status.mid(7));
    return QString();
}

void MainWindow::markSendingMessagesFailed(const QString& reason) {
    bool changed = false;
    for (auto it = conversations_.begin(); it != conversations_.end(); ++it) {
        for (ChatViewMessage& message : it->messages) {
            if (message.is_mine && message.status == "sending") {
                message.status = reason.isEmpty() ? "failed" : QString("failed:%1").arg(reason);
                changed = true;
            }
        }
    }
    if (changed) {
        if (!current_chat_target_.isEmpty()) {
            current_messages_ = conversations_[current_chat_target_].messages;
        }
        renderChatMessages(false);
    }
}

void MainWindow::loadChatList() {
    tcp_client_->getFriendList();
    tcp_client_->getGroupList();
}
