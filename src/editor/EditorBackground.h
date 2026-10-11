// Hutaomu Editor - 编辑器视口背景画刷（无状态共享服务）。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
//
// 设计要点：背景画刷是"纯函数"——结果只由（主题、图片文件身份、模式、浓度、
// 视口尺寸、DPR）决定，并按全部输入做缓存。任何编辑器在任何时机来问，
// 拿到的都是当下正确的答案；不存在"某个编辑器自己的状态落后了"这回事。
// （旧实现把源图/缓存键存在每个 CodeEditor 成员里，隐藏标签页、换图、
// 尺寸变化任一环节不同步就会白屏或显示旧图。）
#pragma once

#include <QBrush>
#include <QColor>
#include <QSize>

namespace editor::editorbackground {

// 视口背景画刷：有背景图时返回按 mode/opacity 合成好的纹理画刷，
// 否则（或视口过小/图片加载失败）返回主题 editorBg 纯色画刷。
QBrush viewportBrush(const QSize& viewportSize, qreal devicePixelRatio);

} // namespace editor::editorbackground
