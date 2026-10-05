// Hutaomu Editor - Declarative plugin manager (L1: manifests + contributions).
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include "plugins/PluginManifest.h"

namespace plugins {

// 一个已安装（或已发现）的插件。
struct Plugin {
    PluginManifest manifest;
    QString rootPath;      // 插件目录（含 plugin.json）
    bool enabled = true;
    bool builtin = false;  // 随程序分发（预装）：不可卸载
};

// L1 声明式插件的生命周期与贡献点查询。
//
// 目录布局：<AppData>/plugins/<id>/plugin.json（+ themes/、assets/、scripts/…）
// 状态：<AppData>/plugins/state.json（enabled / shell 信任），与用户设置分开存，
// 卸载插件只动插件自己的目录与状态项。
//
// 安全边界（v1）：插件本身不含可执行代码——声明式贡献点只有
// "内置动作命令" 与 "shell 外部命令" 两种能触发行为，后者需要 permissions
// 声明 + 首次执行的显式信任。
class PluginManager {
public:
    static PluginManager& instance();

    static QString userPluginDirectory();   // <AppData>/plugins
    // 预装插件根目录列表：<程序目录>/plugins、:/plugins、$HUTAOMU_PLUGIN_DIRS
    static QStringList builtinPluginRoots();
    bool isBuiltin(const QString& pluginId) const;

    // 重新扫描插件目录（安装/卸载/启停后调用）。
    void reload();
    QList<Plugin> plugins() const { return m_plugins; }
    QList<Plugin> enabledPlugins() const;
    Plugin plugin(const QString& id) const;
    bool exists(const QString& id) const;

    // 安装（.htmed 为 ZIP；也接受目录形态）。不覆盖已安装插件时返回失败，
    // overwritten 会指出冲突；调用方负责先做权限确认页。
    struct InstallOutcome {
        bool ok = false;
        QString pluginId;
        bool overwritten = false;
        QString error;
        PluginManifest manifest; // 安装后的清单（供展示）
    };
    // peekManifest：只读取包内清单（用于权限确认页），不安装。
    static PluginManifest peekManifest(const QString& packagePath, QString* error,
                                       QString* suggestedId = nullptr);
    InstallOutcome installPackage(const QString& packagePath);
    bool uninstall(const QString& id, QString* error);
    bool setEnabled(const QString& id, bool enabled, QString* error);

    // ---- 贡献点 ----
    // 插件提供的主题根目录（<plugin>/themes 或清单里显式给出的路径）
    QStringList themeRoots() const;
    // 把插件包内相对路径解析成绝对路径（越界返回空串）；页面/图标用
    QString resolveFile(const QString& pluginId, const QString& relativePath) const;
    // 已启用插件的 扩展名 → 语言 id（供上层推给 LanguageRegistry）
    QHash<QString, QString> formatsMap() const;
    // 扩展名 → 格式（已启用插件；后安装的优先）
    bool formatForExtension(const QString& extension, PluginFormat* out) const;
    // 全部命令：(完整命令 id, 命令, 插件 id)
    struct CommandEntry {
        QString qualifiedId;   // <pluginId>.<commandId>
        PluginCommand command;
        QString pluginId;
        QString pluginName;
    };
    QList<CommandEntry> commands() const;

    // ---- shell 信任 ----
    bool isShellTrusted(const QString& pluginId) const;
    void setShellTrusted(const QString& pluginId, bool trusted);
    // 替换命令参数里的 {file} / {dir} / {workspace} 占位符
    static QString expandShellArgs(const QString& args, const QString& file,
                                   const QString& dir, const QString& workspace);

private:
    PluginManager() = default;

    void loadState();
    void saveState() const;
    QString stateFilePath() const;

    QList<Plugin> m_plugins;
    QStringList m_disabledIds;   // 已禁用的插件 id
    QStringList m_shellTrusted;  // 已信任可执行外部命令的插件 id
};

} // namespace plugins
