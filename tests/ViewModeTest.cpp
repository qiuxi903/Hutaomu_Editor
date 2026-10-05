// Hutaomu Editor - View mode switching integration test (crash regression).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include <QApplication>
#include <iostream>
#include <QMouseEvent>
#include <QMenuBar>
#include <QAction>
#include <QKeyEvent>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>

#include "app/MainWindow.h"
#include "settings/AppSettings.h"
#include "themes/ThemeManager.h"

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

} // namespace

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // 测试模式下 AppSettings 写到临时目录，不污染真实用户设置
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);

    // 构造内容丰富的 Markdown 文档（表格/任务/代码/引用/中文）
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("rich.md"));
    {
        QFile file(path);
        file.open(QIODevice::WriteOnly);
        file.write("# 标题\n\n## 第一章\n\n- [x] 完成\n- [ ] 待办\n\n"
                   "| a | b |\n|---|---:|\n| 1 | 2 |\n\n"
                   "```cpp\nint main() { return 0; }\n```\n\n"
                   "> 引用\n\n***粗斜体*** 与 [链接](https://x.y)\n\n## 第二章\n");
        file.close();
    }

    app::MainWindow window;
    window.openPath(path);

    // live -> split -> live 往返切换（崩溃回归：切换分栏后崩溃），
    // 每次切换后移动光标：回归"切分栏后一移动光标就崩溃"（悬垂指针）
    auto pressDown = [&window] {
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(window.focusWidget(), &key);
        QCoreApplication::processEvents();
    };
    for (int round = 0; round < 4; ++round) {
        window.setMarkdownViewMode(QStringLiteral("split"));
        QCoreApplication::processEvents();
        pressDown();
        pressDown();
        window.setMarkdownViewMode(QStringLiteral("live"));
        QCoreApplication::processEvents();
        pressDown();
        pressDown();
    }
    expect(true, "live/split round trips with cursor moves without crash");

    // split 状态下编辑文本触发防抖刷新
    window.setMarkdownViewMode(QStringLiteral("split"));
    QCoreApplication::processEvents();

    // 预览位置往返
    window.setPreviewPosition(QStringLiteral("right"));
    window.setPreviewPosition(QStringLiteral("bottom"));
    window.setPreviewPosition(QStringLiteral("right"));
    QCoreApplication::processEvents();
    expect(true, "preview position round trips without crash");

    // 关闭前再切一次模式
    window.setMarkdownViewMode(QStringLiteral("source"));
    QCoreApplication::processEvents();
    window.setMarkdownViewMode(QStringLiteral("split"));
    QCoreApplication::processEvents();

    // 标题栏拖动回归：装饰标签不能吃掉鼠标事件，否则"空白处"其实是标签、
    // 在标题文字上按住拖不动窗口（用户实测反馈的问题）
    {
        auto* titleBar = window.findChild<QWidget*>(QStringLiteral("TitleBar"));
        expect(titleBar != nullptr, "title bar reachable");
        const QStringList decorations = { QStringLiteral("brandIcon"),
                                          QStringLiteral("brandName"),
                                          QStringLiteral("titleLabel") };
        for (const QString& name : decorations) {
            QWidget* label = window.findChild<QWidget*>(name);
            expect(label != nullptr
                       && label->testAttribute(Qt::WA_TransparentForMouseEvents),
                   qPrintable(name + QStringLiteral(": transparent for mouse")));
        }
        // 标题栏中线上任意一点：要么落在 TitleBar 空白（直接拖动），要么落在
        // 鼠标透明的装饰控件（事件穿透到 TitleBar）；菜单栏空白由事件过滤器接管。
        // 这里断言"中线命中项若不是 TitleBar 自身，就必须是透明控件或菜单栏"。
        if (titleBar) {
            const QPoint center(titleBar->width() / 2, titleBar->height() / 2);
            QWidget* hit = titleBar->childAt(center);
            const bool ok = hit == nullptr
                            || hit->testAttribute(Qt::WA_TransparentForMouseEvents)
                            || qobject_cast<QMenuBar*>(hit) != nullptr;
            expect(ok, "title-bar midline is draggable (blank / transparent / menu bar)");
        }
        // 按下事件确实能触发拖动路径（事件被 TitleBar 消费）
        if (titleBar) {
            const QPoint probe(titleBar->width() / 2, titleBar->height() / 2);
            QMouseEvent press(QEvent::MouseButtonPress, probe,
                              titleBar->mapToGlobal(probe), Qt::LeftButton,
                              Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(titleBar, &press);
            expect(press.isAccepted(), "title-bar press enters the drag path");
        }
        // 菜单栏空白处的按下交给拖动（不弹菜单）
        if (auto* menuBar = window.findChild<QMenuBar*>()) {
            int lastRight = 0;
            const auto actions = menuBar->actions();
            for (QAction* action : actions)
                lastRight = qMax(lastRight, menuBar->actionGeometry(action).right());
            if (menuBar->width() - lastRight >= 8) {
                const QPoint pos(qMin(menuBar->width() - 2, lastRight + 4),
                                 menuBar->height() / 2);
                expect(menuBar->actionAt(pos) == nullptr,
                       "probe point is menu-bar empty area");
                QMouseEvent press(QEvent::MouseButtonPress, pos,
                                  menuBar->mapToGlobal(pos), Qt::LeftButton,
                                  Qt::LeftButton, Qt::NoModifier);
                QCoreApplication::sendEvent(menuBar, &press);
                expect(press.isAccepted(),
                       "empty menu-bar press is handed to the window drag");
                expect(menuBar->activeAction() == nullptr,
                       "no menu pops from the empty area");
            }
        }
    }

    // 主题选择回归：设置页选中的主题必须被真正应用（而不是翻转深/浅），
    // 且主题自带的编辑器字体/字号覆盖要一并生效。
    {
        auto& settings = settings::AppSettings::instance();
        editor::ThemeManager::applyTheme(QStringLiteral("paper"));
        settings.theme = QStringLiteral("paper");

        window.applyThemeAndRefresh(QStringLiteral("vscode-dark-plus"));
        expect(editor::ThemeManager::theme() == QStringLiteral("vscode-dark-plus"),
               "selected theme is applied (not dark/light flipped)");
        expect(settings.theme == QStringLiteral("vscode-dark-plus"),
               "settings record the applied theme");

        window.applyThemeAndRefresh(QStringLiteral("typora-immersive"));
        expect(editor::ThemeManager::metricInt(QStringLiteral("treeRowHeight")) == 28,
               "official theme metrics active after switch");
        expect(window.editorFontForTest().pointSize() == 17,
               "theme font size overrides user setting");

        // 不存在的主题：回退默认，并记录实际生效的 id（不留下应用不出来的名字）
        window.applyThemeAndRefresh(QStringLiteral("no-such-theme"));
        expect(editor::ThemeManager::theme() == QStringLiteral("paper"),
               "unknown theme id falls back to default");
        expect(settings.theme == QStringLiteral("paper"),
               "settings record the fallback id, not the bogus one");
    }

    if (failures > 0) {
        std::printf("\n%d failure(s)\n", failures);
        return 1;
    }
    std::printf("\nAll view mode tests passed (no crash).\n");
    return 0;
}
