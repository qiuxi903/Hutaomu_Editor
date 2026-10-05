// Hutaomu Editor - Touch gesture scrolling with inertia for scroll areas.
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QTimer>

#include <functional>

class QAbstractScrollArea;
class QMouseEvent;

namespace editor {

// 给任意控件加上触屏行为（拖动=滚动+惯性，轻点=onTap 回调），覆盖两条路径：
// 1. 真实 QTouchEvent（触摸屏/触控笔）；
// 2. Qt 为触摸合成的鼠标事件（MouseEventSynthesizedBySystem）——部分触摸
//    屏驱动只走这条路径；合成事件被消费，不会触发文本选择/误点。
// 真实鼠标（NotSynthesized）完全不受影响。
// 触摸/合成事件都未出现时（纯鼠标设备），一切保持鼠标原生行为。
class TouchScroller : public QObject {
    Q_OBJECT
public:
    // area：实际要滚动的滚动区域；target：接收触摸/合成鼠标的控件
    //（通常是 area 的 viewport，也可以是覆盖层等）。
    TouchScroller(QAbstractScrollArea* area, QWidget* target,
                  std::function<void(const QPoint&)> onTap,
                  QObject* parent = nullptr);

    // 垂直滚动条的"像素/单位"换算。QPlainTextEdit 的垂直滚动条以"行"为
    // 单位（约一行高），不换算的话手指 1px 会被当成 1 行，快得离谱。
    // 默认 1.0（预览等像素滚动的控件无需设置）。
    void setVerticalPixelScaleFn(std::function<qreal()> fn);
    // 拖动死区（px）。触摸轻点常带小幅位移，列表类控件适当放大
    // 可以避免"想点却变成滑"。默认 10。
    void setPanThreshold(int px);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void inertiaStep();

private:
    void processBegin(const QPointF& pos);
    void processUpdate(const QPointF& pos);
    void processEnd(const QPointF& pos);

    QAbstractScrollArea* m_area = nullptr;
    QWidget* m_target = nullptr;
    std::function<void(const QPoint&)> m_onTap;

    bool m_panning = false;
    QPointF m_startPos;
    QPointF m_lastPos;
    QPointF m_velocity;      // px / ms
    int m_startVertical = 0;
    int m_startHorizontal = 0;
    QElapsedTimer m_deltaClock;

    QTimer m_inertiaTimer;
    QPointF m_inertiaVelocity;
    std::function<qreal()> m_verticalScaleFn;
    int m_panThreshold = 10;
};

} // namespace editor
