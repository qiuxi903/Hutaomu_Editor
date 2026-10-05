// Hutaomu Editor - Frameless title bar with embedded menus (VS Code style).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "TitleBar.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QToolButton>
#include <QWindow>

#include "themes/ThemeManager.h"
#include "themes/IconLoader.h"

namespace app {

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("TitleBar"));
    setFixedHeight(36);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 0, 0);
    layout->setSpacing(6);

    // 品牌 Logo + 名称
    m_brandIcon = new QLabel(this);
    m_brandIcon->setObjectName(QStringLiteral("brandIcon"));
    m_brandIcon->setFixedSize(18, 18);
    m_brandIcon->setScaledContents(true);
    layout->addWidget(m_brandIcon);

    m_brandName = new QLabel(this);
    m_brandName->setObjectName(QStringLiteral("brandName"));
    layout->addWidget(m_brandName);

    layout->addSpacing(6);

    m_menuBar = new QMenuBar(this);
    m_menuBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    layout->addWidget(m_menuBar);

    layout->addStretch();

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));
    layout->addWidget(m_titleLabel);
    connect(window(), &QWidget::windowTitleChanged,
            m_titleLabel, &QLabel::setText);
    m_titleLabel->setText(window()->windowTitle());

    layout->addStretch();

    m_minButton = makeWindowButton(QStringLiteral("win-min"), QStringLiteral("winMin"));
    m_maxButton = makeWindowButton(QStringLiteral("win-max"), QStringLiteral("winMax"));
    m_closeButton = makeWindowButton(QStringLiteral("win-close"), QStringLiteral("winClose"));
    layout->addWidget(m_minButton);
    layout->addWidget(m_maxButton);
    layout->addWidget(m_closeButton);

    connect(m_minButton, &QToolButton::clicked,
            this, [this] { window()->showMinimized(); });
    connect(m_maxButton, &QToolButton::clicked,
            this, [this] {
                if (window()->isMaximized())
                    window()->showNormal();
                else
                    window()->showMaximized();
            });
    connect(m_closeButton, &QToolButton::clicked,
            this, [this] { window()->close(); });
}

QToolButton* TitleBar::makeWindowButton(const QString& iconName, const QString& objectName)
{
    auto* button = new QToolButton(this);
    button->setObjectName(objectName);
    button->setFixedSize(44, 36);
    button->setAutoRaise(true);
    button->setProperty("iconName", iconName);
    refreshIcons();
    return button;
}

void TitleBar::refreshIcons()
{
    const editor::ThemeTokens& t = editor::ThemeManager::tokens();
    const QColor glyph = editor::ThemeManager::color("titleBarText");
    for (QToolButton* button : { m_minButton, m_maxButton, m_closeButton }) {
        if (!button)
            continue;
        const QString name = button->property("iconName").toString();
        button->setIcon(editor::IconLoader::load(name, glyph, t.titleBarBackground, 16));
        button->setIconSize(QSize(16, 16));
    }

    // 品牌区随主题换色
    if (m_brandIcon) {
        m_brandIcon->setPixmap(editor::IconLoader::load(
            QStringLiteral("brand"), t.accent, t.titleBarBackground, 18));
    }
    if (m_brandName) {
        const QString subColor = editor::ThemeManager::color("brandSub").name();
        m_brandName->setText(
            QStringLiteral("<span style='color:%1;font-weight:600;'>Hutaomu</span>"
                           "<span style='color:%2;'> Editor</span>")
                .arg(t.accent.name(), subColor));
    }
}

void TitleBar::updateMaximizeButton()
{
    if (!m_maxButton)
        return;
    m_maxButton->setProperty("iconName",
                             window()->isMaximized() ? QStringLiteral("win-restore")
                                                     : QStringLiteral("win-max"));
    refreshIcons();
}

void TitleBar::showEvent(QShowEvent* event)
{
    // The window handle only exists once the top-level is realized.
    if (QWindow* win = window()->windowHandle()) {
        connect(win, &QWindow::visibilityChanged,
                this, &TitleBar::updateMaximizeButton, Qt::UniqueConnection);
        updateMaximizeButton();
    }
    QWidget::showEvent(event);
}

void TitleBar::mousePressEvent(QMouseEvent* event)
{
    // Empty areas drag the window; child widgets (menus, buttons) win first.
    if (event->button() == Qt::LeftButton) {
        if (QWindow* win = windowHandle())
            win->startSystemMove();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (window()->isMaximized())
            window()->showNormal();
        else
            window()->showMaximized();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

} // namespace app
