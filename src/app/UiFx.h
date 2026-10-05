// Hutaomu Editor - Small UI animation helpers.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

class QWidget;

namespace ui {

// 淡入动画：动画结束后移除透明度效果，避免常驻渲染开销。
void fadeIn(QWidget* widget, int durationMs = 160);

} // namespace ui
