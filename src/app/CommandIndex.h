// Hutaomu Editor - Command index for the command palette (Ctrl+Shift+P).
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QList>
#include <QString>

namespace app {

// 命令面板里的一条可执行项。
struct PaletteEntry {
    enum Kind {
        Builtin, // 宿主内置动作（MainWindow::executeBuiltinAction）
        Plugin,  // 插件命令（完整 id，MainWindow::runPluginCommand）
        Theme,   // 切换主题（theme.set:<id>）
    };

    QString id;        // 执行标识
    QString title;     // 显示名
    QString category;  // 分组（内置动作/插件名/主题）
    QString shortcut;  // 快捷键（若有）
    Kind kind = Builtin;
};

// 宿主内置动作表：id + 显示名 + 分组。执行实现见 MainWindow::executeBuiltinAction。
struct BuiltinActionInfo {
    const char* id;
    const char* title;
    const char* category;
};
QList<BuiltinActionInfo> builtinActionTable();

// 汇总所有可执行项：内置动作 + 插件命令 + 全部可用主题。
// 不依赖 MainWindow，便于测试与复用（如 --list-commands）。
QList<PaletteEntry> buildPaletteEntries();

// 模糊过滤：查询串按空格分词，逐词做"子序列匹配"（不区分大小写），
// 全部词都命中的条目按得分排序（前缀命中 > 词首命中 > 位置靠前）。
QList<PaletteEntry> filterPaletteEntries(const QList<PaletteEntry>& entries,
                                        const QString& query);

} // namespace app
