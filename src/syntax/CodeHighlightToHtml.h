// Hutaomu Editor - Highlight a code snippet to colored HTML (preview pane).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QString>

namespace syntax {

// 用 tree-sitter 高亮 code（语言按 lang 名从注册表取），输出带
// <span style="color:..."> 的 HTML（已转义）。lang 为空或未识别时仅做转义。
QString codeToHtml(const QString& code, const QString& lang);

} // namespace syntax
