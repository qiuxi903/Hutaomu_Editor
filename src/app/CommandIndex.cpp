// Hutaomu Editor - Command index implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "CommandIndex.h"

#include <algorithm>

#include "plugins/PluginManager.h"
#include "themes/ThemeManager.h"

namespace app {

QList<BuiltinActionInfo> builtinActionTable()
{
    // 顺序即面板默认顺序：常用在前
    return {
        { "editor.save", "保存文件", "编辑" },
        { "editor.saveAs", "另存为…", "编辑" },
        { "editor.new", "新建文件", "编辑" },
        { "editor.close", "关闭当前标签", "编辑" },
        { "editor.find", "查找…", "编辑" },
        { "editor.quickOpen", "快速打开文件…", "编辑" },
        { "editor.zoomIn", "放大字号", "编辑" },
        { "editor.zoomOut", "缩小字号", "编辑" },
        { "editor.resetZoom", "重置字号", "编辑" },
        { "view.toggleSidebar", "切换侧边栏", "视图" },
        { "view.toggleOutline", "切换大纲", "视图" },
        { "view.toggleWordWrap", "切换自动换行", "视图" },
        { "view.toggleTheme", "深色/浅色切换", "视图" },
        { "view.markdownLive", "Markdown：实时预览", "视图" },
        { "view.markdownSplit", "Markdown：分栏预览", "视图" },
        { "view.markdownSource", "Markdown：源码模式", "视图" },
        { "app.openFile", "打开文件…", "应用" },
        { "app.openFolder", "打开文件夹…", "应用" },
        { "app.settings", "打开设置", "应用" },
        { "app.about", "关于 Hutaomu Editor", "应用" },
    };
}

QList<PaletteEntry> buildPaletteEntries()
{
    QList<PaletteEntry> entries;
    for (const BuiltinActionInfo& info : builtinActionTable()) {
        PaletteEntry entry;
        entry.id = QString::fromLatin1(info.id);
        entry.title = QString::fromUtf8(info.title);
        entry.category = QString::fromUtf8(info.category);
        entry.kind = PaletteEntry::Builtin;
        entries.append(entry);
    }

    for (const auto& command : plugins::PluginManager::instance().commands()) {
        PaletteEntry entry;
        entry.id = command.qualifiedId;
        entry.title = command.command.title;
        entry.category = command.pluginName;
        entry.shortcut = command.command.shortcut;
        entry.kind = PaletteEntry::Plugin;
        entries.append(entry);
    }

    // 主题：让"换个样子"也能用键盘完成
    const QList<editor::ThemeDefinition> themes = editor::ThemeManager::availableThemes();
    for (const editor::ThemeDefinition& theme : themes) {
        PaletteEntry entry;
        entry.id = QStringLiteral("theme.set:") + theme.id;
        entry.title = QStringLiteral("切换主题：%1").arg(theme.name);
        entry.category = QStringLiteral("主题");
        entry.kind = PaletteEntry::Theme;
        entries.append(entry);
    }
    return entries;
}

namespace {

// 匹配打分（越小越靠前）：
//  1) 精确包含：得分 = 命中位置（越靠前越好，前缀命中即 0）
//  2) 子序列命中：得分 = 100 + 各字符命中位置之和（排在所有精确包含之后）
//  3) 不命中：-1
int matchScore(const QString& haystack, const QString& needle)
{
    if (needle.isEmpty())
        return 0;
    const int at = haystack.indexOf(needle, 0, Qt::CaseInsensitive);
    if (at >= 0)
        return at;
    int position = 0;
    int score = 100;
    for (int i = 0; i < needle.size(); ++i) {
        const int found = haystack.indexOf(needle.at(i), position, Qt::CaseInsensitive);
        if (found < 0)
            return -1;
        score += found;
        position = found + 1;
    }
    return score;
}

} // namespace

QList<PaletteEntry> filterPaletteEntries(const QList<PaletteEntry>& entries,
                                        const QString& query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty())
        return entries;

    QStringList words = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QList<QPair<int, PaletteEntry>> scored;
    for (const PaletteEntry& entry : entries) {
        // 标题（中文）+ 分组 + 命令 id 都可搜：用户既能打"保存"，也能打 "save"/"wrap"
        const QString haystack = entry.title + QLatin1Char(' ') + entry.category
                                 + QLatin1Char(' ') + entry.id;
        int total = 0;
        bool matched = true;
        for (const QString& word : words) {
            const int score = matchScore(haystack, word);
            if (score < 0) {
                matched = false;
                break;
            }
            total += score;
        }
        if (matched)
            scored.append({ total, entry });
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const QPair<int, PaletteEntry>& a,
                        const QPair<int, PaletteEntry>& b) {
                         return a.first < b.first;
                     });
    QList<PaletteEntry> result;
    result.reserve(scored.size());
    for (const auto& pair : scored)
        result.append(pair.second);
    return result;
}

} // namespace app
