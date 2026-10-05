// Hutaomu Editor - Theme-aware SVG icon loading.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QColor>
#include <QPixmap>
#include <QString>

namespace editor {

// Loads SVGs, substituting the FILLCOLOR and PANELCOLOR placeholders with theme
// colors before rendering, and caches the result. QIcon-free by design: widgets
// get pixmaps sized for their DPR.
//
// 查找顺序（T3 资源覆盖）：
//   1. <当前主题根>/assets/icons/<name>.svg   ← 主题包覆盖（可整体换风格）
//   2. :/icons/<name>.svg                     ← 内置图标
// 主题覆盖文件里的 SVG 可含 FILLCOLOR/PANELCOLOR 占位符（随主题换色），
// 也可完全写死颜色（如品牌 logo 用固有色）。
class IconLoader {
public:
    static QPixmap load(const QString& name, const QColor& fill,
                        const QColor& panel, int size);

    // 从指定 SVG 文件渲染（插件包内的图标：不走主题覆盖查找，直接按路径读）
    static QPixmap loadFile(const QString& path, const QColor& fill,
                           const QColor& panel, int size);

    // 清空缓存（主题切换后调用；load() 的缓存键已含主题 id，这里用于强制重读）
    static void clearCache();

private:
    IconLoader() = default;
};

} // namespace editor
