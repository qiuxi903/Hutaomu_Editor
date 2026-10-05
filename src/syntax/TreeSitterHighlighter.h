// Hutaomu Editor - tree-sitter based syntax highlighting bridge.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QTimer>
#include <QVector>
#include <QSyntaxHighlighter>

#include "syntax/LanguageSpec.h"

extern "C" {
#include <tree_sitter/api.h>
}

class QTextDocument;

namespace syntax {

// 全量重解析 + 全量查询 + 按行分发的简单设计：
// - contentsChange 触发 120ms 防抖；
// - 超时后取整篇纯文本转 UTF-8，完整 reparse，跑一遍高亮 query；
// - 把捕获区间换算成 行 -> QTextLayout::FormatRange 列表，再 rehighlight。
// 文件在数十 MB 内此路径开销可接受；增量解析留待后续里程碑。
class TreeSitterHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    TreeSitterHighlighter(QTextDocument* doc, const LanguageSpec* spec);
    ~TreeSitterHighlighter() override;

    const LanguageSpec* language() const { return m_spec; }

public slots:
    void refreshTheme(); // 主题切换后重取语法配色

protected:
    void highlightBlock(const QString& text) override;

private slots:
    void retokenize();

private:
    void scheduleRetokenize();

    const LanguageSpec* m_spec = nullptr;
    TSParser* m_parser = nullptr;
    TSQuery* m_query = nullptr;
    TSQueryCursor* m_cursor = nullptr;
    TSTree* m_tree = nullptr;
    QVector<QColor> m_captureColors; // capture index -> color
    QHash<int, QVector<QTextLayout::FormatRange>> m_lineFormats;
    QTimer m_debounce;
    bool m_queryReady = false;
};

} // namespace syntax
