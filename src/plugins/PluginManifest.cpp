// Hutaomu Editor - Plugin manifest parsing.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "PluginManifest.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

namespace plugins {

namespace {

QString tr(const char* text)
{
    return QCoreApplication::translate("plugins", text);
}

QStringList readPermissions(const QJsonArray& array)
{
    // v1 只有 shell 会在声明式插件里生效；其余（fs/net）留给脚本插件阶段，
    // 但仍如实记录并在安装页展示，避免"悄悄多要权限"。
    static const QStringList known = { QStringLiteral("fs.read"),
                                       QStringLiteral("fs.write"),
                                       QStringLiteral("net"),
                                       QStringLiteral("shell") };
    QStringList out;
    for (const QJsonValue& value : array) {
        const QString permission = value.toString();
        if (known.contains(permission) && !out.contains(permission))
            out.append(permission);
    }
    return out;
}

bool looksLikeVersion(const QString& version)
{
    static const QRegularExpression re(
        QStringLiteral("^\\d+(\\.\\d+){0,3}([-+][0-9A-Za-z.-]+)?$"));
    return re.match(version).hasMatch();
}

} // namespace

bool isValidPluginId(const QString& id)
{
    // publisher.name：两段；字符限字母数字 - _ .；整体不做路径解释
    static const QRegularExpression re(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_-]*\\.[A-Za-z0-9][A-Za-z0-9._-]*$"));
    return re.match(id).hasMatch() && !id.contains(QStringLiteral(".."));
}

bool PluginManifest::usesShell() const
{
    for (const PluginCommand& command : commands) {
        if (!command.shell.isEmpty())
            return true;
    }
    return false;
}

PluginManifest parseManifest(const QByteArray& json, QString* error)
{
    PluginManifest manifest;
    const auto fail = [&](const QString& message) {
        if (error)
            *error = message;
        return PluginManifest {};
    };

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(tr("plugin.json 不是合法的 JSON 对象。"));

    const QJsonObject root = document.object();
    manifest.id = root.value(QStringLiteral("id")).toString();
    manifest.name = root.value(QStringLiteral("name")).toString();
    manifest.version = root.value(QStringLiteral("version")).toString();
    manifest.publisher = root.value(QStringLiteral("publisher")).toString();
    manifest.description = root.value(QStringLiteral("description")).toString();
    manifest.license = root.value(QStringLiteral("license")).toString();
    manifest.enginesHost
        = root.value(QStringLiteral("engines")).toObject()
              .value(QStringLiteral("hutaomu"))
              .toString();

    if (manifest.id.isEmpty() || manifest.name.isEmpty())
        return fail(tr("plugin.json 缺少 id 或 name。"));
    if (!isValidPluginId(manifest.id))
        return fail(tr("插件 id 必须形如 publisher.name（字母、数字、-、_、.）。"));
    if (manifest.version.isEmpty() || !looksLikeVersion(manifest.version))
        return fail(tr("插件 version 缺失或不是合法版本号（如 1.0.0）。"));
    if (manifest.publisher.isEmpty())
        return fail(tr("plugin.json 缺少 publisher。"));
    // 约定：publisher 与 id 前缀一致，避免仿冒
    if (!manifest.id.startsWith(manifest.publisher + QLatin1Char('.')))
        return fail(tr("插件 id（%1）必须以 publisher（%2）加 '.' 开头。")
                        .arg(manifest.id, manifest.publisher));

    manifest.permissions
        = readPermissions(root.value(QStringLiteral("permissions")).toArray());

    const QJsonObject contributes = root.value(QStringLiteral("contributes")).toObject();

    for (const QJsonValue& value :
         contributes.value(QStringLiteral("formats")).toArray()) {
        const QJsonObject object = value.toObject();
        PluginFormat format;
        format.extension = object.value(QStringLiteral("extension")).toString().toLower();
        if (format.extension.startsWith(QLatin1Char('.')))
            format.extension.remove(0, 1);
        format.languageId = object.value(QStringLiteral("language")).toString();
        format.label = object.value(QStringLiteral("label")).toString();
        if (!format.extension.isEmpty() && !format.languageId.isEmpty())
            manifest.formats.append(format);
    }

    for (const QJsonValue& value :
         contributes.value(QStringLiteral("commands")).toArray()) {
        const QJsonObject object = value.toObject();
        PluginCommand command;
        command.id = object.value(QStringLiteral("id")).toString();
        command.title = object.value(QStringLiteral("title")).toString();
        command.category = object.value(QStringLiteral("category")).toString();
        command.shortcut = object.value(QStringLiteral("shortcut")).toString();
        command.action = object.value(QStringLiteral("action")).toString();
        command.shell = object.value(QStringLiteral("shell")).toString();
        command.args = object.value(QStringLiteral("args")).toString();
        if (command.id.isEmpty() || command.title.isEmpty())
            continue;
        if (command.action.isEmpty() && command.shell.isEmpty())
            continue; // 无动作的命令忽略（而不是让插件装不上）
        manifest.commands.append(command);
        if (!command.shell.isEmpty() && !manifest.declaresShellPermission())
            return fail(tr("命令「%1」要执行外部程序，但 permissions 里没有声明 \"shell\"。")
                            .arg(command.title));
    }

    // 状态栏部件：只接受内置类型
    static const QStringList statusTypes = {
        QStringLiteral("wordCount"), QStringLiteral("charCount"),
        QStringLiteral("lineCount"), QStringLiteral("indent"),
        QStringLiteral("eol"),       QStringLiteral("encoding"),
    };
    for (const QJsonValue& value :
         contributes.value(QStringLiteral("statusBar")).toArray()) {
        const QJsonObject object = value.toObject();
        PluginStatusItem item;
        item.id = object.value(QStringLiteral("id")).toString();
        item.type = object.value(QStringLiteral("type")).toString();
        item.alignment = object.value(QStringLiteral("alignment")).toString();
        if (!statusTypes.contains(item.type))
            continue; // 未知类型忽略（宿主不认识就不显示）
        if (item.alignment != QLatin1String("left")
            && item.alignment != QLatin1String("right"))
            item.alignment = QStringLiteral("right");
        if (item.id.isEmpty())
            item.id = item.type;
        for (const QJsonValue& language :
             object.value(QStringLiteral("visibleFor")).toArray()) {
            const QString id = language.toString();
            if (!id.isEmpty())
                item.visibleFor.append(id);
        }
        manifest.statusItems.append(item);
    }

    // 侧栏页面
    static const QStringList pageTypes = { QStringLiteral("markdown"),
                                           QStringLiteral("text"),
                                           QStringLiteral("fileList") };
    for (const QJsonValue& value :
         contributes.value(QStringLiteral("pages")).toArray()) {
        const QJsonObject object = value.toObject();
        PluginPage page;
        page.id = object.value(QStringLiteral("id")).toString();
        page.title = object.value(QStringLiteral("title")).toString();
        page.icon = object.value(QStringLiteral("icon")).toString();
        page.type = object.value(QStringLiteral("type")).toString();
        page.content = object.value(QStringLiteral("content")).toString();
        page.defaultSide = object.value(QStringLiteral("defaultSide")).toString();
        if (!pageTypes.contains(page.type) || page.id.isEmpty())
            continue;
        if (page.title.isEmpty())
            page.title = page.id;
        if (page.defaultSide != QLatin1String("right"))
            page.defaultSide = QStringLiteral("left");
        manifest.pages.append(page);
    }

    for (const QJsonValue& value :
         contributes.value(QStringLiteral("themes")).toArray()) {
        if (value.isString())
            manifest.themePaths.append(value.toString());
        else
            manifest.themePaths.append(value.toObject().value(QStringLiteral("path")).toString());
    }
    manifest.themePaths.removeAll(QString());

    if (error)
        error->clear();
    return manifest;
}

} // namespace plugins
