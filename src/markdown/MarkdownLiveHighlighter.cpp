// Hutaomu Editor - Live markdown highlighting (Obsidian-style).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "MarkdownLiveHighlighter.h"

#include <QColor>
#include <QFont>
#include <QHash>
#include <QSet>
#include <QTextBlock>

#include "themes/ThemeManager.h"

namespace markdown {
namespace {

// 隐藏标记的通用格式：1pt + 全透明前景。
QTextCharFormat hiddenFormat()
{
    QTextCharFormat fmt;
    fmt.setFontPointSize(1);
    fmt.setForeground(QColor(0, 0, 0, 0));
    return fmt;
}

// 表格折叠行的隐藏格式：仅透明前景。rowPointSize > 0 时放大字号——
// 行高随字号走，折叠区因此在真实排版里占满覆盖层渲染表格所需的高度，
// 表格全览不裁剪；自然行高撑不满时由覆盖层管理器负责把字号算好。
// （1pt 会把行压到 ~2px，真实排版与覆盖层占位脱节，覆盖层会压住
// 表格上方/下方的内容；块格式 FixedHeight 在 QPlainTextEdit 不生效。）
QTextCharFormat foldedRowFormat(qreal rowPointSize)
{
    QTextCharFormat fmt;
    if (rowPointSize > 0)
        fmt.setFontPointSize(rowPointSize);
    fmt.setForeground(QColor(0, 0, 0, 0));
    return fmt;
}

} // namespace

MarkdownLiveHighlighter::MarkdownLiveHighlighter(QTextDocument* doc)
    : QSyntaxHighlighter(doc)
{
    m_headingRegex = QRegularExpression(QStringLiteral("^(#{1,6})(\\s+)(.+?)\\s*#*\\s*$"));
    m_listRegex = QRegularExpression(QStringLiteral("^(\\s*)([-*+]|\\d+[.)])(\\s+)"));
    m_taskRegex = QRegularExpression(QStringLiteral("^(\\s*[-*+]\\s+)\\[([ xX])\\](\\s+)"));
    m_tableSeparatorRegex = QRegularExpression(
        QStringLiteral("^ {0,3}\\|?( *:?-+:? *\\|)+ *:?-+:? *\\|?\\s*$"));
    m_boldItalicRegex = QRegularExpression(QStringLiteral("(\\*\\*\\*|___)(?!\\s)(.+?)(?!\\s)\\1"));
    m_boldRegex = QRegularExpression(QStringLiteral("(\\*\\*|__)(?!\\s)(.+?)(?!\\s)\\1"));
    m_italicRegex = QRegularExpression(QStringLiteral("(?<!\\*)(\\*)(?!\\s)([^*]+?)(?!\\s)\\*(?!\\*)"));
    m_strikeRegex = QRegularExpression(QStringLiteral("~~(?!\\s)(.+?)(?!\\s)~~"));
    m_codeSpanRegex = QRegularExpression(QStringLiteral("(`+)(?!`)(.+?)(?!`)\\1(?!`)"));
    m_linkRegex = QRegularExpression(QStringLiteral("(\\[)([^\\]]*)(\\]\\([^)]*\\))"));
}

// 应用折叠区行高。预留时机的空实现：
// 实测 QPlainTextDocumentLayout 不应用 QTextBlockFormat::FixedHeight
// （块高纹丝不动），改块格式也无法在 QPlainTextEdit 里撑高折叠区。
// 当前方案：折叠行文本透明但保持自然字号（foldedRowFormat），
// 行高天然正确，无需在此改块格式——也就没有 contentsChange 递归。
void MarkdownLiveHighlighter::applyFoldHeights()
{
}

// 把某个块的行高钉死为 heightPx，使其与覆盖层严格等高。
void MarkdownLiveHighlighter::setBlockHeight(QTextBlock& block, qreal heightPx)
{
    QTextCursor cursor(block);
    QTextBlockFormat format = cursor.blockFormat();
    format.setLineHeight(heightPx, QTextBlockFormat::FixedHeight);
    cursor.setBlockFormat(format);
}

void MarkdownLiveHighlighter::hideSpan(int start, int length)
{
    if (length <= 0)
        return;
    setFormat(start, length, hiddenFormat());
}

void MarkdownLiveHighlighter::setActiveRange(int firstLine, int lastLine)
{
    if (firstLine > lastLine)
        std::swap(firstLine, lastLine);
    if (firstLine == m_activeFirst && lastLine == m_activeLast)
        return;

    // 新旧范围的并集都需要重刷（旧的恢复渲染，新的恢复源码）
    const int from = qMin(m_activeFirst, firstLine);
    const int to = qMax(m_activeLast, lastLine);
    m_activeFirst = firstLine;
    m_activeLast = lastLine;

    for (int b = from; b <= to; ++b) {
        const QTextBlock block = document()->findBlockByNumber(b);
        if (block.isValid())
            rehighlightBlock(block);
    }
}

void MarkdownLiveHighlighter::setHeadingUnderline(bool on)
{
    if (m_headingUnderline == on)
        return;
    m_headingUnderline = on;
    rehighlight();
}

void MarkdownLiveHighlighter::setHiddenRegions(const QVector<markdown::TableRegion>& regions)
{
    bool same = regions.size() == m_hiddenRegions.size();
    if (same) {
        for (int i = 0; i < regions.size(); ++i) {
            if (regions.at(i).firstLine != m_hiddenRegions.at(i).firstLine
                || regions.at(i).lastLine != m_hiddenRegions.at(i).lastLine
                || qRound(regions.at(i).collapsedHeightPx * 10)
                       != qRound(m_hiddenRegions.at(i).collapsedHeightPx * 10)
                || qRound(regions.at(i).rowPointSize * 10)
                       != qRound(m_hiddenRegions.at(i).rowPointSize * 10)) {
                same = false;
                break;
            }
        }
    }
    if (same)
        return;
    m_hiddenRegions = regions;
    rehighlight();
}

void MarkdownLiveHighlighter::highlightBlock(const QString& text)
{
    const editor::ThemeTokens& t = editor::ThemeManager::tokens();
    const QColor accent = t.accent;
    const QColor muted(0x8f, 0x9a, 0x97);
    const QColor codeFg = editor::ThemeManager::syntaxColor(QStringLiteral("string"));

    const int blockNumber = currentBlock().blockNumber();
    const bool active = blockNumber >= m_activeFirst && blockNumber <= m_activeLast;

    // ---- 表格折叠区 ----
    // 文字透明化 + 按区域设定的字号撑高：折叠区在真实排版里占满覆盖层
    // 渲染表格所需的高度（MarkdownBlocks.h 的 TableRegion 注释），覆盖层
    // 按真实块几何定位即可与上下文严丝合缝。光标进入表格时切回可见源码。
    for (const markdown::TableRegion& region : m_hiddenRegions) {
        if (blockNumber < region.firstLine || blockNumber > region.lastLine)
            continue;
        setCurrentBlockState(0);
        setFormat(0, text.length(), foldedRowFormat(region.rowPointSize));
        return;
    }

    // ---- 围栏代码块：状态由 blockState 传播，闭合围栏必须与打开字符匹配 ----
    // 状态编码：0=普通，1=``` 围栏内，2=~~~ 围栏内
    const int previousState = qMax(0, previousBlockState()); // -1 = 未设置
    const bool inFence = previousState == 1 || previousState == 2;
    const QString trimmed = text.trimmed();
    const bool backtickFence = trimmed.startsWith(QStringLiteral("```"));
    const bool tildeFence = trimmed.startsWith(QStringLiteral("~~~"));
    if (backtickFence || tildeFence) {
        if (previousState == 0) {
            setCurrentBlockState(backtickFence ? 1 : 2);
            if (!active)
                hideSpan(0, text.length());
            return;
        }
        if ((previousState == 1 && backtickFence) || (previousState == 2 && tildeFence)) {
            setCurrentBlockState(0);
            if (!active)
                hideSpan(0, text.length());
            return;
        }
        // 围栏内的围栏标记字符 = 代码内容
        setCurrentBlockState(previousState);
        if (!active) {
            QTextCharFormat fmt;
            fmt.setForeground(codeFg.isValid() ? codeFg : muted);
            setFormat(0, text.length(), fmt);
        }
        return;
    }
    setCurrentBlockState(previousState);
    if (inFence) {
        if (!active) {
            QTextCharFormat fmt;
            fmt.setForeground(codeFg.isValid() ? codeFg : muted);
            setFormat(0, text.length(), fmt);
        }
        return;
    }

    // ---- 引用块 ----
    if (trimmed.startsWith(QLatin1Char('>'))) {
        if (!active) {
            QTextCharFormat fmt;
            fmt.setForeground(muted);
            fmt.setFontItalic(true);
            setFormat(0, text.length(), fmt);
            hideSpan(0, text.indexOf(QLatin1Char('>')) + 1);
        }
        return;
    }

    // ---- 表格行 ----
    if (text.contains(QLatin1Char('|'))) {
        highlightTable(text);
        return;
    }

    // ---- 标题 ----
    const QRegularExpressionMatch heading = m_headingRegex.match(text);
    if (heading.hasMatch()) {
        const int level = heading.captured(1).size();
        const int bodyStart = heading.capturedEnd(2);

        if (!active) {
            hideSpan(0, bodyStart);
            const qreal baseSize = document()->defaultFont().pointSizeF() > 0
                                       ? document()->defaultFont().pointSizeF()
                                       : 12.0;
            const qreal scale = 1.9 - 0.13 * level; // h1 约 1.77x，h6 约 1.12x
            QTextCharFormat body;
            body.setFontWeight(QFont::Bold);
            body.setFontPointSize(baseSize * scale);
            body.setForeground(accent);
            if (m_headingUnderline && level <= 2)
                body.setFontUnderline(true); // 主题形态：标题下划线
            setFormat(bodyStart, text.length() - bodyStart, body);
        }
        return;
    }

    // ---- 分隔线 ----
    if (QRegularExpression(QStringLiteral("^ {0,3}([-*_])\\s*(?:\\1\\s*){2,}$")).match(text).hasMatch()) {
        if (!active) {
            QTextCharFormat fmt;
            fmt.setForeground(muted);
            setFormat(0, text.length(), fmt);
        }
        return;
    }

    // ---- 列表标记 ----
    const QRegularExpressionMatch list = m_listRegex.match(text);
    if (list.hasMatch() && !active)
        setFormat(list.capturedStart(2), list.capturedLength(2), accent);

    // ---- 任务列表 [ ] / [x] ----
    const QRegularExpressionMatch task = m_taskRegex.match(text);
    if (task.hasMatch()) {
        const bool checked = task.captured(2).contains(QLatin1Char('x'))
                             || task.captured(2).contains(QLatin1Char('X'));
        if (!active) {
            hideSpan(task.capturedStart(2) - 1, task.capturedLength(2) + 2);
            if (checked) {
                QTextCharFormat fmt;
                fmt.setForeground(muted);
                fmt.setFontStrikeOut(true);
                setFormat(task.capturedEnd(3), text.length() - task.capturedEnd(3), fmt);
            }
        }
        return;
    }

    // ---- 行内元素 ----
    highlightInline(text, active);
}

void MarkdownLiveHighlighter::highlightTable(const QString& text)
{
    const int blockNumber = currentBlock().blockNumber();
    const bool active = blockNumber >= m_activeFirst && blockNumber <= m_activeLast;
    if (active)
        return;

    const QColor accent = editor::ThemeManager::tokens().accent;
    const QColor muted(0x8f, 0x9a, 0x97);

    if (m_tableSeparatorRegex.match(text).hasMatch()) {
        QTextCharFormat fmt;
        fmt.setForeground(muted);
        setFormat(0, text.length(), fmt);
        return;
    }

    QTextCharFormat pipeFmt;
    pipeFmt.setForeground(accent);
    for (int i = 0; i < text.length(); ++i) {
        if (text.at(i) == QLatin1Char('|'))
            setFormat(i, 1, pipeFmt);
    }
}

void MarkdownLiveHighlighter::highlightInline(const QString& text, bool active)
{
    if (active)
        return; // 光标行保持原始 Markdown，便于编辑

    const editor::ThemeTokens& t = editor::ThemeManager::tokens();
    const QColor accent = t.accent;
    const QColor muted(0x8f, 0x9a, 0x97);
    const QColor codeFg = editor::ThemeManager::syntaxColor(QStringLiteral("string"));

    // 已着色/已隐藏区间，防止嵌套正则互相覆盖
    QVector<QPair<int, int>> taken;
    auto occupied = [&taken](int start, int end) {
        for (const auto& range : taken) {
            if (start < range.second && end > range.first)
                return true;
        }
        return false;
    };
    auto take = [&taken](int start, int end) {
        taken.append({ start, end });
    };

    // 链接 [text](url)
    QRegularExpressionMatchIterator it = m_linkRegex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (occupied(m.capturedStart(), m.capturedEnd()))
            continue;
        QTextCharFormat fmt;
        fmt.setForeground(accent);
        fmt.setFontUnderline(true);
        setFormat(m.capturedStart(2), m.capturedLength(2), fmt);
        hideSpan(m.capturedStart(1), m.capturedLength(1));
        hideSpan(m.capturedStart(3), m.capturedLength(3));
        take(m.capturedStart(), m.capturedEnd());
    }

