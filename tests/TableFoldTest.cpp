// Hutaomu Editor - Live-mode table fold geometry regression tests.
// SPDX-License-Identifier: LicenseRef-Proprietary
//
// 回归背景：折叠表格曾与 Qt 真实排版脱节（隐藏行 1pt 塌缩、覆盖层按
// 平行模型定位），多表格 + 标题混排时覆盖层压住表格上方的标题/代码块。
// 这里验证核心不变量：覆盖层 == 真实布局中折叠区的位置和高度。
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <algorithm>
#include <cstdio>

#include "editor/CodeEditor.h"

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

// 折叠高度应用是异步的（防抖扫描 + contentsChange 再触发），轮询至稳定
void settle()
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 900)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 40);
}

class FoldTestEditor : public editor::CodeEditor {
public:
    using editor::CodeEditor::CodeEditor;
    qreal topOf(int line)
    {
        const QTextBlock block = document()->findBlockByNumber(line);
        return blockBoundingGeometry(block).translated(contentOffset()).top();
    }
    qreal heightOf(int line) { return blockBoundingRect(document()->findBlockByNumber(line)).height(); }
};

QVector<QTextBrowser*> overlays(const FoldTestEditor& editor)
{
    auto list = editor.viewport()->findChildren<QTextBrowser*>(
        QStringLiteral("tableOverlay"));
    std::sort(list.begin(), list.end(), [](const QWidget* a, const QWidget* b) {
        return a->y() < b->y();
    });
    return list;
}

