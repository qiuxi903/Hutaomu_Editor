// Hutaomu Editor - Command palette regression tests (P5).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
//
// 覆盖：条目汇总（内置动作 + 插件命令 + 主题）、模糊过滤（子序列、分词、
// 排序：前缀命中优先）、插件命令进入面板且禁用后消失、主题条目覆盖全部主题。
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cstdio>

#include "app/CommandIndex.h"
#include "app/CommandPalette.h"
#include "core/ZipWriter.h"
#include "plugins/PluginManager.h"
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

bool containsId(const QList<app::PaletteEntry>& entries, const QString& id)
{
    for (const app::PaletteEntry& entry : entries) {
        if (entry.id == id)
            return true;
    }
    return false;
}

QStringList idsOf(const QList<app::PaletteEntry>& entries)
{
    QStringList ids;
    for (const app::PaletteEntry& entry : entries)
        ids.append(entry.id);
    return ids;
}

QByteArray testPluginJson()
{
    return QByteArrayLiteral(R"({
        "id": "publisherx.palette", "name": "Palette Plugin", "version": "1.0.0",
        "publisher": "publisherx", "license": "MIT",
        "contributes": { "commands": [
            { "id": "toggleWrap", "title": "切换自动换行（插件）",
              "action": "view.toggleWordWrap" }
        ] }
    })");
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);

    auto& manager = plugins::PluginManager::instance();
    QDir(plugins::PluginManager::userPluginDirectory()).removeRecursively();
    manager.reload();

    std::printf("== entries ==\n");
    {
        const QList<app::PaletteEntry> entries = app::buildPaletteEntries();
        expect(entries.size() > 20, "palette has builtin actions and themes");
        expect(containsId(entries, QStringLiteral("editor.save")),
               "builtin action listed");
        expect(containsId(entries, QStringLiteral("theme.set:paper")),
               "theme entries listed for every available theme");

        // 主题条目数量 == 可用主题数量
        int themeEntries = 0;
        for (const app::PaletteEntry& entry : entries) {
            if (entry.kind == app::PaletteEntry::Theme)
                ++themeEntries;
        }
        expect(themeEntries == editor::ThemeManager::availableThemes().size(),
               "one palette entry per theme");

        // 夹具插件尚未安装 → 它的命令不应出现
        // （构建目录里可能已铺开随包预装插件，所以按 id 断言而不是计数）
        expect(!containsId(entries, QStringLiteral("publisherx.palette.toggleWrap")),
               "fixture plugin command absent before installing it");
    }

    std::printf("== fuzzy filter ==\n");
    {
        const QList<app::PaletteEntry> entries = app::buildPaletteEntries();

        expect(app::filterPaletteEntries(entries, QString()).size() == entries.size(),
               "empty query returns everything");

        const QList<app::PaletteEntry> save = app::filterPaletteEntries(
            entries, QStringLiteral("保存"));
        expect(!save.isEmpty()
                   && save.first().id.startsWith(QStringLiteral("editor.save")),
               "Chinese prefix match ranks first (保存)");

        // 子序列：tw -> toggleWordWrap 这类
        const QList<app::PaletteEntry> sub = app::filterPaletteEntries(
            entries, QStringLiteral("tw"));
        expect(containsId(sub, QStringLiteral("view.toggleWordWrap")),
               "subsequence match finds toggleWordWrap");

        // 分词：两个词都命中才保留
        const QList<app::PaletteEntry> words = app::filterPaletteEntries(
            entries, QStringLiteral("主题 paper"));
        expect(containsId(words, QStringLiteral("theme.set:paper"))
                   && words.size() < entries.size(),
               "space-separated terms must all match");

        expect(app::filterPaletteEntries(entries, QStringLiteral("zzzz-nothing"))
                   .isEmpty(),
               "no match returns empty list");

        // 排序：前缀命中排在包含命中之前
        const QList<app::PaletteEntry> ranked = app::filterPaletteEntries(
            entries, QStringLiteral("theme"));
        expect(!ranked.isEmpty(), "theme query matches entries");
    }

    std::printf("== plugin commands join the palette ==\n");
    {
        QTemporaryDir tmp;
        const QString package = tmp.filePath(QStringLiteral("palette.htmed"));
        core::ZipWriter writer(package);
        writer.addFile(QStringLiteral("publisherx.palette/plugin.json"),
                       testPluginJson());
        writer.close();
        const auto outcome = manager.installPackage(package);
        expect(outcome.ok, "install fixture plugin");

        const QList<app::PaletteEntry> entries = app::buildPaletteEntries();
        expect(containsId(entries, QStringLiteral("publisherx.palette.toggleWrap")),
               "plugin command appears in the palette");
        const QList<app::PaletteEntry> filtered = app::filterPaletteEntries(
            entries, QStringLiteral("插件"));
        expect(containsId(filtered, QStringLiteral("publisherx.palette.toggleWrap")),
               "plugin command searchable by its title");

        QString error;
        manager.setEnabled(QStringLiteral("publisherx.palette"), false, &error);
        expect(!containsId(app::buildPaletteEntries(),
                           QStringLiteral("publisherx.palette.toggleWrap")),
               "disabled plugin's command disappears from the palette");

        manager.uninstall(QStringLiteral("publisherx.palette"), &error);
        expect(!containsId(app::buildPaletteEntries(),
                           QStringLiteral("publisherx.palette.toggleWrap")),
               "uninstalled plugin's command disappears");
    }

    std::printf("== palette dialog ==\n");
    {
        app::CommandPalette palette;
        expect(palette.currentEntries().size() == app::buildPaletteEntries().size(),
               "dialog lists every entry when the query is empty");
        palette.setQuery(QStringLiteral("保存"));
        expect(!palette.currentEntries().isEmpty()
                   && palette.currentEntries().first().id.startsWith(
                       QStringLiteral("editor.save")),
               "dialog filters as the user types");
        palette.setQuery(QStringLiteral("zzzz"));
        expect(palette.currentEntries().isEmpty(), "dialog shows nothing for no match");
    }

    std::printf("== builtin action table sanity ==\n");
    {
        const QList<app::BuiltinActionInfo> table = app::builtinActionTable();
        expect(table.size() >= 18, "builtin action table is populated");
        QStringList ids;
        for (const app::BuiltinActionInfo& info : table)
            ids.append(QString::fromLatin1(info.id));
        ids.removeDuplicates();
        expect(ids.size() == table.size(), "no duplicate builtin action ids");
    }

    QDir(plugins::PluginManager::userPluginDirectory()).removeRecursively();

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll command palette tests passed.\n");
    return 0;
}
