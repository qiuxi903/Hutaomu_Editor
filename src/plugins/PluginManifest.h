// Hutaomu Editor - Plugin manifest (plugin.json) — declarative plugin definition.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace plugins {

// 内置动作命令：直接映射到宿主既有功能（不执行外部程序，零风险）
struct PluginCommand {
    QString id;          // 插件内唯一（完整 id = <pluginId>.<commandId>）
    QString title;       // 菜单显示名
    QString category;    // 分组（可选）
    QString shortcut;    // 可选快捷键（如 "Ctrl+Alt+L"）
    QString action;      // 内置动作，如 "editor.save" / "theme.set:ocean"
    QString shell;       // 外部命令（需 permissions 含 "shell"）
    QString args;        // 外部命令参数（支持 {file} / {dir} / {workspace}）
};

// 文件格式：扩展名 → 已注册语言（打开此类文件时按该语言高亮/统计）
struct PluginFormat {
    QString extension;   // "log"（不含点）
    QString languageId;  // LanguageRegistry 里的 id，如 "plaintext"
    QString label;       // 可选显示名
};

// 状态栏部件：只能用宿主内置类型（v1 不执行插件代码）
struct PluginStatusItem {
    QString id;          // 插件内唯一
    QString type;        // wordCount / charCount / lineCount / indent / eol / encoding
    QString alignment;   // "left" / "right"（默认 right）
    QStringList visibleFor; // 语言 id 白名单；空 = 始终显示
};

// 侧栏页面：内容来自插件包内的静态文件（v1 不执行脚本）
struct PluginPage {
    QString id;          // 插件内唯一（面板 pageId = "plugin:<pluginId>:<id>"）
    QString title;       // 面板标题 / 活动栏悬停提示
    QString icon;        // 可选：包内 svg 相对路径
    QString type;        // "markdown" / "text" / "fileList"
    QString content;     // markdown/text: 包内文件；fileList: 包内目录
    QString defaultSide; // "left" / "right"
};

struct PluginManifest {
    QString id;              // 必填，且必须是 publisher.name 形式
    QString name;            // 必填，显示名
    QString version;         // 必填，语义化版本（宽松校验）
    QString publisher;       // 必填
    QString description;
    QString license;
    QString enginesHost;     // 可选：宿主最低版本（当前不强制，仅展示）
    QStringList permissions; // fs.read / fs.write / net / shell（v1 只有 shell 生效）
    QList<PluginFormat> formats;
    QList<PluginCommand> commands;
    QStringList themePaths;  // 相对插件目录的主题路径（目录或 theme.json / .htmtpi）
    QList<PluginStatusItem> statusItems;
    QList<PluginPage> pages;

    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
    // 安装确认页的说明文案：讲清 v1 声明式插件的边界
    static QString usageHint()
    {
        return QStringLiteral(
            "v1 的插件是声明式的：不含可执行代码，只能添加主题、文件格式与命令。"
            "命令分两种——内置动作（直接调用编辑器既有功能）与外部命令"
            "（需要 shell 权限，首次执行会再弹窗确认）。");
    }
    bool usesShell() const;
    // 权限列表里声明了 shell，且确有 shell 命令
    bool declaresShellPermission() const
    {
        return permissions.contains(QStringLiteral("shell"));
    }
};

// 解析 plugin.json；失败时 error 给出面向用户的原因，返回的 manifest 无效。
PluginManifest parseManifest(const QByteArray& json, QString* error);

// 校验插件 id 规则：publisher.name（两段，安全字符，与目录名一致）
bool isValidPluginId(const QString& id);

} // namespace plugins
