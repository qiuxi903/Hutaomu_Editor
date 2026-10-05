// Hutaomu Editor - Panel header drag implementation (ghost + mouse tracking).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "PanelDragMover.h"

#include <QMouseEvent>
#include <QPainter>

namespace editor {

namespace {
constexpr int kDragThreshold = 8; // px，超过才起拖
} // namespace

PanelDragMover::PanelDragMover(QWidget* header, const QString& panelId,
                               QObject* parent)
    : QObject(parent)
    , m_header(header)
    , m_panelId(panelId)
{
    header->installEventFilter(this);
    header->setCursor(Qt::SizeAllCursor);
}

bool PanelDragMover::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_header)
        return QObject::eventFilter(watched, event);

    auto* mouse = static_cast<QMouseEvent*>(event);
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        if (mouse->button() == Qt::LeftButton) {
            m_pressed = true;
            m_dragging = false;
            m_pressGlobal = mouse->globalPosition().toPoint();
        }
        break;
    case QEvent::MouseMove:
        if (!m_pressed)
            break;
        if (!m_dragging
            && (mouse->globalPosition().toPoint() - m_pressGlobal).manhattanLength()
                   > kDragThreshold) {
            m_dragging = true;
            emit dragStarted(m_panelId);
        }
        if (m_dragging) {
            emit dragMoved(m_panelId, mouse->globalPosition().toPoint());
            return true;
        }
        break;
    case QEvent::MouseButtonRelease:
        if (m_pressed && mouse->button() == Qt::LeftButton) {
            const bool wasDragging = m_dragging;
            m_pressed = false;
            m_dragging = false;
            if (wasDragging) {
                emit dragFinished(m_panelId, mouse->globalPosition().toPoint());
                return true;
            }
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

// ---- 幻影标题条 ----

PanelDragGhost::PanelDragGhost(QWidget* parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setVisible(false);
}

void PanelDragGhost::showFor(const QString& title, const QPoint& globalPos,
                             int width)
{
    m_title = title;
    setFixedSize(width, 34);
    moveTo(globalPos);
    setVisible(true);
    raise();
}

void PanelDragGhost::moveTo(const QPoint& globalPos)
{
    move(globalPos.x() - width() / 2, globalPos.y() - 17);
}

void PanelDragGhost::hideGhost()
{
    setVisible(false);
}

void PanelDragGhost::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0x2d, 0x8c, 0x74), 2));
    p.setBrush(QColor(0x2d, 0x8c, 0x74, 200));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
    p.setPen(Qt::white);
    p.drawText(rect().adjusted(12, 0, -8, 0), Qt::AlignVCenter, m_title);
}

} // namespace editor