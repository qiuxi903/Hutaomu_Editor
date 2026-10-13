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

    // stretch 锚点：页面按钮插在它之前，底部组/关于在其后
    m_stretch = new QWidget(this);
    m_stretch->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    layout->addWidget(m_stretch);

    QToolButton* about = makeButton(QStringLiteral("activity-about"), tr("关于"));
    layout->addWidget(about);
    connect(about, &QToolButton::clicked,
            this, &ActivityBar::aboutRequested);
}

QToolButton* ActivityBar::addPanelButton(const QString& pageId,
                                         const QString& iconName,
                                         const QString& toolTip, bool checked,
                                         const QIcon& icon, bool bottomGroup)
{
    if (QToolButton* existing = m_pageButtons.value(pageId))
        return existing;

    QToolButton* button = makeButton(iconName, toolTip);
    if (!icon.isNull())   // 插件页面：直接给渲染好的图标
        button->setIcon(icon);
    button->setCheckable(true);
    button->setChecked(checked);
    button->setProperty("pageId", pageId);
    if (bottomGroup) {
        // 底部组（VS Code 的扩展/齿轮位）：移到 stretch 之后，重排时保持
        button->setProperty("bottomGroup", true);
        if (auto* layout = qobject_cast<QVBoxLayout*>(this->layout()))
            layout->addWidget(button); // 从页面区移到 stretch 之后
    }
    m_pageButtons.insert(pageId, button);

    // 按钮 = 面板开关：点亮/熄灭都转发给宿主（展开/收起对应面板）。
    // 侧栏列是多卡片堆叠，多个按钮同时点亮是正常状态——
    // 不做互斥（旧互斥只偷别的按钮高亮、不动面板，反而造成状态错乱）
    connect(button, &QToolButton::toggled, this, [this, pageId](bool checked) {
        emit panelToggled(pageId, checked);
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

    // 页面按钮插到 stretch 锚点之前（顶部组）；bottomGroup 在其后（底部组）
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout) {
        const int index = m_stretch ? layout->indexOf(m_stretch)
                                    : layout->count();
        layout->insertWidget(qMax(0, index), button);
    }
    restyleButton(button);
    return button;
}

void ActivityBar::restackButtons()
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (!layout || !m_stretch)
        return;
    // 取出所有项，按 顶部组(m_buttons 序) → stretch → 底部组 → 其他(关于) 重装
    QList<QWidget*> others;
    QList<QToolButton*> top, bottom;
    while (layout->count() > 0) {
        QLayoutItem* item = layout->takeAt(0);
        if (item->widget())
            others.append(item->widget());
        delete item;
    }
    for (QToolButton* button : m_buttons) {
        if (button->property("bottomGroup").toBool())
            bottom.append(button);
        else if (!button->property("pageId").toString().isEmpty())
            top.append(button);
        else
            others.append(button); // 关于等非页面按钮
    }
    for (QToolButton* button : top)
        layout->addWidget(button);
    layout->addWidget(m_stretch);
    for (QToolButton* button : bottom)
        layout->addWidget(button);
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
