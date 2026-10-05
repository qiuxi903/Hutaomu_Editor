// Hutaomu Editor - Touch gesture scrolling with inertia for scroll areas.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "TouchScroller.h"

#include <QAbstractScrollArea>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTouchEvent>

namespace editor {
namespace {
constexpr qreal kInertiaDecay = 0.94;     // 每帧速度衰减
constexpr qreal kInertiaStopSpeed = 0.03; // px/ms，低于此速度停止
constexpr int kInertiaIntervalMs = 16;

bool isSynthetic(const QMouseEvent* mouse)
{
    return mouse->source() == Qt::MouseEventSynthesizedBySystem
           || mouse->source() == Qt::MouseEventSynthesizedByApplication;
}
} // namespace

TouchScroller::TouchScroller(QAbstractScrollArea* area, QWidget* target,
                             std::function<void(const QPoint&)> onTap,
                             QObject* parent)
    : QObject(parent)
    , m_area(area)
    , m_target(target)
    , m_onTap(std::move(onTap))
    , m_verticalScaleFn([] { return 1.0; })
{
    target->setAttribute(Qt::WA_AcceptTouchEvents, true);
    target->installEventFilter(this);

    m_inertiaTimer.setInterval(kInertiaIntervalMs);
    connect(&m_inertiaTimer, &QTimer::timeout,
            this, &TouchScroller::inertiaStep);
}

void TouchScroller::setVerticalPixelScaleFn(std::function<qreal()> fn)
{
    m_verticalScaleFn = std::move(fn);
}

void TouchScroller::setPanThreshold(int px)
{
    m_panThreshold = qMax(4, px);
}

bool TouchScroller::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_target)
        return QObject::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd: {
        auto* touch = static_cast<QTouchEvent*>(event);
        if (touch->points().isEmpty())
            break;
        const QPointF pos = touch->points().first().position();

        if (event->type() == QEvent::TouchBegin)
            processBegin(pos);
        else if (event->type() == QEvent::TouchUpdate)
            processUpdate(pos);
        else
            processEnd(pos);
        return true; // 接管触摸，阻止合成为鼠标选择
    }

    case QEvent::TouchCancel:
        // 系统收回触摸序列（如手掌误触识别）：复位状态并吞掉
        m_panning = false;
        m_inertiaTimer.stop();
        return true;

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove: {
        // 部分触摸屏驱动不产生 QTouchEvent，只产生合成的鼠标事件。
        // 这些事件同样按"拖动=滚动、轻点=轻点回调"处理；真实鼠标放行。
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (!isSynthetic(mouse))
            break;

        if (event->type() == QEvent::MouseButtonPress
            && mouse->button() == Qt::LeftButton)
            processBegin(mouse->position());
        else if (event->type() == QEvent::MouseMove
                 && mouse->buttons() & Qt::LeftButton)
            processUpdate(mouse->position());
        else if (event->type() == QEvent::MouseButtonRelease
                 && mouse->button() == Qt::LeftButton)
            processEnd(mouse->position());
        else
            break;
        return true;
    }

    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}

void TouchScroller::processBegin(const QPointF& pos)
{
    m_inertiaTimer.stop();
    m_panning = false;
    m_startPos = m_lastPos = pos;
    m_startVertical = m_area->verticalScrollBar()->value();
    m_startHorizontal = m_area->horizontalScrollBar()->value();
    m_velocity = QPointF();
    m_deltaClock.restart();
}

void TouchScroller::processUpdate(const QPointF& pos)
{
    const QPointF delta = pos - m_startPos;
    if (!m_panning
        && (qAbs(delta.x()) > m_panThreshold || qAbs(delta.y()) > m_panThreshold))
        m_panning = true;

    if (m_panning) {
        QScrollBar* v = m_area->verticalScrollBar();
        QScrollBar* h = m_area->horizontalScrollBar();
        const qreal vScale = m_verticalScaleFn ? m_verticalScaleFn() : 1.0;
        v->setValue(qBound(v->minimum(),
                           m_startVertical - qRound(delta.y() / vScale),
                           v->maximum()));
        h->setValue(qBound(h->minimum(), m_startHorizontal - qRound(delta.x()), h->maximum()));

        const qint64 dt = m_deltaClock.restart();
        if (dt > 0)
            m_velocity = (pos - m_lastPos) / qreal(dt);
        m_lastPos = pos;
    }
}

void TouchScroller::processEnd(const QPointF& pos)
{
    if (!m_panning) {
        // 轻点：交给宿主处理（定位光标 / 展开表格 / 选中条目）
        if (m_onTap)
            m_onTap(pos.toPoint());
        return;
    }

    // 惯性：按释放时的速度继续滚动并衰减
    m_inertiaVelocity = m_velocity;
    if (m_inertiaVelocity.manhattanLength() > kInertiaStopSpeed)
        m_inertiaTimer.start();
}

void TouchScroller::inertiaStep()
{
    QScrollBar* v = m_area->verticalScrollBar();
    QScrollBar* h = m_area->horizontalScrollBar();
    const qreal vScale = m_verticalScaleFn ? m_verticalScaleFn() : 1.0;
    const bool vMovable = v->minimum() < v->maximum() && qAbs(m_inertiaVelocity.y()) > 0.001;
    const bool hMovable = h->minimum() < h->maximum() && qAbs(m_inertiaVelocity.x()) > 0.001;

    if (vMovable)
        v->setValue(qBound(v->minimum(),
                           v->value() - qRound(m_inertiaVelocity.y() * kInertiaIntervalMs / vScale),
                           v->maximum()));
    if (hMovable)
        h->setValue(qBound(h->minimum(), h->value() - qRound(m_inertiaVelocity.x() * kInertiaIntervalMs),
                           h->maximum()));

    m_inertiaVelocity *= kInertiaDecay;
    const bool hitEdge = (v->value() == v->maximum() || v->value() == v->minimum())
                         && (h->value() == h->maximum() || h->value() == h->minimum());
    if (m_inertiaVelocity.manhattanLength() < kInertiaStopSpeed || hitEdge)
        m_inertiaTimer.stop();
}

} // namespace editor
