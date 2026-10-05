// Hutaomu Editor - tree-sitter based syntax highlighting bridge.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "TreeSitterHighlighter.h"

#include <QFile>
#include <QStringView>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include "themes/ThemeManager.h"

namespace syntax {
namespace {

// 捕获名 -> 展示色角色。primary 段（"keyword.modifier" -> "keyword"）优先，
// 退化到次级段（"variable.builtin" -> "builtin"）。
QString primaryRole(const char* name, uint32_t len)
{
    const QString full = QString::fromLatin1(name, len);
    return full.section(QLatin1Char('.'), 0, 0);
}

QString secondaryRole(const char* name, uint32_t len)
{
    const QString full = QString::fromLatin1(name, len);
    return full.section(QLatin1Char('.'), 1);
}

// UTF-8 字节偏移 -> 行内 UTF-16 字符索引（QTextLayout::FormatRange 用字符单位）。
int utf16IndexInLine(const char* data, int lineStartByte, int byteInLine)
{
    int codePoints = 0;
    int i = lineStartByte;
    const int end = lineStartByte + byteInLine;
    while (i < end) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        const int width = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        i += width;
        ++codePoints;
    }
    // 越界防御（CJK 组合等极端场景忽略）：
    return codePoints;
}

int utf16WidthOfLine(const char* data, int lineStartByte, int lineByteLen)
{
    return utf16IndexInLine(data, lineStartByte, lineByteLen);
}

} // namespace

TreeSitterHighlighter::TreeSitterHighlighter(QTextDocument* doc, const LanguageSpec* spec)
    : QSyntaxHighlighter(doc)
    , m_spec(spec)
{
    m_parser = ts_parser_new();
    if (m_spec && m_spec->parser) {
        if (!ts_parser_set_language(m_parser, m_spec->parser())) {
            ts_parser_delete(m_parser);
            m_parser = nullptr;
        }
    }

    if (m_parser && m_spec && m_spec->hasHighlighting()) {
        // 拼接多份 query（例如 cpp 复用 c 的基础高亮）。
        QByteArray source;
        for (const QString& resource : m_spec->highlightQueries) {
            QFile file(resource);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                source += file.readAll();
                source += '\n';
            }
        }

        uint32_t errorOffset = 0;
        TSQueryError errorType = TSQueryErrorNone;
        m_query = ts_query_new(m_parser ? ts_parser_language(m_parser) : nullptr,
                               source.constData(), uint32_t(source.size()),
                               &errorOffset, &errorType);
        if (m_query) {
            m_cursor = ts_query_cursor_new();
            const uint32_t captureCount = ts_query_capture_count(m_query);
            m_captureColors.resize(int(captureCount));
            for (uint32_t i = 0; i < captureCount; ++i) {
                uint32_t len = 0;
                const char* name = ts_query_capture_name_for_id(m_query, i, &len);
                QColor color = editor::ThemeManager::syntaxColor(primaryRole(name, len));
                if (!color.isValid())
                    color = editor::ThemeManager::syntaxColor(secondaryRole(name, len));
                m_captureColors[int(i)] = color;
            }
            m_queryReady = true;
        }

        connect(&m_debounce, &QTimer::timeout, this, &TreeSitterHighlighter::retokenize);
        m_debounce.setSingleShot(true);
        m_debounce.setInterval(120);
        connect(doc, &QTextDocument::contentsChange,
                this, [this](int, int, int) { scheduleRetokenize(); });
        retokenize(); // 首次全量
    }
}

TreeSitterHighlighter::~TreeSitterHighlighter()
{
    if (m_tree)
        ts_tree_delete(m_tree);
    if (m_cursor)
        ts_query_cursor_delete(m_cursor);
    if (m_query)
        ts_query_delete(m_query);
    if (m_parser)
        ts_parser_delete(m_parser);
}

