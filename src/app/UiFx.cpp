// Hutaomu Editor - Small UI animation helpers.
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "UiFx.h"

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QWidget>

namespace ui {

void fadeIn(QWidget* widget, int durationMs)
{
    if (!widget)
        return;

    auto* effect = new QGraphicsOpacityEffect(widget);
    effect->setOpacity(0.0);
    widget->setGraphicsEffect(effect);

    auto* animation = new QPropertyAnimation(effect, "opacity", effect);
    animation->setDuration(durationMs);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(animation, &QPropertyAnimation::finished, widget,
                     [widget] { widget->setGraphicsEffect(nullptr); });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace ui
