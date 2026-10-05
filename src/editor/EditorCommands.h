// Hutaomu Editor - Editor command helpers (comments, markdown wraps).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QString>

class QPlainTextEdit;

namespace syntax {
class LanguageSpec;
}

namespace editor {
namespace commands {

// Ctrl+/：对选区覆盖的行做行注释开/关。无行注释语法的语言为无操作。
void toggleLineComment(QPlainTextEdit* editor, const syntax::LanguageSpec* spec);

// Ctrl+Shift+/：用块注释包裹/解包选区。
void toggleBlockComment(QPlainTextEdit* editor, const syntax::LanguageSpec* spec);

// 用成对标记包裹选区（Markdown 加粗/斜体等）；无选区时插入一对并置于中间。
void wrapSelection(QPlainTextEdit* editor, const QString& marker);

} // namespace commands
} // namespace editor
