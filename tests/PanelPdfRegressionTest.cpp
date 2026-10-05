// Hutaomu Editor - Panel drag round-trip + PDF page navigation regressions.
// SPDX-License-Identifier: LicenseRef-Proprietary
//
// 回归 1：面板拖到另一侧后必须能拖回来（主分栏方向曾由单个面板的
//         停靠侧决定，导致整条侧栏区被搬走、"拖不回去"）。
// 回归 2：搜索结果面板缺少标题栏把手，无法拖动。
// 回归 3：PDF 翻页崩溃（FPDF_LoadMemDocument 持有的字节是局部变量，
//         函数返回即释放 -> 翻页时读悬空内存）。
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QMouseEvent>
#include <QSplitter>
#include <QTemporaryDir>
#include <cstdio>

#include "app/MainWindow.h"
#include "panels/SearchPanel.h"
#include "settings/AppSettings.h"
#include "viewers/PdfViewer.h"

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

QPoint headerGlobal(QWidget* header)
{
    return header->mapToGlobal(header->rect().center());
}

// 真实鼠标拖拽：按下 -> 移动过阈值 -> 释放到目标
void dragHeader(QWidget* header, const QPoint& toGlobal)
{
    const QPoint from = headerGlobal(header);
    auto send = [&](QEvent::Type type, const QPoint& g, Qt::MouseButton btn,
                    Qt::MouseButtons buttons) {
        QMouseEvent ev(type, header->mapFromGlobal(g), g, btn, buttons,
                       Qt::NoModifier);
        QApplication::sendEvent(header, &ev);
        pump(30);
    };
    send(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    send(QEvent::MouseMove, (from + toGlobal) / 2, Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseMove, toGlobal, Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, toGlobal, Qt::LeftButton, Qt::NoButton);
    pump(120);
}

// 面板当前所在列（"left" / "right" / "collapsed"）
QString sideOf(app::MainWindow& window, const QString& panelObjectName)
{
    struct ColRef {
        QWidget* col;
        const char* name;
    };
    const ColRef cols[] = {
        { window.findChild<QWidget*>(QStringLiteral("panelColumn_left")), "left" },
        { window.findChild<QWidget*>(QStringLiteral("panelColumn_right")), "right" },
    };
    for (const ColRef& ref : cols) {
        if (!ref.col)
            continue;
        for (QWidget* child :
             ref.col->findChildren<QWidget*>(QString(),
                                             Qt::FindDirectChildrenOnly)) {
            if (child->objectName() == panelObjectName
                || child->findChild<QWidget*>(panelObjectName))
                return QString::fromLatin1(ref.name);
        }
    }
    return QStringLiteral("collapsed");
}

} // namespace

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    setvbuf(stdout, nullptr, _IONBF, 0);

    QTemporaryDir dir;

    app::MainWindow window;
    window.show();
    pump(250);

    // ============ 回归 1：面板拖到右侧，再拖回左侧 ============
    {
        auto* explorerHeader =
            window.findChild<QWidget*>(QStringLiteral("sidebarHeader"));
        expect(explorerHeader != nullptr, "explorer header exists");
        if (explorerHeader) {
            const int w = window.width();
            const int y = headerGlobal(explorerHeader).y();
            // 拖到右区
            dragHeader(explorerHeader, QPoint(window.x() + w - 60,
                                              window.y() + window.height() / 2));
            pump(200);
            expect(settings::AppSettings::instance().sidebarPosition
                       == QStringLiteral("right"),
                   "explorer drags to right");
            expect(sideOf(window, QStringLiteral("Sidebar"))
                       == QStringLiteral("right"),
                   "explorer panel actually in right column");

            // 拖回左区（关键回归：曾经拖不回去）
            dragHeader(explorerHeader, QPoint(window.x() + 60,
                                              window.y() + window.height() / 2));
            pump(200);
            expect(settings::AppSettings::instance().sidebarPosition
                       != QStringLiteral("right"),
                   "explorer drags back to left");
            expect(sideOf(window, QStringLiteral("Sidebar"))
                       == QStringLiteral("left"),
                   "explorer panel actually back in left column");
        }
    }

    // ============ 回归 2：搜索面板有拖动把手且能换列 ============
    {
        auto* searchHeader =
            window.findChild<QWidget*>(QStringLiteral("searchHeader"));
        expect(searchHeader != nullptr,
               "search panel has a draggable header");
        if (searchHeader) {
            const int w = window.width();
            dragHeader(searchHeader, QPoint(window.x() + w - 60,
                                            window.y() + window.height() / 2));
            pump(200);
            expect(sideOf(window, QStringLiteral("searchPanel"))
                       == QStringLiteral("right"),
                   "search panel drags to right column");
            // 拖回
            dragHeader(searchHeader, QPoint(window.x() + 60,
                                            window.y() + window.height() / 2));
            pump(200);
            expect(sideOf(window, QStringLiteral("searchPanel"))
                       == QStringLiteral("left"),
                   "search panel drags back to left column");
        }
    }

    // ============ 回归 3：PDF 翻页（悬空内存崩溃） ============
    {
        // 构造两页 PDF
        const QString pdfPath = dir.filePath(QStringLiteral("two_pages.pdf"));
        QFile file(pdfPath);
        file.open(QIODevice::WriteOnly);
        auto pageObj = [](int n) {
            return QStringLiteral(
                       "%1 0 obj\n<< /Type /Page /Parent 2 0 R "
                       "/MediaBox [0 0 612 792] /Contents %2 0 R "
                       "/Resources << /Font << /F1 5 0 R >> >> >>\nendobj\n")
                .arg(n)
                .arg(n + 2);
        };
        file.write(QStringLiteral(
                       "%PDF-1.4\n"
                       "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
                       "2 0 obj\n<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>\nendobj\n"
                       "%1%2"
                       "5 0 obj\n<< /Length 42 >>\nstream\n"
                       "BT /F1 24 Tf 100 700 Td (Page One) Tj ET\nendstream\nendobj\n"
                       "6 0 obj\n<< /Length 42 >>\nstream\n"
                       "BT /F1 24 Tf 100 700 Td (Page Two) Tj ET\nendstream\nendobj\n"
                       "7 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n"
                       "trailer\n<< /Size 8 /Root 1 0 R >>\n%%EOF\n")
                       .arg(pageObj(3), pageObj(4))
                       .toUtf8());
        file.close();

        viewers::PdfViewer viewer(pdfPath);
        auto* label = viewer.findChild<QLabel*>(QStringLiteral("pdfPageLabel"));
        auto* canvas = viewer.findChild<QLabel*>(QStringLiteral("pdfCanvas"));
        expect(label && label->text().contains(QStringLiteral("1 / 2")),
               "pdf shows page 1 of 2");
        expect(canvas && !canvas->pixmap().isNull(),
               "pdf page 1 rendered");

        // 翻到下一页（崩溃现场）
        viewer.zoomIn();
        viewer.zoomOut();
        QMetaObject::invokeMethod(&viewer, "nextPage", Qt::DirectConnection);
        pump(100);
        expect(label && label->text().contains(QStringLiteral("2 / 2")),
               "pdf next page navigates to page 2");
        expect(canvas && !canvas->pixmap().isNull(),
               "pdf page 2 rendered (no crash)");

        // 再翻回来 + 反复翻页（内存悬空问题会在此暴露）
        for (int i = 0; i < 10; ++i) {
            QMetaObject::invokeMethod(&viewer, "previousPage",
                                      Qt::DirectConnection);
            QMetaObject::invokeMethod(&viewer, "nextPage", Qt::DirectConnection);
        }
        pump(100);
        expect(label && label->text().contains(QStringLiteral("2 / 2")),
               "repeated page flips stable");
    }

    // ============ 回归 4：侧边栏宽度稳定（不莫名变大） ============
    {
        auto leftWidth = [&window]() {
            auto* col = window.findChild<QWidget*>(
                QStringLiteral("panelColumn_left"));
            return col ? col->width() : -1;
        };
        auto* splitter = window.findChild<QSplitter*>(
            QStringLiteral("mainSplitter"));
        const int before = leftWidth();
        expect(before > 0, "left column has a width");

        // 反复触发布局（切视图模式 + 改窗口尺寸）
        for (int round = 0; round < 3; ++round) {
            window.setMarkdownViewMode(QStringLiteral("split"));
            pump(80);
            window.setMarkdownViewMode(QStringLiteral("live"));
            pump(80);
            window.resize(window.width() + 80, window.height());
            pump(80);
            window.resize(window.width() - 80, window.height());
            pump(80);
        }
        const int after = leftWidth();
        std::printf("  DBG left col width: before=%d after=%d\n", before, after);
        expect(after <= before + 60,
               "left column width stays stable across layout churn");

        if (splitter) {
            splitter->setSizes({ 280, 900, 1 });
            pump(150);
            expect(qAbs(leftWidth() - 280) <= 20,
                   "explicit setSizes pins column width");
        }
    }

    if (failures > 0) {
        std::printf("\n%d failure(s)\n", failures);
        return 1;
    }
    std::printf("\nAll panel drag + pdf navigation tests passed.\n");
    return 0;
}