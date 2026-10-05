// Hutaomu Editor - Small UI animation helpers.
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

class QWidget;

namespace ui {

// 淡入动画：动画结束后移除透明度效果，避免常驻渲染开销。
void fadeIn(QWidget* widget, int durationMs = 160);

} // namespace ui
