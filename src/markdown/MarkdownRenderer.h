// Hutaomu Editor - Markdown to HTML renderer (md4c based, for preview pane).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QString>

namespace markdown {

struct RenderResult {
    QString html;        // 可直接放进 QTextBrowser 的 HTML 片段
    QString title;       // 文档首个一级标题（若有）
};

// CommonMark + GFM 方言（表格、删除线、任务列表）。
// codeLangHint: 代码块语言由代码块 info 串决定；此参数仅用于无 info 时的默认。
RenderResult renderToHtml(const QString& markdown);

} // namespace markdown
