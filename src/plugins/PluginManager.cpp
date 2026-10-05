// Hutaomu Editor - Declarative plugin manager implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "PluginManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>

#include "viewers/ZipReader.h"

namespace plugins {

namespace {

constexpr const char* kManifestName = "plugin.json";
constexpr qint64 kMaxPackageBytes = 64LL * 1024 * 1024;
constexpr int kMaxEntries = 4000;

QString tr(const char* text)
{
    return QCoreApplication::translate("plugins", text);
}

bool isSafeRelativePath(const QString& path)
{
    if (path.isEmpty())
        return false;
    QString normalized = path;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalized.startsWith(QLatin1Char('/')) || normalized.contains(QLatin1Char(':')))
        return false;
    for (const QString& part : normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (part == QLatin1String(".."))
            return false;
    }
    return true;
}

QString normalizedEntryPath(const QString& raw)
{
    QString path = raw;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (path.startsWith(QLatin1String("./")))
        path.remove(0, 2);
    return path;
}

// 包内 plugin.json 的位置：包根或唯一的一级子目录（与主题包同规则）。
bool locateManifest(const QStringList& entries, QString* prefix)
{
    QString best;
    for (const QString& raw : entries) {
        const QString path = normalizedEntryPath(raw);
        if (path == QLatin1String(kManifestName)) {
            *prefix = QString();
            return true;
        }
        if (!path.endsWith(QLatin1String("/") + QLatin1String(kManifestName)))
            continue;
        const QString dir = path.left(path.size() - qstrlen(kManifestName) - 1);
        if (dir.contains(QLatin1Char('/')) || !isSafeRelativePath(dir))
            continue;
        if (!best.isEmpty() && best != dir)
            return false;
        best = dir;
    }
    if (best.isEmpty())
        return false;
    *prefix = best + QLatin1Char('/');
    return true;
}

bool readPackageBytes(const QString& path, QByteArray* out, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = tr("无法读取插件包：%1").arg(path);
        return false;
    }
    if (file.size() > kMaxPackageBytes) {
        if (error)
            *error = tr("插件包体积超出限制。");
        return false;
    }
    *out = file.readAll();
    return true;
}

void copyDirContents(const QString& from, const QString& to, QString* error)
{
    QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    const QDir base(from);
    while (it.hasNext()) {
        const QString source = it.next();
        const QFileInfo info = it.fileInfo();
        const QString relative = base.relativeFilePath(source);
        if (info.isSymLink() || !isSafeRelativePath(relative)) {
            if (error)
                *error = tr("插件包包含不安全的路径，已拒绝安装。");
            return;
        }
        const QString target = to + QLatin1Char('/') + relative;
        if (info.isDir()) {
            QDir().mkpath(target);
            continue;
        }
        if (!QDir().mkpath(QFileInfo(target).absolutePath())
            || !QFile::copy(source, target)) {
            if (error)
                *error = tr("写入插件文件失败：%1").arg(relative);
            return;
        }
    }
}

} // namespace

PluginManager& PluginManager::instance()
{
    static PluginManager manager;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        manager.reload();
    }
    return manager;
}

QString PluginManager::userPluginDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + QStringLiteral("/plugins");
}

QStringList PluginManager::builtinPluginRoots()
{
    QStringList roots;
    roots.append(QCoreApplication::applicationDirPath() + QStringLiteral("/plugins"));
    roots.append(QStringLiteral(":/plugins"));
    const QString extra = qEnvironmentVariable("HUTAOMU_PLUGIN_DIRS");
    for (const QString& dir : extra.split(QLatin1Char(';'), Qt::SkipEmptyParts))
        roots.append(dir);
    return roots;
}

bool PluginManager::isBuiltin(const QString& pluginId) const
{
    return plugin(pluginId).builtin;
}

QString PluginManager::stateFilePath() const
{
    return userPluginDirectory() + QStringLiteral("/state.json");
}

