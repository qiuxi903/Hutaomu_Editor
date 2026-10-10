// Hutaomu Editor - UI animations implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "UiAnimations.h"

#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>

namespace app::fx {

namespace {

// 主题切换 crossfade 的旧快照标签
QPointer<QLabel> s_fadeOverlay;

} // namespace

void beginThemeFade(QWidget* window)
{
    if (!window)
        return;
    endThemeFade();

    // 截取当前外观快照（在主题应用之前）
    const QPixmap snapshot = window->grab();
    if (snapshot.isNull())
        return;

    // 叠一层半透明标签显示旧快照，然后淡出
    auto* overlay = new QLabel(window);
    overlay->setPixmap(snapshot);
    overlay->setGeometry(window->rect());
    overlay->setAlignment(Qt::AlignCenter);
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->show();
    overlay->raise();
    s_fadeOverlay = overlay;
}

void endThemeFade()
{
    if (!s_fadeOverlay)
        return;
    QLabel* overlay = s_fadeOverlay;

    auto* effect = new QGraphicsOpacityEffect(overlay);
    overlay->setGraphicsEffect(effect);
    auto* anim = new QPropertyAnimation(effect, "opacity", overlay);
    anim->setDuration(220);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(anim, &QPropertyAnimation::finished,
                     overlay, &QObject::deleteLater);
    anim->start(QPropertyAnimation::DeleteWhenStopped);
    s_fadeOverlay.clear();
}

void fadeIn(QWidget* widget, int durationMs)
{
    if (!widget)
        return;
    auto* effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    effect->setOpacity(0.0);

    auto* anim = new QPropertyAnimation(effect, "opacity", widget);
    anim->setDuration(durationMs);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(anim, &QPropertyAnimation::finished, effect, [effect]() {
        // 动画完成后移除效果（避免影响后续渲染）
        if (qobject_cast<QWidget*>(effect->parent()))
            qobject_cast<QWidget*>(effect->parent())->setGraphicsEffect(nullptr);
    });
    anim->start(QPropertyAnimation::DeleteWhenStopped);
}

void fadeOutAndClose(QWidget* widget, int durationMs)
{
    if (!widget)
        return;
    auto* effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);

    auto* anim = new QPropertyAnimation(effect, "opacity", widget);
    anim->setDuration(durationMs);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);
    QObject::connect(anim, &QPropertyAnimation::finished,
                     widget, &QWidget::close);
    anim->start(QPropertyAnimation::DeleteWhenStopped);
}

} // namespace app::fx
