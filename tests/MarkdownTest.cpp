// Hutaomu Editor - Markdown feature tests (live highlighter, outline, renderer).
// SPDX-License-Identifier: LicenseRef-Proprietary
#include <QApplication>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <cstdio>

#include "markdown/MarkdownLiveHighlighter.h"
#include "markdown/MarkdownRenderer.h"
#include "panels/OutlinePanel.h"

namespace {

int failures = 0;

void expect(bool condition, const char* what)
{
    if (condition)
        std::printf("  PASS  %s\n", what);
    else {
        std::printf("  FAIL  %s\n", what);
        ++failures;
    }
}

int blockState(QPlainTextEdit& editor, int line)
{
    return editor.document()->findBlockByNumber(line).userState();
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // --- 围栏状态在插入文本后仍正确传播 ---
    {
        QPlainTextEdit editor;
        markdown::MarkdownLiveHighlighter highlighter(editor.document());
        editor.setPlainText("# t\n```cpp\nint x;\n```\nafter\n");
        QCoreApplication::processEvents();

        expect(blockState(editor, 0) == 0 && blockState(editor, 1) == 1
                   && blockState(editor, 2) == 1 && blockState(editor, 3) == 0
                   && blockState(editor, 4) == 0,
               "fence states correct on load");

        // 在围栏前插入一行，围栏整体后移
        QTextCursor cursor(editor.document());
        cursor.insertText("extra\n");
        QCoreApplication::processEvents();

        expect(blockState(editor, 0) == 0 && blockState(editor, 1) == 0
                   && blockState(editor, 2) == 1 && blockState(editor, 3) == 1
                   && blockState(editor, 4) == 0 && blockState(editor, 5) == 0,
               "fence states propagate after edit above fence");
    }

    // --- 围栏内的 # 与 ~~~ 不被误判 ---
    {
        QPlainTextEdit editor;
        markdown::MarkdownLiveHighlighter highlighter(editor.document());
        editor.setPlainText("```md\n# not a heading\n~~~\n```\n# real\n");
        QCoreApplication::processEvents();

        expect(blockState(editor, 0) == 1 && blockState(editor, 1) == 1
                   && blockState(editor, 2) == 1 && blockState(editor, 3) == 0
                   && blockState(editor, 4) == 0,
               "nested fence characters inside code stay in fence");
    }

    // --- 大纲：跳过代码围栏内的 # ---
    {
        panels::OutlinePanel outline;
        outline.updateFromText("## A\n```\n# fake\n```\n### B\n");
        expect(outline.count() == 2, "outline ignores headings inside fences");
        expect(outline.lineNumberAt(0) == 1 && outline.lineNumberAt(1) == 5,
               "outline maps headings to source lines");
    }

    // --- 渲染器：任务列表 / 表格 / 标题锚点 ---
    {
        const auto result = markdown::renderToHtml("- [x] 已完成\n- [ ] 待办\n");
        expect(result.html.contains(QStringLiteral("&#9745;")), "checked task glyph");
        expect(result.html.contains(QStringLiteral("&#9744;")), "unchecked task glyph");
    }
    {
        const auto result = markdown::renderToHtml("| a | b |\n|---|---:|\n| 1 | 2 |\n");
        expect(result.html.contains(QStringLiteral("<table border=")), "table borders");
        expect(result.html.contains(QStringLiteral("right")), "table column alignment");
    }
    {
        const auto result = markdown::renderToHtml("# One\n\ntext\n\n## Two\n");
        expect(result.html.contains(QStringLiteral("name=\"sec-0\""))
                   && result.html.contains(QStringLiteral("name=\"sec-1\"")),
               "heading anchors in document order");
    }

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll markdown tests passed.\n");
    return 0;
}
