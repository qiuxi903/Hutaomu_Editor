// Hutaomu Editor - Highlight a code snippet to colored HTML (preview pane).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "CodeHighlightToHtml.h"

#include <QByteArray>
#include <QFile>
#include <QString>

#include <algorithm>

#include "syntax/LanguageRegistry.h"
#include "themes/ThemeManager.h"

extern "C" {
#include <tree_sitter/api.h>
}

namespace syntax {
namespace {

QString escapeHtml(const QString& text)
{
    QString out = text;
    out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    return out;
}

QString span(const QColor& color, const QString& escapedText)
{
    return QStringLiteral("<span style=\"color:%1;\">%2</span>")
        .arg(color.name(), escapedText);
}

// 与 TreeSitterHighlighter 相同的捕获名解析逻辑
QString primaryRole(const char* name, uint32_t len)
{
    return QString::fromLatin1(name, len).section(QLatin1Char('.'), 0, 0);
}

QString secondaryRole(const char* name, uint32_t len)
{
    return QString::fromLatin1(name, len).section(QLatin1Char('.'), 1);
}

} // namespace

QString codeToHtml(const QString& code, const QString& lang)
{
    const LanguageSpec* spec = LanguageRegistry::instance().findById(lang.toLower());
    if (!spec || !spec->hasHighlighting())
        return escapeHtml(code);

    TSParser* parser = ts_parser_new();
    if (!ts_parser_set_language(parser, spec->parser())) {
        ts_parser_delete(parser);
        return escapeHtml(code);
    }

    const QByteArray utf8 = code.toUtf8();
    TSTree* tree = ts_parser_parse_string(parser, nullptr, utf8.constData(),
                                          uint32_t(utf8.size()));
    if (!tree) {
        ts_parser_delete(parser);
        return escapeHtml(code);
    }

    QByteArray source;
    for (const QString& resource : spec->highlightQueries) {
        QFile file(resource);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            source += file.readAll();
    }

    uint32_t errOffset = 0;
    TSQueryError errType = TSQueryErrorNone;
    TSQuery* query = ts_query_new(spec->parser(), source.constData(),
                                  uint32_t(source.size()), &errOffset, &errType);
    if (!query) {
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        return escapeHtml(code);
    }

    QHash<int, QColor> captureColors;
    const uint32_t captureCount = ts_query_capture_count(query);
    for (uint32_t i = 0; i < captureCount; ++i) {
        uint32_t len = 0;
        const char* name = ts_query_capture_name_for_id(query, i, &len);
        QColor color = editor::ThemeManager::syntaxColor(primaryRole(name, len));
        if (!color.isValid())
            color = editor::ThemeManager::syntaxColor(secondaryRole(name, len));
        if (color.isValid())
            captureColors.insert(int(i), color);
    }

    // 收集着色区间，排序后线性拼接（外层区间优先）
    struct Piece {
        int start;
        int end;
        QColor color;
    };
    QVector<Piece> pieces;

    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, query, ts_tree_root_node(tree));
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        for (uint16_t c = 0; c < match.capture_count; ++c) {
            const TSQueryCapture& capture = match.captures[c];
            const QColor color = captureColors.value(int(capture.index));
            if (!color.isValid())
                continue;
            const int start = int(ts_node_start_byte(capture.node));
            const int end = int(ts_node_end_byte(capture.node));
            if (end > start)
                pieces.append({ start, end, color });
        }
    }
    ts_query_cursor_delete(cursor);
    ts_query_delete(query);
    ts_tree_delete(tree);
    ts_parser_delete(parser);

    if (pieces.isEmpty())
        return escapeHtml(code);

    std::sort(pieces.begin(), pieces.end(),
              [](const Piece& a, const Piece& b) {
                  if (a.start != b.start)
                      return a.start < b.start;
                  return a.end > b.end; // 长区间优先
              });

    QString html;
    html.reserve(utf8.size() * 2);
    int pos = 0;
    int i = 0;
    while (pos < utf8.size()) {
        // 跳过被已输出区间覆盖的 pieces
        while (i < pieces.size() && pieces[i].end <= pos)
            ++i;
        if (i >= pieces.size())
            break;

        const Piece& piece = pieces[i];
        if (piece.start > pos) {
            html += escapeHtml(QString::fromUtf8(utf8.mid(pos, piece.start - pos)));
            pos = piece.start;
        }
        // 在 [pos, piece.end) 内找首个可扩展段（处理 UTF-8 边界不做切分，直接整段）
        html += span(piece.color,
                     escapeHtml(QString::fromUtf8(utf8.mid(pos, piece.end - pos))));
        pos = piece.end;
        ++i;
    }
    if (pos < utf8.size())
        html += escapeHtml(QString::fromUtf8(utf8.mid(pos)));

    return html;
}

} // namespace syntax
