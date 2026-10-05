// Hutaomu Editor - Sidebar column implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "PanelColumn.h"

#include <QLabel>
#include <QPainter>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace app {

PanelColumn::PanelColumn(const QString& id, QWidget* parent)
    : QWidget(parent)
    , m_id(id)
{
    setObjectName(QStringLiteral("panelColumn_%1").arg(id));
    // 侧栏列是"固定宽度"的容器：不随窗口拉伸，宽度由 QSplitter 的
    // sizes 决定（否则内部的 sizeHint 会把它撑大——"侧边栏莫名变大"）
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    setVisible(false);
}

QWidget* PanelColumn::panelAt(int index) const
{
    return (index >= 0 && index < m_panels.size()) ? m_panels.at(index) : nullptr;
}

void PanelColumn::addPanel(QWidget* panel)
{
    insertPanel(panel, m_panels.size());
}

void PanelColumn::insertPanel(QWidget* panel, int index)
{
    index = qBound(0, index, m_panels.size());
    if (m_panels.contains(panel))
        m_panels.removeOne(panel);
    m_panels.insert(index, panel);
    relayout();
    if (!isVisible())
        setVisible(true);
}

void PanelColumn::takePanel(QWidget* panel)
{
    if (!m_panels.removeOne(panel))
        return;
    relayout();
    if (m_panels.isEmpty())
        setVisible(false);
    if (m_indicatorBefore == panel) {
        m_indicatorBefore = nullptr;
        m_indicatorActive = false;
    }
}

bool PanelColumn::containsPanel(QWidget* panel) const
{
    return m_panels.contains(panel);
}

int PanelColumn::panelCount() const
{
    return m_panels.size();
}

QStringList PanelColumn::panelOrder() const
{
    QStringList order;
    for (QWidget* p : m_panels)
        order.append(p->objectName());
    return order;
}

QString PanelColumn::dropTarget(QWidget* dragging, const QPoint& globalPos)
{
    const QPoint local = mapFromGlobal(globalPos);
    if (!rect().contains(local))
        return QString();

    QString before;
    int y = 0;
    for (QWidget* p : m_panels) {
        if (p == dragging) {
            y += p->height();
            continue;
        }
        const int mid = y + p->height() / 2;
        if (local.y() < mid) {
            before = p->objectName();
            break;
        }
        y += p->height();
    }
    m_indicatorBefore = m_panels.size() > 1 ? panelForId(before) : nullptr;
    m_indicatorActive = true;
    update();
    return before;
}

void PanelColumn::clearDropIndicator()
{
    if (m_indicatorActive) {
        m_indicatorActive = false;
        m_indicatorBefore = nullptr;
        update();
    }
}

QWidget* PanelColumn::panelForId(const QString& id) const
{
    for (QWidget* p : m_panels) {
        if (p->objectName() == id)
            return p;
    }
    return nullptr;
}

QString PanelColumn::hitTestId(const QPoint& globalPos) const
{
    const QPoint local = mapFromGlobal(globalPos);
    if (!rect().contains(local))
        return QString();
    int y = 0;
    for (QWidget* p : m_panels) {
        if (local.y() >= y && local.y() < y + p->height())
            return p->objectName();
        y += p->height();
    }
    return QString();
}

void PanelColumn::relayout()
{
    // 重装布局：按 m_panels 顺序
    QList<QWidget*> current;
    while (m_layout->count() > 0) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (item->widget())
            current.append(item->widget());
        delete item;
    }
    for (QWidget* p : m_panels)
        m_layout->addWidget(p);
    for (QWidget* w : current) {
        if (!m_panels.contains(w))
            w->setParent(nullptr);
    }
}

void PanelColumn::paintEvent(QPaintEvent*)
{
    if (!m_indicatorActive)
        return;
    // 指示线：画在目标面板顶边（或唯一面板的中线）
    QPainter p(this);
    p.setPen(QPen(QColor(0x2d, 0x8c, 0x74), 3));
    QWidget* before = m_indicatorBefore;
    if (before) {
        p.drawLine(0, before->y(), width(), before->y());
    } else if (!m_panels.isEmpty()) {
        QWidget* last = m_panels.last();
        p.drawLine(0, last->y() + last->height(), width(),
                   last->y() + last->height());
    }
}

void PanelColumn::resizeEvent(QResizeEvent*)
{
    // 列内卡片高度均分（VS Code 的 section 默认等分，用户可后续拖分隔条）
}

} // namespace app