void TreeSitterHighlighter::refreshTheme()
{
    if (!m_query)
        return;
    const uint32_t captureCount = ts_query_capture_count(m_query);
    m_captureColors.resize(int(captureCount));
    for (uint32_t i = 0; i < captureCount; ++i) {
        uint32_t len = 0;
        const char* name = ts_query_capture_name_for_id(m_query, i, &len);
        QColor color = editor::ThemeManager::syntaxColor(primaryRole(name, len));
        if (!color.isValid())
            color = editor::ThemeManager::syntaxColor(secondaryRole(name, len));
        m_captureColors[int(i)] = color;
    }
    retokenize();
}

void TreeSitterHighlighter::scheduleRetokenize()
{
    m_debounce.start();
}

void TreeSitterHighlighter::retokenize()
{
    if (!m_parser || !m_queryReady)
        return;

    const QString text = document()->toPlainText();
    const QByteArray utf8 = text.toUtf8();
    const char* data = utf8.constData();
    const uint32_t length = uint32_t(utf8.size());

    TSTree* tree = ts_parser_parse_string(m_parser, nullptr, data, length);
    if (!tree)
        return;
    ts_tree_delete(m_tree);
    m_tree = tree;

    // 行起始字节偏移表
    QVector<int> lineStarts;
    lineStarts.append(0);
    for (int i = 0; i < int(length); ++i) {
        if (data[i] == '\n')
            lineStarts.append(i + 1);
    }
    const int lineCount = lineStarts.size();

    m_lineFormats.clear();
    m_lineFormats.reserve(lineCount);

    TSNode root = ts_tree_root_node(m_tree);
    ts_query_cursor_exec(m_cursor, m_query, root);

    TSQueryMatch match;
    uint32_t matchIndex = 0;
    while (ts_query_cursor_next_match(m_cursor, &match)) {
        for (uint16_t c = 0; c < match.capture_count; ++c) {
            const TSQueryCapture& capture = match.captures[c];
            const QColor& color = m_captureColors.value(int(capture.index));
            if (!color.isValid())
                continue;

            const uint32_t start = ts_node_start_byte(capture.node);
            const uint32_t end = ts_node_end_byte(capture.node);

            // 定位起始行
            int line = 0;
            {
                int lo = 0, hi = lineCount - 1;
                while (lo < hi) {
                    const int mid = (lo + hi + 1) / 2;
                    if (lineStarts[mid] <= int(start)) lo = mid; else hi = mid - 1;
                }
                line = lo;
            }

            uint32_t pos = start;
            while (line < lineCount && pos < end) {
                const int lineStart = lineStarts[line];
                const int lineEnd = (line + 1 < lineCount) ? lineStarts[line + 1] - 1 /*不含\n*/
                                                           : int(length);
                const int spanEnd = qMin(int(end), lineEnd);
                const int byteInLineStart = int(pos) - lineStart;
                const int byteLen = spanEnd - int(pos);

                const int charStart = utf16IndexInLine(data, lineStart, byteInLineStart);
                const int charEnd = utf16IndexInLine(data, lineStart, byteInLineStart + byteLen);
                if (charEnd > charStart) {
                    QTextLayout::FormatRange range;
                    range.start = charStart;
                    range.length = charEnd - charStart;
                    QTextCharFormat fmt;
                    fmt.setForeground(color);
                    range.format = fmt;
                    m_lineFormats[line].append(range);
                }

                pos = uint32_t(spanEnd);
                ++line;
                Q_UNUSED(utf16WidthOfLine);
            }
        }
    }

    rehighlight();
}

void TreeSitterHighlighter::highlightBlock(const QString&)
{
    const int blockNumber = currentBlock().blockNumber();
    const QVector<QTextLayout::FormatRange> ranges = m_lineFormats.value(blockNumber);
    for (const QTextLayout::FormatRange& range : ranges)
        setFormat(range.start, range.length, range.format);
}

} // namespace syntax
