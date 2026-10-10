// Hutaomu Editor - UI animations: theme crossfade + menu fade-in.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QEasingCurve>
#include <QPointer>
#include <QWidget>

class QPropertyAnimation;
class QGraphicsOpacityEffect;
class QTimer;

namespace app::fx {

// 主题切换：截取当前窗口快照，叠加半透明旧快照淡出（crossfade 效果）。
// 用法：applyTheme 之前调用 beginThemeFade(window)，之后调用 endThemeFade()。
void beginThemeFade(QWidget* window);
void endThemeFade();

// 菜单/弹出面板淡入：设置 opacity 0→1（150ms）。
// 用法：widget->show() 之后调用 fadeIn(widget)。
void fadeIn(QWidget* widget, int durationMs = 150);

// 菜单/弹出面板淡出后关闭。
void fadeOutAndClose(QWidget* widget, int durationMs = 120);

} // namespace app::fx
