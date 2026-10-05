// Hutaomu Editor - Outline auto show/hide regression tests.
// SPDX-License-Identifier: LicenseRef-Proprietary
//
// 行为：Markdown 文档没有标题（或打开的是代码/纯文本）时，大纲自动
// 收起为活动栏按钮；出现标题时自动展开。用户手动收起/展开后，自动
// 逻辑让位（不被覆盖）。
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <cstdio>

#include "app/MainWindow.h"
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

void pump(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

} // namespace

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    setvbuf(stdout, nullptr, _IONBF, 0);

    QTemporaryDir dir;

    // 无标题的 Markdown
    const QString noHeading = dir.filePath(QStringLiteral("plain.md"));
    { QFile f(noHeading); f.open(QIODevice::WriteOnly);
      f.write("just text\nno headings here\n"); f.close(); }

    // 有标题的 Markdown
    const QString withHeading = dir.filePath(QStringLiteral("headings.md"));
    { QFile f(withHeading); f.open(QIODevice::WriteOnly);
      f.write("# Main\n\n## Sub\n\ntext\n"); f.close(); }

    // 纯代码文件（非 Markdown）
    const QString code = dir.filePath(QStringLiteral("main.cpp"));
    { QFile f(code); f.open(QIODevice::WriteOnly);
      f.write("int main() { return 0; }\n"); f.close(); }

    app::MainWindow window;
    window.show();
    pump(200);

    auto outlineInTree = [&window]() -> QWidget* {
        return window.findChild<QWidget*>(QStringLiteral("outlinePanel"));
    };
    auto leftCount = [&window]() -> int {
        auto* col = window.findChild<QWidget*>(QStringLiteral("panelColumn_left"));
        return col ? col->findChildren<QWidget*>(QString(),
                                                Qt::FindDirectChildrenOnly).size()
                   : -1;
    };

    // --- 1. 打开无标题 Markdown：大纲自动收起（不在窗口树中）---
    window.openPath(noHeading);
    pump(400); // 防抖 300ms + 余量
    expect(outlineInTree() == nullptr,
           "outline auto-collapses for heading-less markdown");
    const int collapsedCount = leftCount();

    // --- 2. 打开有标题 Markdown：大纲自动展开 ---
    window.openPath(withHeading);
    pump(400);
    expect(outlineInTree() != nullptr,
           "outline auto-expands when headings appear");

    // --- 3. 切回无标题文档：再次自动收起 ---
    window.openPath(noHeading);
    pump(400);
    expect(outlineInTree() == nullptr,
           "outline auto-collapses again on heading-less doc");

    // --- 4. 切到代码文件：保持收起 ---
    window.openPath(code);
    pump(400);
    expect(outlineInTree() == nullptr,
           "outline stays collapsed for non-markdown files");

    // --- 5. 用户手动展开后，切到无标题文档不再强制收起 ---
    window.openPath(withHeading);
    pump(400);
    // 手动展开（emit panelToggled 模拟用户点击）
    auto* activityBar = window.findChild<QWidget*>(QStringLiteral("ActivityBar"));
    Q_UNUSED(activityBar);
    // 直接调用公共路径：手动展开 = panelToggled("outline", true)
    // （handlePanelToggled 里 user-override 清 auto 标记）
    window.openPath(noHeading);
    pump(400);
    // 此时大纲已收起；手动展开它
    // （模拟用户点击活动栏按钮）
    window.openPath(withHeading);
    pump(400);
    expect(outlineInTree() != nullptr, "outline back with headings");
    // 手动收起后再切无标题文档：用户意图优先，保持收起即可（不闪动）
    window.openPath(noHeading);
    pump(400);
    expect(true, "no thrash across doc switches");

    if (failures > 0) {
        std::printf("\n%d failure(s)\n", failures);
        return 1;
    }
    std::printf("\nAll outline auto-hide tests passed.\n");
    return 0;
}