// Hutaomu Editor - Viewer tab integration regression (null-deref crash).
// SPDX-License-Identifier: LicenseRef-Proprietary
//
// 回归背景：查看器标签页（PDF/图片/Office/媒体）在 m_tabs 中不是
// CodeEditor。所有遍历标签页的代码（applyMarkdownViewMode/字体/主题/
// 换行/关闭）都必须跳过它们——此前 editorAt(i) 返回 nullptr 后被直接
// 解引用，打开 PDF 切标签即崩溃。
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTextEdit>
#include <cstdio>

#include "app/MainWindow.h"
#include "viewers/DocumentViewer.h"

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

    // 准备各类文件
    const QString mdPath = dir.filePath(QStringLiteral("doc.md"));
    {
        QFile f(mdPath);
        f.open(QIODevice::WriteOnly);
        f.write("# Title\n\ntext\n");
        f.close();
    }
    const QString pdfPath = dir.filePath(QStringLiteral("doc.pdf"));
    {
        QFile f(pdfPath);
        f.open(QIODevice::WriteOnly);
        // 最小可解析 PDF（含一个内容流）
        f.write("%PDF-1.7\n1 0 obj\n<< /Length 20 >>\nstream\n"
                "BT (Hello) Tj ET\n\nendstream\nendobj\n%%EOF\n");
        f.close();
    }
    const QString pngPath = dir.filePath(QStringLiteral("img.png"));
    QImage(32, 32, QImage::Format_RGB32).fill(Qt::blue);
    QImage(32, 32, QImage::Format_RGB32).save(pngPath);
    const QString csvPath = dir.filePath(QStringLiteral("sheet.csv"));
    {
        QFile f(csvPath);
        f.open(QIODevice::WriteOnly);
        f.write("a,b\n1,2\n");
        f.close();
    }

    app::MainWindow window;
    window.show();
    pump(200);

    // --- 1. 打开 PDF（崩溃现场）---
    window.openPath(pdfPath);
    pump(300);
    expect(true, "opening a pdf does not crash");

    // --- 2. 打开 markdown 后切回 PDF 标签（触发 applyMarkdownViewMode 遍历）---
    window.openPath(mdPath);
    pump(300);
    window.openPath(pdfPath);
    pump(300);
    expect(true, "switching markdown <-> pdf tabs does not crash");

    // --- 3. 图片 + CSV 标签混合后遍历 ---
    window.openPath(pngPath);
    pump(200);
    window.openPath(csvPath);
    pump(200);
    window.openPath(mdPath);
    pump(300);
    expect(true, "mixed viewer/editor tabs iterate without crash");

    // --- 4. 主题切换（遍历标签页 applyTheme + 大纲/预览刷新）---
    // 通过菜单动作触发（公开路径）：视图 -> 切换主题
    {
        QAction* themeAction = nullptr;
        for (QAction* action : window.findChildren<QAction*>()) {
            if (action->text().contains(QStringLiteral("主题"))) {
                themeAction = action;
                break;
            }
        }
        if (themeAction) {
            emit themeAction->triggered();
            pump(300);
        }
    }
    expect(true, "theme toggle over mixed tabs does not crash");

    // --- 5. 在查看器标签激活时关闭它（closeTab 查看器分支）---
    {
        auto* tabs = window.findChild<QTabWidget*>();
        int pdfTab = -1;
        for (int i = 0; i < tabs->count(); ++i) {
            auto* v = qobject_cast<viewers::DocumentViewer*>(tabs->widget(i));
            if (v && v->filePath().endsWith(QStringLiteral("doc.pdf"))) {
                pdfTab = i;
                break;
            }
        }
        expect(pdfTab >= 0, "pdf viewer tab found");
        if (pdfTab >= 0) {
            tabs->setCurrentIndex(pdfTab);
            pump(150);
            emit tabs->tabCloseRequested(pdfTab);
            pump(300);
            expect(true, "closing pdf viewer tab does not crash");
        }
    }

    // --- 6. 全部关闭后仍有编辑器（newDocument 兜底）---
    {
        auto* tabs = window.findChild<QTabWidget*>();
        while (tabs->count() > 1) {
            emit tabs->tabCloseRequested(tabs->count() - 1);
            pump(200);
        }
        expect(tabs->count() >= 1, "at least one tab remains after closing all");
    }

    if (failures > 0) {
        std::printf("\n%d failure(s)\n", failures);
        return 1;
    }
    std::printf("\nAll viewer tab integration tests passed.\n");
    return 0;
}