// 文档（0-based 行号）：两个表格分别夹在标题之间，下方各跟真实内容
const char* kDoc =
    "## Heading A\n"
    "\n"
    "| a | b |\n"
    "|---|---|\n"
    "| 1 | 2 |\n"
    "| 3 | 4 |\n"
    "\n"
    "### Heading B\n"
    "\n"
    "| c | d |\n"
    "|---|---|\n"
    "| 5 | 6 |\n"
    "\n"
    "tail text\n";

} // namespace

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    FoldTestEditor editor;
    editor.resize(900, 600);
    editor.setEditorFont(QApplication::font());
    editor.show();
    editor.setPlainText(QString::fromLatin1(kDoc));
    editor.setLiveMarkdown(true);
    settle();

    const qreal naturalLine = editor.heightOf(1); // 空行 = 普通行自然高度

    // --- 基本折叠几何：覆盖层与真实布局对齐，且不压住下方内容 ---
    {
        const auto list = overlays(editor);
        expect(list.size() == 2, "two table overlays created");

        const QTextBrowser* first = list.value(0);
        const QTextBrowser* second = list.value(1);
        expect(first && qAbs(first->y() - editor.topOf(2)) <= 3.0,
               "overlay 1 sits exactly at its table's first line");
        expect(second && qAbs(second->y() - editor.topOf(9)) <= 3.0,
               "overlay 2 sits exactly at its table's first line");

        // 回归核心：折叠区在真实排版里占满覆盖层高度——折叠后第一块内容
        // （行 6 空行 / 行 12 空行）从覆盖层底边开始，后续标题不被遮挡
        expect(first && qAbs(editor.topOf(6) - (first->y() + first->height())) <= 3.0,
               "content after fold 1 starts at overlay bottom (no overlap)");
        expect(second && qAbs(editor.topOf(12) - (second->y() + second->height())) <= 3.0,
               "content after fold 2 starts at overlay bottom (no overlap)");
        expect(first && editor.topOf(7) >= first->y() + first->height() - 1.0,
               "Heading B is fully below overlay 1");
        expect(second && editor.topOf(13) >= second->y() + second->height() - 1.0,
               "tail text is fully below overlay 2");

        // 表格全览：覆盖层内容不得被裁剪（渲染内容高度 <= 覆盖层高度）
        for (const QTextBrowser* ov : list) {
            if (!ov->isVisible())
                continue;
            expect(ov->document()->size().height() <= ov->height() + 1.5,
                   "overlay table fully visible (no clipped rows)");
        }
    }

    // --- 光标进入表格：整块展开为源码，覆盖层隐藏、其余表格仍对齐 ---
    {
        QTextCursor cursor(editor.document()->findBlockByNumber(2));
        editor.setTextCursor(cursor);
        settle();

        const auto list = overlays(editor);
        const QTextBrowser* first = list.value(0);
        const QTextBrowser* second = list.value(1);
        expect(first && !first->isVisible(), "active table overlay hidden");
        expect(editor.heightOf(2) <= naturalLine + 2.0,
               "expanded table rows restored to natural line height");
        expect(second && qAbs(second->y() - editor.topOf(9)) <= 3.0,
               "other overlay re-aligned after fold above expanded");
    }

    // --- 光标离开：重新折叠，几何恢复 ---
    {
        QTextCursor cursor(editor.document()->findBlockByNumber(0));
        editor.setTextCursor(cursor);
        settle();

        const auto list = overlays(editor);
        const QTextBrowser* first = list.value(0);
        const QTextBrowser* second = list.value(1);
        expect(first && first->isVisible(),
               "table folds again after cursor leaves");
        expect(first && qAbs(first->y() - editor.topOf(2)) <= 3.0,
               "refolded overlay 1 realigned");
        expect(first && qAbs(editor.topOf(6) - (first->y() + first->height())) <= 3.0,
               "fold 1 height re-reserved after refold");
        expect(second && qAbs(second->y() - editor.topOf(9)) <= 3.0,
               "overlay 2 still aligned after refold");
    }

    // --- 拖选：选区触及 -> 表格变文本；选区退出 -> 立即恢复；边缘无抖动 ---
    {
        const qreal foldBefore = editor.topOf(7) - editor.topOf(2);
        const qreal zoneBottom = editor.topOf(2) + (foldBefore - naturalLine) + 6;

        auto sendButton = [&editor](QEvent::Type type, Qt::MouseButton button,
                                    Qt::MouseButtons buttons) {
            QMouseEvent ev(type, QPointF(400, 300), QPointF(400, 300), button,
                           buttons, Qt::NoModifier);
            QApplication::sendEvent(editor.viewport(), &ev);
            QCoreApplication::processEvents();
        };
        auto dragMove = [&editor](qreal y) {
            QMouseEvent ev(QEvent::MouseMove, QPointF(400, y), QPointF(400, y),
                           Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(editor.viewport(), &ev);
            QCoreApplication::processEvents();
        };
        // 模拟拖选：锚点行 anchorLine 到当前行 toLine 的选区
        auto dragSelect = [&editor](int anchorLine, int toLine) {
            QTextCursor c(editor.document()->findBlockByNumber(anchorLine));
            c.movePosition(QTextCursor::StartOfBlock);
            const QTextBlock endBlock = editor.document()->findBlockByNumber(toLine);
            c.setPosition(endBlock.position(), QTextCursor::KeepAnchor);
            editor.setTextCursor(c);
            QApplication::processEvents();
        };

        sendButton(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);

        // 1) 选区伸入表格 -> 展开为文本
        dragMove(editor.topOf(2) + 10); // 鼠标置于表格足迹内
        dragSelect(0, 3);
        expect(editor.heightOf(4) <= naturalLine + 2.0,
               "selection touching table expands it to source text");

        // 2) 返回：选区缩回表格上方、鼠标在表格上方 -> 立即恢复表格
        dragMove(editor.topOf(2) - 40);
        dragSelect(0, 1);
        expect(editor.heightOf(4) > naturalLine + 2.0,
               "table restores immediately when selection no longer covers it");

        // 3) 边缘防抖：选区覆盖展开后，光标跳出到表格之后、鼠标仍在足迹内
        //    -> 保持展开（同一鼠标位置在两个状态下映射不同，按足迹判定防抖动）
        dragMove(editor.topOf(2) + 10);
        dragSelect(0, 8);
        expect(editor.heightOf(4) <= naturalLine + 2.0,
               "re-expands while selection covers table");
        dragSelect(8, 9);
        expect(editor.heightOf(4) <= naturalLine + 2.0,
               "no flicker while cursor sweeps across table edge (mouse inside footprint)");

        // 4) 鼠标移出足迹 -> 立即折叠
        dragMove(zoneBottom + 40);
        dragSelect(8, 10);
        expect(editor.heightOf(4) > naturalLine + 2.0,
               "collapses as soon as the mouse leaves the table footprint");

        // 5) 释放：布局随后完全静止
        sendButton(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
        const qreal settled = editor.topOf(7) - editor.topOf(2);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        expect(qAbs((editor.topOf(7) - editor.topOf(2)) - settled) < 0.5,
               "layout stable after release");
    }

    // --- 大纲跳转：目标标题落在视口中部（长文档 + 折叠表格的非线性映射） ---
    {
        FoldTestEditor tall;
        tall.resize(900, 600);
        tall.show();
        QString big = QStringLiteral("para 0\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n");
        for (int i = 1; i <= 30; ++i)
            big += QStringLiteral("para %1\n\n").arg(i);
        big += QStringLiteral("## Target Heading\n\ntext\n\n");
        for (int i = 31; i <= 60; ++i)
            big += QStringLiteral("para %1\n\n").arg(i);
        big += QStringLiteral("tail\n");
        int targetLine0 = 0;
        {
            const QStringList ls = big.split(QLatin1Char('\n'));
            for (int i = 0; i < ls.size(); ++i)
                if (ls.at(i) == QStringLiteral("## Target Heading"))
                    targetLine0 = i;
        }
        tall.setPlainText(big);
        tall.setLiveMarkdown(true);
        settle();

        tall.jumpToLine(targetLine0 + 1); // 大纲传 1-based 行号
        QElapsedTimer wait;
        wait.start();
        while (wait.elapsed() < 500)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 30);

        const qreal y = tall.topOf(targetLine0);
        const qreal vh = tall.viewport()->height();
        expect(y > vh * 0.25 && y < vh * 0.75,
               "outline jump lands heading near viewport center");
    }

    // --- 退出实时模式：覆盖层全部隐藏，行高回到源码排版 ---
    {
        editor.setLiveMarkdown(false);
        QCoreApplication::processEvents();

        bool anyVisible = false;
        const auto list = editor.viewport()->findChildren<QTextBrowser*>(
            QStringLiteral("tableOverlay"));
        for (const QTextBrowser* overlay : list)
            anyVisible |= overlay->isVisible();
        expect(!anyVisible, "all overlays hidden after live mode off");
        expect(qAbs(editor.heightOf(4) - naturalLine) <= 2.0,
               "table rows back to natural height in source mode");
    }

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll table fold tests passed.\n");
    return 0;
}
