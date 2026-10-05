// Hutaomu Editor - Comment toggling unit tests.
// SPDX-License-Identifier: LicenseRef-Proprietary
#include <QApplication>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <cstdio>

#include "editor/EditorCommands.h"
#include "syntax/LanguageRegistry.h"

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

QString runToggle(const QString& content, int line)
{
    QPlainTextEdit editor;
    editor.setPlainText(content);
    QTextCursor cursor = editor.textCursor();
    const QTextBlock block = editor.document()->findBlockByNumber(line);
    cursor.setPosition(block.position() + 2);
    editor.setTextCursor(cursor);
    editor::commands::toggleLineComment(&editor,
                                        syntax::LanguageRegistry::instance().findById("cpp"));
    return editor.toPlainText();
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    auto& registry = syntax::LanguageRegistry::instance();

    // --- C++ 行注释：添加 ---
    {
        const QString result = runToggle(
            "int a = 1;\nint b = 2;\nint c = 3;\n", 1);
        expect(result == "int a = 1;\n// int b = 2;\nint c = 3;\n",
               "C++ line comment added");
    }

    // --- C++ 行注释：移除 ---
    {
        const QString result = runToggle(
            "int a = 1;\n// int b = 2;\nint c = 3;\n", 1);
        expect(result == "int a = 1;\nint b = 2;\nint c = 3;\n",
               "C++ line comment removed");
    }

    // --- 缩进行保持缩进 ---
    {
        QPlainTextEdit editor;
        editor.setPlainText("void f() {\n    int deep = 0;\n}\n");
        QTextCursor cursor = editor.textCursor();
        const QTextBlock block = editor.document()->findBlockByNumber(1);
        cursor.setPosition(block.position() + 8);
        editor.setTextCursor(cursor);
        editor::commands::toggleLineComment(&editor, registry.findById("cpp"));
        expect(editor.toPlainText() == "void f() {\n    // int deep = 0;\n}\n",
               "comment keeps indentation");
    }

    // --- 多行选区一起切换 ---
    {
        QPlainTextEdit editor;
        editor.setPlainText("int a;\nint b;\nint c;\n");
        QTextCursor cursor = editor.textCursor();
        const QTextBlock b1 = editor.document()->findBlockByNumber(0);
        const QTextBlock b3 = editor.document()->findBlockByNumber(2);
        cursor.setPosition(b1.position() + 1);
        cursor.setPosition(b3.position() + 1, QTextCursor::KeepAnchor);
        editor.setTextCursor(cursor);
        editor::commands::toggleLineComment(&editor, registry.findById("cpp"));
        expect(editor.toPlainText() == "// int a;\n// int b;\n// int c;\n",
               "multi-line selection all commented");
    }

    // --- Python 用 # ---
    {
        QPlainTextEdit editor;
        editor.setPlainText("x = 1\ny = 2\n");
        QTextCursor cursor = editor.textCursor();
        const QTextBlock block = editor.document()->findBlockByNumber(1);
        cursor.setPosition(block.position());
        editor.setTextCursor(cursor);
        editor::commands::toggleLineComment(&editor, registry.findById("python"));
        expect(editor.toPlainText() == "x = 1\n# y = 2\n",
               "python uses # token");
    }

    // --- HTML 块注释（选中整行内容后包裹） ---
    {
        QPlainTextEdit editor;
        editor.setPlainText("<p>hi</p>\n");
        QTextCursor cursor = editor.textCursor();
        cursor.setPosition(0);
        cursor.setPosition(9, QTextCursor::KeepAnchor); // 选中 "<p>hi</p>"
        editor.setTextCursor(cursor);
        editor::commands::toggleBlockComment(&editor, registry.findById("html"));
        expect(editor.toPlainText() == "<!--<p>hi</p>-->\n",
               "html block comment wraps selection");
    }

    // --- 已包裹的块注释 -> 解开 ---
    {
        QPlainTextEdit editor;
        editor.setPlainText("<!-- <p>hi</p> -->\n");
        QTextCursor cursor = editor.textCursor();
        cursor.setPosition(0);
        cursor.setPosition(18, QTextCursor::KeepAnchor); // 选中整行 "<!-- <p>hi</p> -->"
        editor.setTextCursor(cursor);
        editor::commands::toggleBlockComment(&editor, registry.findById("html"));
        expect(editor.toPlainText() == " <p>hi</p> \n",
               "html block comment unwraps");
    }

    // --- 无选区时的 Markdown 包裹 ---
    {
        QPlainTextEdit editor;
        editor.setPlainText("word \n");
        QTextCursor cursor = editor.textCursor();
        cursor.setPosition(2);
        editor.setTextCursor(cursor);
        editor::commands::wrapSelection(&editor, "**");
        expect(editor.toPlainText().contains("****"),
               "markdown wrap inserts markers");
    }

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll comment tests passed.\n");
    return 0;
}
