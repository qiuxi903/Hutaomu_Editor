// Hutaomu Editor - Drop feedback layer implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "DropZoneOverlay.h"

#include <QPainter>

namespace editor {

DropFeedbackLayer::DropFeedbackLayer(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("dropFeedbackLayer"));
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setVisible(false);
}

void DropFeedbackLayer::begin(const QString& panelId)
{
    m_panelId = panelId;
    m_target = Target::None;
    setGeometry(parentWidget()->rect());
    setVisible(true);
    raise();
}

void DropFeedbackLayer::end()
{
    setVisible(false);
    m_target = Target::None;
}

void DropFeedbackLayer::updateHover(const QPoint& globalPos,
                                    bool overActivityBar)
{
    const Target t = overActivityBar ? Target::ActivityBar : Target::None;
    if (t != m_target) {
        m_target = t;
        update();
    }
}

void DropFeedbackLayer::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    if (m_target == Target::ActivityBar) {
        // 高亮活动栏区域：覆盖 x∈[0,48]
        QWidget* bar = parentWidget()->findChild<QWidget*>(
            QStringLiteral("ActivityBar"));
        if (bar) {
            p.fillRect(QRect(0, 0, bar->width(), height()),
                       QColor(0x2d, 0x8c, 0x74, 60));
            p.setPen(QPen(QColor(0x2d, 0x8c, 0x74), 2));
            p.drawRect(QRect(1, 1, bar->width() - 2, height() - 2));
            p.setPen(Qt::white);
            p.save();
            p.translate(24, height() / 2);
            p.rotate(-90);
            const QString tip = tr("松开收起为按钮");
            const int tw = p.fontMetrics().horizontalAdvance(tip);
            p.drawText(QRect(-tw / 2, -8, tw, 16), Qt::AlignCenter, tip);
            p.restore();
        }
    }
}

} // namespace editor