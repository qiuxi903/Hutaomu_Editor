// Hutaomu Editor - Activity bar implementation (panel registry + drag).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "ActivityBar.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QMimeData>
#include <QIcon>
#include <QToolButton>
#include <QVBoxLayout>

#include "../app/PanelDragMover.h"
#include "themes/IconLoader.h"
#include "themes/ThemeManager.h"

namespace app {

ActivityBar::ActivityBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ActivityBar"));
    setFixedWidth(48);
    setAcceptDrops(true); // 接收面板标题栏拖入（收起为按钮）

    m_buttons.reserve(8);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 4);
    layout->setSpacing(2);

    explorerButton = addPanelButton(QStringLiteral("explorer"),
                                    QStringLiteral("activity-explorer"),
                                    tr("资源管理器"), true);
    connect(explorerButton, &QToolButton::toggled,
            this, &ActivityBar::explorerToggled);

    searchButton = addPanelButton(QStringLiteral("search"),
                                  QStringLiteral("activity-search"),
                                  tr("全局搜索"));
    connect(searchButton, &QToolButton::toggled,
            this, &ActivityBar::searchToggled);

    layout->addStretch();

    QToolButton* about = makeButton(QStringLiteral("activity-about"), tr("关于"));
    layout->addWidget(about);
    connect(about, &QToolButton::clicked,
            this, &ActivityBar::aboutRequested);
}

QToolButton* ActivityBar::addPanelButton(const QString& pageId,
                                         const QString& iconName,
                                         const QString& toolTip, bool checked,
                                         const QIcon& icon)
{
    if (QToolButton* existing = m_pageButtons.value(pageId))
        return existing;

    QToolButton* button = makeButton(iconName, toolTip);
    if (!icon.isNull())   // 插件页面：直接给渲染好的图标
        button->setIcon(icon);
    button->setCheckable(true);
    button->setChecked(checked);
    button->setProperty("pageId", pageId);
    m_pageButtons.insert(pageId, button);

    // 互斥：点亮某页时静默取消其它页（VS Code 单页显示语义）
    connect(button, &QToolButton::toggled, this, [this, pageId](bool checked) {
        if (!checked)
            return;
        for (auto it = m_pageButtons.constBegin(); it != m_pageButtons.constEnd(); ++it) {
            if (it.key() != pageId && it.value()->isChecked()) {
                it.value()->blockSignals(true);
                it.value()->setChecked(false);
                it.value()->blockSignals(false);
            }
        }
        emit panelToggled(pageId, true);
    });
    return button;
}

void ActivityBar::removePanelButtonsForPrefix(const QString& prefix)
{
    const auto keys = m_pageButtons.keys();
    for (const QString& pageId : keys) {
        if (!pageId.startsWith(prefix))
            continue;
        QToolButton* button = m_pageButtons.take(pageId);
        if (button) {
            button->setParent(nullptr);
            button->deleteLater();
        }
    }
}

void ActivityBar::setPanelChecked(const QString& pageId, bool checked)
{
    // 状态同步（启动/布局重排）不应触发页面折叠逻辑：
    // 状态相同则无事发生；状态变化也只静默更新按钮，宿主如需响应
    // 应走用户交互路径（clicked/toggled 由真实点击发出）。
    if (QToolButton* button = m_pageButtons.value(pageId)) {
        if (button->isChecked() == checked)
            return;
        button->blockSignals(true);
        button->setChecked(checked);
        button->blockSignals(false);
    }
}

bool ActivityBar::isPanelChecked(const QString& pageId) const
{
    const QToolButton* button = m_pageButtons.value(pageId);
    return button && button->isChecked();
}

void ActivityBar::removePanelButton(const QString& pageId)
{
    if (QToolButton* button = m_pageButtons.take(pageId)) {
        m_buttons.removeOne(button);
        button->deleteLater();
    }
}

QStringList ActivityBar::panelOrder() const
{
    QStringList order;
    // m_buttons 即视觉顺序（restackButtons 维护）
    for (QToolButton* button : m_buttons) {
        const QString id = button->property("pageId").toString();
        if (!id.isEmpty())
            order.append(id);
    }
    return order;
}