void PluginManager::reload()
{
    m_plugins.clear();
    loadState();

    // 扫描一个根目录下的所有插件（<root>/<id>/plugin.json）
    const auto scanRoot = [this](const QString& root, bool builtin) {
        const QDir dir(root);
        const QStringList entries
            = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& entry : entries) {
            const QString pluginRoot = root + QLatin1Char('/') + entry;
            QFile file(pluginRoot + QLatin1Char('/') + QLatin1String(kManifestName));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            QString error;
            const PluginManifest manifest = parseManifest(file.readAll(), &error);
            if (!manifest.isValid())
                continue; // 坏清单直接跳过（不阻断其他插件）

            // 已有同 id（用户目录后扫，因此会覆盖预装）
            bool replaced = false;
            for (int i = 0; i < m_plugins.size(); ++i) {
                if (m_plugins.at(i).manifest.id != manifest.id)
                    continue;
                if (!builtin && m_plugins.at(i).builtin) {
                    m_plugins.removeAt(i); // 用户副本覆盖预装副本
                    replaced = true;
                }
                break;
            }
            if (!replaced && plugin(manifest.id).manifest.id == manifest.id)
                continue; // 同来源重复（先扫到的优先）

            Plugin plugin;
            plugin.manifest = manifest;
            plugin.rootPath = pluginRoot;
            plugin.builtin = builtin;
            plugin.enabled = !m_disabledIds.contains(manifest.id);
            m_plugins.append(plugin);
        }
    };

    for (const QString& root : builtinPluginRoots())
        scanRoot(root, true);
    scanRoot(userPluginDirectory(), false);
}

// 已启用插件声明的 扩展名 → 语言 id 映射。由上层（MainWindow）推给
// LanguageRegistry —— 这样插件层不依赖语法层，语法层也不依赖插件层。
QHash<QString, QString> PluginManager::formatsMap() const
{
    QHash<QString, QString> map;
    for (const Plugin& plugin : m_plugins) {
        if (!plugin.enabled)
            continue;
        for (const PluginFormat& format : plugin.manifest.formats)
            map.insert(format.extension, format.languageId);
    }
    return map;
}

QList<Plugin> PluginManager::enabledPlugins() const
{
    QList<Plugin> out;
    for (const Plugin& plugin : m_plugins) {
        if (plugin.enabled)
            out.append(plugin);
    }
    return out;
}

Plugin PluginManager::plugin(const QString& id) const
{
    for (const Plugin& plugin : m_plugins) {
        if (plugin.manifest.id == id)
            return plugin;
    }
    return Plugin {};
}

bool PluginManager::exists(const QString& id) const
{
    return !plugin(id).manifest.id.isEmpty();
}

