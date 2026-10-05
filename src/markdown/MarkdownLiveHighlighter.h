// Hutaomu Editor - Live markdown highlighting (Obsidian-style).
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>

#include "markdown/MarkdownBlocks.h"

namespace markdown {

// 实时预览装饰层：
// - 标题行放大加粗；粗体/斜体/删除线/行内代码/链接用字符格式渲染；
// - 语法标记在非光标行隐藏（1pt 透明）；
// - 围栏代码块状态通过 blockState 传播（编辑后局部重刷也正确）；
// - 任务列表 [ ] / [x] 与表格管道符着色。
// 说明：md4c 回调不携带位置信息，故此处使用规则匹配而非 AST。
class MarkdownLiveHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit MarkdownLiveHighlighter(QTextDocument* document);

    // 编辑器在光标/选区变化时调用：范围内的行显示原始 Markdown，
    // 其余行渲染。这比在高亮器里读 currentBlock() 可靠——后者是
    // "最后渲染的块"，与光标位置无关。
    void setActiveRange(int firstLine, int lastLine);
    // 主题形态：给 h1/h2 加下划线（Typora 风标题装饰）
    void setHeadingUnderline(bool on);
    // 需要折叠成覆盖层的表格区域（光标所在的区域由编辑器排除）
    void setHiddenRegions(const QVector<markdown::TableRegion>& regions);

protected:
    void highlightBlock(const QString& text) override;
    using QSyntaxHighlighter::rehighlightBlock;

private:
    void hideSpan(int start, int length);
    // 用固定行高精确控制折叠块高度（字号方案有换算舍入，会与覆盖层不等高）。
    // 必须在 highlightBlock 之外调用：修改块格式会触发 contentsChange 递归。
    static void setBlockHeight(QTextBlock& block, qreal heightPx);

public:
    // 应用折叠区行高（预留时机；见 .cpp 中的说明——当前方案无需改块格式）
    void applyFoldHeights();
    void highlightInline(const QString& text, bool active);
    void highlightTable(const QString& text);

    int m_activeFirst = 0;
    int m_activeLast = 0;
    QVector<markdown::TableRegion> m_hiddenRegions;

    bool m_headingUnderline = false;
    QRegularExpression m_headingRegex;
    QRegularExpression m_listRegex;
    QRegularExpression m_taskRegex;
    QRegularExpression m_tableSeparatorRegex;
    QRegularExpression m_boldItalicRegex;
    QRegularExpression m_boldRegex;
    QRegularExpression m_italicRegex;
    QRegularExpression m_strikeRegex;
    QRegularExpression m_codeSpanRegex;
    QRegularExpression m_linkRegex;
};

} // namespace markdown