    // 行内代码 `code`
    it = m_codeSpanRegex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (occupied(m.capturedStart(), m.capturedEnd()))
            continue;
        QTextCharFormat fmt;
        fmt.setForeground(codeFg.isValid() ? codeFg : accent);
        setFormat(m.capturedStart(2), m.capturedLength(2), fmt);
        hideSpan(m.capturedStart(1), m.capturedLength(1));
        hideSpan(m.capturedEnd(2), m.capturedLength(1));
        take(m.capturedStart(), m.capturedEnd());
    }

    // 粗斜体 ***text***
    it = m_boldItalicRegex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (occupied(m.capturedStart(), m.capturedEnd()))
            continue;
        QTextCharFormat fmt;
        fmt.setFontWeight(QFont::Bold);
        fmt.setFontItalic(true);
        setFormat(m.capturedStart(2), m.capturedLength(2), fmt);
        hideSpan(m.capturedStart(1), m.capturedLength(1));
        hideSpan(m.capturedEnd(2), m.capturedLength(1));
        take(m.capturedStart(), m.capturedEnd());
    }

    // 粗体 **text**
    it = m_boldRegex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (occupied(m.capturedStart(), m.capturedEnd()))
            continue;
        QTextCharFormat fmt;
        fmt.setFontWeight(QFont::Bold);
        setFormat(m.capturedStart(2), m.capturedLength(2), fmt);
        hideSpan(m.capturedStart(1), m.capturedLength(1));
        hideSpan(m.capturedEnd(2), m.capturedLength(1));
        take(m.capturedStart(), m.capturedEnd());
    }

    // 斜体 *text*
    it = m_italicRegex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (occupied(m.capturedStart(), m.capturedEnd()))
            continue;
        QTextCharFormat fmt;
        fmt.setFontItalic(true);
        setFormat(m.capturedStart(2), m.capturedLength(2), fmt);
        hideSpan(m.capturedStart(1), m.capturedLength(1));
        hideSpan(m.capturedEnd(2), m.capturedLength(1));
        take(m.capturedStart(), m.capturedEnd());
    }

    // 删除线 ~~text~~
    it = m_strikeRegex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (occupied(m.capturedStart(), m.capturedEnd()))
            continue;
        QTextCharFormat fmt;
        fmt.setFontStrikeOut(true);
        fmt.setForeground(muted);
        setFormat(m.capturedStart(2), m.capturedLength(2), fmt);
        hideSpan(m.capturedStart(1), m.capturedLength(1));
        hideSpan(m.capturedEnd(2), m.capturedLength(1));
        take(m.capturedStart(), m.capturedEnd());
    }
}

} // namespace markdown