void PluginManager::loadState()
{
    m_disabledIds.clear();
    m_shellTrusted.clear();
    QFile file(stateFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    for (const QJsonValue& value : root.value(QStringLiteral("disabled")).toArray()) {
        const QString id = value.toString();
        if (!id.isEmpty())
            m_disabledIds.append(id);
    }
    for (const QJsonValue& value : root.value(QStringLiteral("shellTrusted")).toArray()) {
        const QString id = value.toString();
        if (!id.isEmpty())
            m_shellTrusted.append(id);
    }
}

void PluginManager::saveState() const
{
    QJsonObject root;
    QJsonArray disabled;
    for (const QString& id : m_disabledIds)
        disabled.append(id);
    root.insert(QStringLiteral("disabled"), disabled);
    QJsonArray trusted;
    for (const QString& id : m_shellTrusted)
        trusted.append(id);
    root.insert(QStringLiteral("shellTrusted"), trusted);

    QDir().mkpath(userPluginDirectory());
    QFile file(stateFilePath());
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

PluginManifest PluginManager::peekManifest(const QString& packagePath, QString* error,
                                           QString* suggestedId)
{
    const QFileInfo info(packagePath);
    if (info.isDir()) {
        QFile file(packagePath + QLatin1Char('/') + QLatin1String(kManifestName));
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = tr("目录中缺少 plugin.json。");
            return {};
        }
        const PluginManifest manifest = parseManifest(file.readAll(), error);
        if (manifest.isValid() && suggestedId)
            *suggestedId = manifest.id;
        return manifest;
    }

    QByteArray bytes;
    if (!readPackageBytes(packagePath, &bytes, error))
        return {};
    const viewers::ZipReader reader(bytes);
    if (!reader.isValid()) {
        if (error)
            *error = tr("无法读取插件包（不是有效的 .htmed/ZIP 文件）。");
        return {};
    }
    QString prefix;
    if (!locateManifest(reader.entries(), &prefix)) {
        if (error)
            *error = tr("插件包中找不到 plugin.json。");
        return {};
    }
    const PluginManifest manifest
        = parseManifest(reader.entry(prefix + QLatin1String(kManifestName)), error);
    if (manifest.isValid() && suggestedId)
        *suggestedId = manifest.id;
    return manifest;
}

PluginManager::InstallOutcome PluginManager::installPackage(const QString& packagePath)
{
    InstallOutcome outcome;
    const QFileInfo info(packagePath);
    if (!info.exists()) {
        outcome.error = tr("插件包不存在：%1").arg(packagePath);
        return outcome;
    }

    QString error;
    QString id;
    const PluginManifest manifest = peekManifest(packagePath, &error, &id);
    if (!manifest.isValid()) {
        outcome.error = error.isEmpty() ? tr("插件包无效。") : error;
        return outcome;
    }

    const QString target = userPluginDirectory() + QLatin1Char('/') + manifest.id;
    outcome.overwritten = QFileInfo::exists(target);
    if (outcome.overwritten && !QDir(target).removeRecursively()) {
        outcome.error = tr("无法覆盖已安装插件：%1").arg(manifest.id);
        return outcome;
    }
    if (!QDir().mkpath(userPluginDirectory())) {
        outcome.error = tr("无法创建插件目录：%1").arg(userPluginDirectory());
        return outcome;
    }

    bool ok = false;
    if (info.isDir()) {
        QDir().mkpath(target);
        copyDirContents(packagePath, target, &error);
        ok = error.isEmpty();
    } else {
        QByteArray bytes;
        if (!readPackageBytes(packagePath, &bytes, &error)) {
            outcome.error = error;
            return outcome;
        }
        const viewers::ZipReader reader(bytes);
        if (!reader.isValid()) {
            outcome.error = tr("无法读取插件包（不是有效的 .htmed/ZIP 文件）。");
            return outcome;
        }
        const QStringList entries = reader.entries();
        if (entries.size() > kMaxEntries) {
            outcome.error = tr("插件包文件数量超出限制。");
            return outcome;
        }
        QString prefix;
        if (!locateManifest(entries, &prefix)) {
            outcome.error = tr("插件包中找不到 plugin.json。");
            return outcome;
        }
        qint64 totalBytes = 0;
        ok = QDir().mkpath(target);
        for (const QString& raw : entries) {
            if (!ok)
                break;
            const QString path = normalizedEntryPath(raw);
            if (!path.startsWith(prefix))
                continue;
            const QString relative = path.mid(prefix.size());
            if (relative.isEmpty() || relative.startsWith(QLatin1String("__MACOSX")))
                continue;
            if (!isSafeRelativePath(relative)) {
                error = tr("插件包包含不安全的路径，已拒绝安装。");
                ok = false;
                break;
            }
            const QByteArray data = reader.entry(raw);
            totalBytes += data.size();
            if (totalBytes > kMaxPackageBytes) {
                error = tr("插件包体积超出限制。");
                ok = false;
                break;
            }
            const QString outPath = target + QLatin1Char('/') + relative;
            if (!QDir().mkpath(QFileInfo(outPath).absolutePath())) {
                error = tr("无法创建目录：%1").arg(relative);
                ok = false;
                break;
            }
            QFile out(outPath);
            if (!out.open(QIODevice::WriteOnly)
                || out.write(data) != data.size()) {
                error = tr("写入插件文件失败：%1").arg(relative);
                ok = false;
                break;
            }
        }
    }

    if (!ok) {
        QDir(target).removeRecursively();
        outcome.error = error.isEmpty() ? tr("安装插件包失败。") : error;
        return outcome;
    }

    reload();
    // 覆盖安装：保留原有的启用状态与 shell 信任（同一插件 id）
    outcome.ok = true;
    outcome.pluginId = manifest.id;
    outcome.manifest = manifest;
    return outcome;
}

bool PluginManager::uninstall(const QString& id, QString* error)
{
    if (!isValidPluginId(id)) {
        if (error)
            *error = tr("插件 id 不合法。");
        return false;
    }
    const Plugin target = plugin(id);
    if (target.manifest.id.isEmpty()) {
        if (error)
            *error = tr("未安装该插件：%1").arg(id);
        return false;
    }
    if (target.builtin) {
        if (error)
            *error = tr("「%1」是随程序预装的插件，不能卸载（可以禁用它）。")
                         .arg(target.manifest.name);
        return false;
    }
    if (!QDir(target.rootPath).removeRecursively()) {
        if (error)
            *error = tr("删除插件目录失败：%1").arg(id);
        return false;
    }
    m_disabledIds.removeAll(id);
    m_shellTrusted.removeAll(id);
    saveState();
    reload();
    return true;
}

bool PluginManager::setEnabled(const QString& id, bool enabled, QString* error)
{
    if (!exists(id)) {
        if (error)
            *error = tr("未安装该插件：%1").arg(id);
        return false;
    }
    if (enabled)
        m_disabledIds.removeAll(id);
    else if (!m_disabledIds.contains(id))
        m_disabledIds.append(id);
    saveState();
    reload();
    return true;
}

QStringList PluginManager::themeRoots() const
{
    QStringList roots;
    for (const Plugin& plugin : m_plugins) {
        if (!plugin.enabled)
            continue;
        if (!plugin.manifest.themePaths.isEmpty()) {
            for (const QString& relative : plugin.manifest.themePaths) {
                if (!isSafeRelativePath(relative))
                    continue;
                roots.append(plugin.rootPath + QLatin1Char('/') + relative);
            }
        } else if (QFileInfo::exists(plugin.rootPath + QStringLiteral("/themes"))) {
            roots.append(plugin.rootPath + QStringLiteral("/themes"));
        }
    }
    return roots;
}

QString PluginManager::resolveFile(const QString& pluginId,
                                   const QString& relativePath) const
{
    const Plugin target = plugin(pluginId);
    if (target.manifest.id.isEmpty() || !isSafeRelativePath(relativePath))
        return QString();
    const QString path = target.rootPath + QLatin1Char('/') + relativePath;
    return QFileInfo::exists(path) ? path : QString();
}

bool PluginManager::formatForExtension(const QString& extension, PluginFormat* out) const
{
    const QString ext = extension.toLower();
    // 后安装的插件优先（列表按名字排序，这里反转以保证"后来的赢"）
    const QList<Plugin> enabled = enabledPlugins();
    for (auto it = enabled.crbegin(); it != enabled.crend(); ++it) {
        for (const PluginFormat& format : it->manifest.formats) {
            if (format.extension == ext) {
                if (out)
                    *out = format;
                return true;
            }
        }
    }
    return false;
}

QList<PluginManager::CommandEntry> PluginManager::commands() const
{
    QList<CommandEntry> entries;
    for (const Plugin& plugin : enabledPlugins()) {
        for (const PluginCommand& command : plugin.manifest.commands) {
            CommandEntry entry;
            entry.qualifiedId
                = plugin.manifest.id + QLatin1Char('.') + command.id;
            entry.command = command;
            entry.pluginId = plugin.manifest.id;
            entry.pluginName = plugin.manifest.name;
            entries.append(entry);
        }
    }
    return entries;
}

bool PluginManager::isShellTrusted(const QString& pluginId) const
{
    return m_shellTrusted.contains(pluginId);
}

void PluginManager::setShellTrusted(const QString& pluginId, bool trusted)
{
    if (trusted) {
        if (!m_shellTrusted.contains(pluginId))
            m_shellTrusted.append(pluginId);
    } else {
        m_shellTrusted.removeAll(pluginId);
    }
    saveState();
}

QString PluginManager::expandShellArgs(const QString& args, const QString& file,
                                       const QString& dir, const QString& workspace)
{
    QString out = args;
    out.replace(QStringLiteral("{file}"), file);
    out.replace(QStringLiteral("{dir}"), dir);
    out.replace(QStringLiteral("{workspace}"), workspace);
    return out;
}

} // namespace plugins
