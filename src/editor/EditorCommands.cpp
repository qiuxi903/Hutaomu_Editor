// Hutaomu Editor - Editor command helpers (comments, markdown wraps).
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "EditorCommands.h"

#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>

#include "syntax/LanguageSpec.h"

namespace editor {
namespace commands {
namespace {

struct LineSpan {
    int first;
    int last;
};

LineSpan selectedLines(const QTextCursor& cursor)
{
    int first = cursor.document()->blockCount() - 1;
    int last = 0;
    if (cursor.hasSelection()) {
        first = cursor.document()->findBlock(cursor.selectionStart()).blockNumber();
        last = cursor.document()->findBlock(cursor.selectionEnd() - (cursor.selectionEnd() > cursor.selectionStart() ? 1 : 0)).blockNumber();
    } else {
        first = last = cursor.block().blockNumber();
    }
    if (last < first)
        std::swap(first, last);
    return { first, last };
}

QTextCursor cursorForLines(QPlainTextEdit* editor, int first, int last)
{
    QTextDocument* doc = editor->document();
    QTextBlock b1 = doc->findBlockByNumber(first);
    QTextBlock b2 = doc->findBlockByNumber(last);
    if (!b1.isValid() || !b2.isValid())
        return QTextCursor();
    QTextCursor cursor(doc);
    cursor.setPosition(b1.position());
    cursor.setPosition(b2.position() + b2.length() - 1, QTextCursor::KeepAnchor);
    return cursor;
}

} // namespace

void toggleLineComment(QPlainTextEdit* editor, const syntax::LanguageSpec* spec)
{
    if (!editor || !spec || !spec->hasLineComment())
        return;

    const QString token = QString::fromUtf8(spec->lineComment);
    QTextCursor cursor = editor->textCursor();

    const LineSpan span = selectedLines(cursor);
    QTextCursor whole = cursorForLines(editor, span.first, span.last);
    if (whole.isNull())
        return;

    // 逐行判断当前状态：全部已注释 -> 取消；否则 -> 添加
    QTextBlock block = editor->document()->findBlockByNumber(span.first);
    QStringList lines;
    for (int n = span.first; n <= span.last; ++n) {
        lines << block.text();
        block = block.next();
    }

    static const QRegularExpression commentPrefix(
        QStringLiteral("^(\\s*)") + QRegularExpression::escape(token) + QStringLiteral(" ?"));

    bool allCommented = true;
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue; // 空行不打断判断
        if (!commentPrefix.match(line).hasMatch()) {
            allCommented = false;
            break;
        }
    }

    QString result;
    result.reserve(lines.size() * (lines.size() < 4 ? 0 : 0) + 16);
    for (const QString& line : lines) {
        if (allCommented) {
            const QRegularExpressionMatch m = commentPrefix.match(line);
            result += m.hasMatch() ? line.mid(m.capturedLength()) : line;
        } else {
            const int indent = line.indexOf(QRegularExpression(QStringLiteral("\\S")));
            if (indent < 0) {
                result += line; // 空行不加注释
            } else {
                result += line.left(indent) + token + QLatin1Char(' ')
                          + line.mid(indent);
            }
        }
        if (&line != &lines.constLast())
            result += QLatin1Char('\n');
    }

    whole.beginEditBlock();
    whole.insertText(result);
    whole.endEditBlock();

    // 保持行选区
    QTextCursor after = cursorForLines(editor, span.first,
                                       span.first + result.count(QLatin1Char('\n')));
    if (!after.isNull())
        editor->setTextCursor(after);
}

void toggleBlockComment(QPlainTextEdit* editor, const syntax::LanguageSpec* spec)
{
    if (!editor || !spec || !spec->hasBlockComment())
        return;

    const QString start = QString::fromUtf8(spec->blockCommentStart);
    const QString end = QString::fromUtf8(spec->blockCommentEnd);
    QTextCursor cursor = editor->textCursor();

    const QString selected = cursor.hasSelection()
                                 ? cursor.selectedText()
                                 : QString();
    const QString joined = QString(selected).replace(QChar(0x2029), QChar('\n'));

    // 已被包裹 -> 解开
    if (cursor.hasSelection() && joined.startsWith(start) && joined.endsWith(end)
        && joined.length() >= start.length() + end.length()) {
        const QString inner = joined.mid(start.length(),
                                         joined.length() - start.length() - end.length());
        cursor.insertText(inner);
        return;
    }

    // 选区或光标所在词内已包含完整注释块 -> 移除
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::WordUnderCursor);
    }
    const QString around = cursor.selectedText().replace(QLatin1Char('\u2029'), QLatin1Char('\n'));
    const int startPos = around.indexOf(start);
    const int endPos = around.lastIndexOf(end);
    if (startPos >= 0 && endPos > startPos) {
        const QString inner = around.left(startPos)
                              + around.mid(startPos + start.length(),
                                           endPos - startPos - start.length())
                              + around.mid(endPos + end.length());
        cursor.insertText(inner);
        return;
    }

    // 包裹
    QTextCursor target = editor->textCursor();
    if (!target.hasSelection())
        target.select(QTextCursor::WordUnderCursor);
    const QString text = target.selectedText().replace(QLatin1Char('\u2029'), QLatin1Char('\n'));
    target.insertText(start + text + end);
}

void wrapSelection(QPlainTextEdit* editor, const QString& marker)
{
    if (!editor)
        return;
    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        const QString selected = cursor.selectedText().replace(QLatin1Char('\u2029'),
                                                               QLatin1Char('\n'));
        cursor.insertText(marker + selected + marker);
    } else {
        cursor.insertText(marker + marker);
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, marker.length());
        editor->setTextCursor(cursor);
    }
}

} // namespace commands
} // namespace editor
