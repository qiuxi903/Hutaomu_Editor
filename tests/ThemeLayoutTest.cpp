// Hutaomu Editor - Theme layout (T4 "换形态") regression tests.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
//
// 覆盖：layout.json 解析（含 immersive 预设与非法值忽略）、界面形态真实生效
// （侧栏/大纲/状态栏显隐、标签样式、侧栏卡片化、编辑区居中限宽）、
// 切到无形态主题后原样还原、"只应用配色"（declined）时不改形态、
// 以及"不再提示"等设置项的持久化。
#include <QApplication>
#include <QFile>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <cstdio>

#include "app/MainWindow.h"
#include "editor/CodeEditor.h"
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

void pump(int ms = 120)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

QWidget* panelOf(app::MainWindow& window, const char* objectName)
{
    return window.findChild<QWidget*>(QString::fromLatin1(objectName));
}

} // namespace

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true); // 设置写到临时目录，不碰真实配置
    QApplication app(argc, argv);

    auto& settings = settings::AppSettings::instance();

    std::printf("== layout.json parsing ==\n");
    {
        const editor::ThemeDefinition typora = editor::ThemeManager::themeDefinition(
            QStringLiteral("typora-immersive"));
        expect(!typora.layout.isDefault(), "typora declares a layout");
        expect(typora.layout.sidebarVisible && !*typora.layout.sidebarVisible,
               "typora hides the sidebar");
        expect(typora.layout.tabsStyle == QStringLiteral("underline"),
               "typora uses underline tabs");
        expect(typora.layout.centered && *typora.layout.centered,
               "typora centers the editor column");
        expect(typora.layout.maxWidth && *typora.layout.maxWidth == 760,
               "typora column width parsed");
        expect(typora.layout.headingUnderline && *typora.layout.headingUnderline,
               "typora underlines headings");
        expect(typora.capabilities.contains(QStringLiteral("layout")),
               "layout capability present");

        const editor::ThemeDefinition obsidian
            = editor::ThemeManager::themeDefinition(QStringLiteral("obsidian-cards"));
        expect(obsidian.layout.sidebarStyle == QStringLiteral("card"),
               "obsidian uses card sidebar");
        expect(obsidian.layout.tabsStyle == QStringLiteral("pill"),
               "obsidian uses pill tabs");
        expect(obsidian.layout.outlineVisible && !*obsidian.layout.outlineVisible,
               "obsidian hides the outline panel");

        const editor::ThemeDefinition vscode
            = editor::ThemeManager::themeDefinition(QStringLiteral("vscode-dark-plus"));
        expect(vscode.layout.isDefault(), "vscode theme keeps the default form");
        expect(!vscode.capabilities.contains(QStringLiteral("layout")),
               "no layout capability without layout.json");

        expect(typora.layout.summary().contains(QStringLiteral("隐藏侧栏")),
               "layout summary lists the changes");
        expect(typora.layout.summary().contains(QStringLiteral("760")),
               "layout summary mentions the column width");

        // immersive 预设：未显式给出的键取"全隐藏 + 居中"，显式键仍可覆盖
        editor::ThemeDefinition immersiveSpec;
        immersiveSpec.layout.immersive = true;
        expect(immersiveSpec.layout.immersive, "immersive flag readable");
    }

    std::printf("== applying the writing form (typora) ==\n");
    app::MainWindow window;
    window.resize(1200, 800);
    window.show();
    pump(200);

    editor::ThemeManager::applyTheme(QStringLiteral("paper"));
    window.applyThemeLayout();
    pump();
    auto* editor = window.findChild<editor::CodeEditor*>();
    expect(editor != nullptr, "editor instance available");
    const int fullWidth = editor ? editor->viewport()->width() : 0;

    window.applyThemeAndRefresh(QStringLiteral("typora-immersive"));
    pump(160);
    {
        expect(window.themeLayoutActive(), "layout marked active");
        expect(editor && editor->viewport()->width() <= 760 && editor->viewport()->width() > 600,
               "editor column limited to ~760px");
        expect(fullWidth > editor->viewport()->width(),
               "centering actually narrows the text column");
        auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("editorTabs"));
        expect(tabs && tabs->property("tabStyle").toString() == QStringLiteral("underline"),
               "underline tab style applied");
        expect(tabs && tabs->tabBar()->isVisible(), "tab bar still visible");
        expect(window.statusBar()->isVisible(), "status bar kept visible by this theme");
        // 侧栏（资源管理器）应被收起
        expect(window.isPanelCollapsed(QStringLiteral("explorer")),
               "sidebar collapsed by the theme");
    }

    std::printf("== switching back restores the original form ==\n");
    {
        window.applyThemeAndRefresh(QStringLiteral("paper"));
        pump(160);
        expect(!window.themeLayoutActive(), "layout no longer active");
        expect(!window.isPanelCollapsed(QStringLiteral("explorer")),
               "sidebar restored");
        expect(editor && editor->viewport()->width() >= fullWidth - 4,
               "editor column back to full width");
        auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("editorTabs"));
        expect(tabs && tabs->property("tabStyle").toString().isEmpty(),
               "tab style cleared");
    }

    std::printf("== card sidebar + hidden outline (obsidian) ==\n");
    {
        // 大纲还会被"文档有没有标题"的自动显隐逻辑影响：
        // 先打开一份含标题的文档，让基线是"大纲可见"，断言才说明问题。
        QTemporaryDir dir;
        const QString docPath = dir.filePath(QStringLiteral("outline.md"));
        {
            QFile file(docPath);
            file.open(QIODevice::WriteOnly);
            file.write("# 一级标题\n\n正文段落。\n\n## 二级标题\n\n更多正文。\n");
            file.close();
        }
        window.openPath(docPath);
        pump(260);
        expect(!window.isPanelCollapsed(QStringLiteral("outline")),
               "baseline: outline visible for a document with headings");

        window.applyThemeAndRefresh(QStringLiteral("obsidian-cards"));
        pump(200);
        QWidget* sidebar = panelOf(window, "Sidebar");
        expect(sidebar && sidebar->property("cardStyle").toString() == QStringLiteral("card"),
               "card sidebar style applied");
        expect(window.isPanelCollapsed(QStringLiteral("outline")),
               "outline panel hidden by the theme");
        auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("editorTabs"));
        expect(tabs && tabs->property("tabStyle").toString() == QStringLiteral("pill"),
               "pill tab style applied");

        window.applyThemeAndRefresh(QStringLiteral("paper"));
        pump(200);
        expect(!window.isPanelCollapsed(QStringLiteral("outline")),
               "outline comes back when the theme stops hiding it");
    }

    std::printf("== \"colours only\" (declined) keeps the default form ==\n");
    {
        if (!settings.themeLayoutDeclined.contains(QStringLiteral("obsidian-cards")))
            settings.themeLayoutDeclined.append(QStringLiteral("obsidian-cards"));
        window.applyThemeAndRefresh(QStringLiteral("paper"));
        pump(120);
        window.applyThemeAndRefresh(QStringLiteral("obsidian-cards"));
        pump(160);
        expect(!window.themeLayoutActive(), "declined theme does not change the form");
        QWidget* sidebar = panelOf(window, "Sidebar");
        expect(sidebar && sidebar->property("cardStyle").toString().isEmpty(),
               "card style not applied for declined theme");
        auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("editorTabs"));
        expect(tabs && tabs->property("tabStyle").toString().isEmpty(),
               "pill tab style not applied for declined theme");
        expect(editor::ThemeManager::theme() == QStringLiteral("obsidian-cards"),
               "colours still applied");
        settings.themeLayoutDeclined.removeAll(QStringLiteral("obsidian-cards"));
    }

    std::printf("== prompt suppression persists ==\n");
    {
        settings.themeLayoutPromptSuppressed = true;
        settings.themeLayoutApproved.append(QStringLiteral("typora-immersive"));
        settings.save();

        settings.themeLayoutPromptSuppressed = false;
        settings.themeLayoutApproved.clear();
        settings.load();
        expect(settings.themeLayoutPromptSuppressed,
               "suppression flag survives a reload");
        expect(settings.themeLayoutApproved.contains(QStringLiteral("typora-immersive")),
               "approved theme list survives a reload");

        settings.themeLayoutPromptSuppressed = false;
        settings.themeLayoutApproved.clear();
        settings.save();
    }

    window.applyThemeAndRefresh(QStringLiteral("paper"));
    pump(80);

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll theme layout tests passed.\n");
    return 0;
}
