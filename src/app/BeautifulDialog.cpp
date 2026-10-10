// Hutaomu Editor - Beautiful message boxes implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "BeautifulDialog.h"

#include <QDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "themes/ThemeManager.h"
#include "app/UiAnimations.h"

namespace app::dialog {

namespace {

// 画图标（不用 Qt 内置图标——自己画颜色和形状统一）
QPixmap makeIcon(Icon icon, int size)
{
    QPixmap pixmap(size * 2, size * 2);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(2.0);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF circle(2, 2, size - 4, size - 4);
    const QFont iconFont(QStringLiteral("Segoe UI Symbol"), qRound(size * 0.45));
    painter.setFont(iconFont);

    QColor bg, fg;
    QString glyph;
    switch (icon) {
    case Icon::Question:
        bg = QColor(0x00, 0x78, 0xd4);
        fg = Qt::white;
        glyph = QStringLiteral("?");
        break;
    case Icon::Warning:
        bg = QColor(0xff, 0x9e, 0x00);
        fg = Qt::white;
        glyph = QStringLiteral("!");
        break;
    case Icon::Error:
        bg = QColor(0xd9, 0x53, 0x4f);
        fg = Qt::white;
        glyph = QStringLiteral("✕");
        break;
    case Icon::Success:
        bg = QColor(0x2e, 0xa8, 0x4f);
        fg = Qt::white;
        glyph = QStringLiteral("✓");
        break;
    case Icon::Info:
    default:
        bg = QColor(0x00, 0x78, 0xd4);
        fg = Qt::white;
        glyph = QStringLiteral("i");
        break;
    }

    painter.setBrush(bg);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(circle);
    painter.setPen(fg);
    painter.drawText(circle, Qt::AlignCenter, glyph);
    painter.end();
    return pixmap;
}

} // namespace

QPushButton* show(QWidget* parent,
                  const QString& title,
                  const QString& message,
                  const QString& informativeText,
                  Icon icon,
                  const QList<QPair<QString, Button>>& buttons)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("beautifulDialog"));
    dialog.setProperty("dialogStyled", true); // QSS 选择器用
    dialog.setWindowFlags(Qt::Dialog);
    // 不用 FramelessWindowHint / WA_TranslucentBackground：
    // 实测在 Windows 上会导致窗口阻塞（点不动）和偶发崩溃。
    // 改用标准 QDialog + QSS 圆角（安全且效果相当）
    dialog.setMinimumWidth(380);
    dialog.setMaximumWidth(520);

    // 圆角容器（带阴影效果的边框）
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(12);

    // 标题行：图标 + 粗体标题
    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(12);
    if (icon != Icon::None) {
        auto* iconLabel = new QLabel(&dialog);
        iconLabel->setPixmap(makeIcon(icon, 32));
        iconLabel->setFixedSize(32, 32);
        titleRow->addWidget(iconLabel, 0, Qt::AlignTop);
    }
    auto* titleLabel = new QLabel(title, &dialog);
    titleLabel->setObjectName(QStringLiteral("dialogTitle"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.2);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setWordWrap(true);
    titleRow->addWidget(titleLabel, 1);
    layout->addLayout(titleRow);

    // 正文
    if (!message.isEmpty()) {
        auto* messageLabel = new QLabel(message, &dialog);
        messageLabel->setObjectName(QStringLiteral("dialogMessage"));
        messageLabel->setWordWrap(true);
        layout->addWidget(messageLabel);
    }

    // 补充说明（灰色小字）
    if (!informativeText.isEmpty()) {
        auto* infoLabel = new QLabel(informativeText, &dialog);
        infoLabel->setObjectName(QStringLiteral("dialogInfo"));
        infoLabel->setWordWrap(true);
        layout->addWidget(infoLabel);
    }

    layout->addStretch(1);

    // 按钮行（右对齐，等宽，间距 8px）
    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    buttonRow->addStretch(1);

    QPushButton* result = nullptr;
    QList<QPushButton*> buttonWidgets;
    for (const auto& [text, role] : buttons) {
        auto* btn = new QPushButton(text, &dialog);
        btn->setMinimumHeight(36);
        btn->setMinimumWidth(88);
        btn->setCursor(Qt::PointingHandCursor);

        // 主按钮用 accent 背景，其他用次要样式
        if (role == Button::Ok || role == Button::Save || role == Button::Yes) {
            btn->setObjectName(QStringLiteral("dialogPrimaryButton"));
        } else if (role == Button::Discard || role == Button::No) {
            btn->setObjectName(QStringLiteral("dialogDangerButton"));
        } else {
            btn->setObjectName(QStringLiteral("dialogSecondaryButton"));
        }

        buttonWidgets.append(btn);
        buttonRow->addWidget(btn);

        QPushButton* capturedBtn = btn;
        QObject::connect(btn, &QPushButton::clicked, [&dialog, &result, capturedBtn]() {
            result = capturedBtn;
            dialog.accept();
        });
    }
    layout->addLayout(buttonRow);

    // 关闭（Esc / X）= 取消
    QObject::connect(&dialog, &QDialog::rejected, [&result]() {
        result = nullptr;
    });

    dialog.adjustSize();
    if (parent) {
        dialog.move(parent->geometry().center() - dialog.rect().center());
    }
    dialog.exec();
    return result;
}

bool confirm(QWidget* parent, const QString& title, const QString& message,
             const QString& confirmText)
{
    QPushButton* clicked = show(parent, title, message, QString(), Icon::Question,
                               { { confirmText, Button::Ok },
                                 { QStringLiteral("取消"), Button::Cancel } });
    return clicked != nullptr;
}

void info(QWidget* parent, const QString& title, const QString& message)
{
    show(parent, title, message, QString(), Icon::Info,
         { { QStringLiteral("确定"), Button::Ok } });
}

void warning(QWidget* parent, const QString& title, const QString& message)
{
    show(parent, title, message, QString(), Icon::Warning,
         { { QStringLiteral("确定"), Button::Ok } });
}

} // namespace app::dialog