void ActivityBar::restoreOrder(const QStringList& pageIds)
{
    // 按 pageIds 顺序重排 m_buttons 中对应按钮，再重装布局
    QVector<QToolButton*> ordered;
    for (const QString& id : pageIds) {
        if (QToolButton* b = m_pageButtons.value(id))
            ordered.append(b);
    }
    for (QToolButton* b : m_buttons) {
        if (!ordered.contains(b))
            ordered.append(b); // 未列出的（如占位）保持相对顺序
    }
    // 重排：替换 m_buttons 中页面按钮的顺序，非页面按钮（about）位置不变
    QVector<QToolButton*> merged;
    for (QToolButton* b : m_buttons) {
        if (b->property("pageId").toString().isEmpty())
            merged.append(b);
    }
    // 简化：全部页面按钮按 ordered，非页面按钮（about 等）按原相对位置插入
    QVector<QToolButton*> result;
    int pageIdx = 0;
    int aboutIdx = 0;
    QVector<QToolButton*> statics;
    for (QToolButton* b : m_buttons) {
        if (b->property("pageId").toString().isEmpty())
            statics.append(b);
    }
    // 现有布局里 stretch 之前是页面按钮、之后是静态按钮——按此重建
    for (QToolButton* b : ordered) {
        if (!statics.contains(b))
            result.append(b);
    }
    result.append(statics);
    m_buttons = result;
    restackButtons();
}

QToolButton* ActivityBar::makeButton(const QString& iconName, const QString& toolTip)
{
    auto* button = new QToolButton(this);
    button->setObjectName(QStringLiteral("activityButton"));
    button->setProperty("iconName", iconName);
    button->setToolTip(toolTip);
    button->setAutoRaise(true);
    button->setFixedSize(44, 44);
    button->setIconSize(QSize(24, 24));
    m_buttons.append(button);

    // 新按钮插到 stretch 之前（页面区）
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout) {
        int stretchIndex = layout->count() - 1; // stretch 是最后一项
        layout->insertWidget(qMax(0, stretchIndex), button);
    }
    restyleButton(button);
    return button;
}

void ActivityBar::restackButtons()
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (!layout)
        return;
    // 取出所有项再按 m_buttons 顺序放回（stretch 保持最后）
    QWidget* stretchItem = nullptr;
    QList<QWidget*> others;
    while (layout->count() > 0) {
        QLayoutItem* item = layout->takeAt(0);
        if (item->spacerItem()) {
            stretchItem = new QWidget(this);
            stretchItem->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
            delete item;
        } else if (item->widget()) {
            if (item->widget() != stretchItem)
                others.append(item->widget());
            delete item;
        } else {
            delete item;
        }
    }
    for (QToolButton* button : m_buttons)
        layout->addWidget(button);
    if (stretchItem)
        layout->addWidget(stretchItem);
    for (QWidget* w : others) {
        if (w && !m_buttons.contains(qobject_cast<QToolButton*>(w)))
            layout->addWidget(w);
    }
}

void ActivityBar::restyleButton(QToolButton* button)
{
    const editor::ThemeTokens& t = editor::ThemeManager::tokens();
    const QColor iconColor = button->isEnabled()
                                 ? editor::ThemeManager::color("activityIconFg")
                                 : editor::ThemeManager::color("activityIconDisabledFg");
    button->setIcon(editor::IconLoader::load(button->property("iconName").toString(),
                                     iconColor, t.activityBarBackground, 24));
}

void ActivityBar::refreshIcons()
{
    for (QToolButton* button : m_buttons)
        restyleButton(button);
}

// ---- 拖放：接收面板标题栏拖入（收起为按钮）/ 按钮重排 ----

void ActivityBar::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(QLatin1String(editor::panelIdMime()))) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void ActivityBar::dragMoveEvent(QDragMoveEvent* event)
{
    if (!event->mimeData()->hasFormat(QLatin1String(editor::panelIdMime())))
        return;
    event->acceptProposedAction();
    // 计算插入位置：拖到哪个按钮上方就插到它前面
    m_dropBeforeId.clear();
    for (QToolButton* button : m_buttons) {
        if (button->geometry().contains(event->position().toPoint())) {
            const QString id = button->property("pageId").toString();
            if (!id.isEmpty())
                m_dropBeforeId = id;
            break;
        }
    }
}

void ActivityBar::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasFormat(QLatin1String(editor::panelIdMime())))
        return;
    const QString droppedId =
        QString::fromUtf8(event->mimeData()->data(QLatin1String(editor::panelIdMime())));
    event->acceptProposedAction();
    emit panelToggled(droppedId, isPanelChecked(droppedId)); // 语义由宿主决定
}

} // namespace app
