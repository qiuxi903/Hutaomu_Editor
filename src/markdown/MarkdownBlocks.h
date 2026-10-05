// Hutaomu Editor - Markdown block scanning (tables) shared by the editor
// overlay and the live highlighter.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QVector>

namespace markdown {

// 一个被"折叠成覆盖层"的表格区域。
//
// 单位约定（曾因混用 pt/px 导致表格下方出现大片空白，务必遵守）：
//   collapsedHeightPx —— 折叠后要占的**像素**高度，= max(折叠行真实块高
//                        之和, 渲染内容高度)。折叠行文字透明，由
//                        rowPointSize 控制字号把行撑到目标高度，实测回填；
//                        覆盖层几何、行号、滚动位置全部同源。
//   htmlHeightPx      —— 覆盖层渲染出的表格内容的实测高度（在覆盖层实际
//                        排版宽度下测量）。折叠区至少要占这么高，表格才能
//                        完整显示、不被裁剪。
//   rowPointSize      —— 折叠行透明文字的字号（pt）。QPlainTextEdit 无法
//                        用块格式撑高行（FixedHeight 不生效），字号是唯一
//                        可靠的行高控制手段；由覆盖层管理器按需计算。
//   pointToPixel      —— 当前编辑器字号下 1pt 对应的像素高（含行距），
//                        由编辑器在字体变化时告知，用于 px↔pt 换算。
struct TableRegion {
    int firstLine = -1;   // 0-based，含表头
    int lastLine = -1;    // 0-based，含最后一行数据

    qreal collapsedHeightPx = 90.0; // 折叠后占用的像素高度（渲染后实测回填）
    qreal htmlHeightPx = 0.0;       // 渲染内容实测高度（折叠区高度下限）
    qreal rowPointSize = 0.0;       // 折叠行透明文字字号（pt），0 = 不调整
    qreal pointToPixel = 1.53;      // 1pt = 多少像素（含行距）
};

// 扫描文档中的 GFM 表格区域（表头 + |---| 分隔行 + 数据行连续出现）。
// 代码围栏（``` / ~~~）内的内容不参与。
QVector<TableRegion> scanTableRegions(const QString& markdown);

bool isTableSeparatorLine(const QString& line);

} // namespace markdown
