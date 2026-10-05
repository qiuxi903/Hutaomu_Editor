// Hutaomu Editor - Theme package (.htmtpi) installation: ZIP unpacking with
// path-traversal protection, plus directory-form packages.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "ThemePackage.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

#include "viewers/ZipReader.h"

namespace editor::themepkg {

namespace {

constexpr const char* kThemeFileName = "theme.json";
constexpr qint64 kMaxPackageBytes = 64LL * 1024 * 1024; // 64MB 上限，防打包炸弹
constexpr int kMaxEntries = 4000;

QString tr(const char* text)
{
    return QCoreApplication::translate("themepkg", text);
}

// 包内相对路径必须落在目标目录内（拒绝绝对路径、盘符、.. 越界）。
bool isSafeRelativePath(const QString& path)
{
    if (path.isEmpty())
        return false;
    QString normalized = path;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalized.startsWith(QLatin1Char('/')) || normalized.contains(QLatin1Char(':')))
        return false;
    const QStringList parts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        if (part == QLatin1String(".."))
            return false;
    }
    return true;
}

// 归一化 zip 内条目路径：统一分隔符、去掉 "./" 前缀；不安全则返回空串。
QString normalizedEntryPath(const QString& raw)
{
    QString path = raw;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (path.startsWith(QLatin1String("./")))
        path.remove(0, 2);
    return path;
}

QString parseThemeId(const QByteArray& json, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = tr("theme.json 不是合法的 JSON 对象。");
        return QString();
    }
    const QString id = document.object().value(QStringLiteral("id")).toString();
    if (!isValidThemeId(id)) {
        if (error)
            *error = tr("theme.json 缺少合法的 id（只允许字母、数字、-、_、.）。");
        return QString();
    }
    return id;
}

bool readPackageBytes(const QString& path, QByteArray* out, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = tr("无法读取主题包：%1").arg(path);
        return false;
    }
    if (file.size() > kMaxPackageBytes) {
        if (error)
            *error = tr("主题包体积超出限制。");
        return false;
    }
    *out = file.readAll();
    return true;
}

bool copyDirectory(const QString& from, const QString& to, QString* error)
{
    const QDir fromDir(from);
    qint64 totalBytes = 0;
    QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString source = it.next();
        const QFileInfo info = it.fileInfo();
        const QString relative = fromDir.relativeFilePath(source);
        // 符号链接一律拒绝：它们可以指向包外的任意位置。
        if (info.isSymLink() || !isSafeRelativePath(relative)) {
            if (error)
                *error = tr("主题包包含不安全的路径（符号链接或越界路径），已拒绝安装。");
            return false;
        }
        const QString target = to + QLatin1Char('/') + relative;
        if (info.isDir()) {
            if (!QDir().mkpath(target)) {
                if (error)
                    *error = tr("无法创建目录：%1").arg(relative);
                return false;
            }
            continue;
        }
        totalBytes += info.size();
        if (totalBytes > kMaxPackageBytes) {
            if (error)
                *error = tr("主题包体积超出限制。");
            return false;
        }
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(source, target)) {
            if (error)
                *error = tr("写入主题文件失败：%1").arg(relative);
            return false;
        }
    }
    return true;
}

// 找到 theme.json 的位置：包根或唯一的一级子目录内（ZIP 与目录都可能是这两种布局）。
// 返回前缀（含结尾 '/'，根目录时为空串），失败返回 false。
bool locateThemeJson(const QStringList& entryPaths, QString* prefix)
{
    QString best;
    for (const QString& raw : entryPaths) {
        const QString path = normalizedEntryPath(raw);
        if (path == QLatin1String(kThemeFileName)) {
            *prefix = QString();
            return true;
        }
        if (!path.endsWith(QLatin1String("/") + QLatin1String(kThemeFileName)))
            continue;
        const QString dir = path.left(path.size() - qstrlen(kThemeFileName) - 1);
        if (dir.contains(QLatin1Char('/')))
            continue; // 只接受一级子目录
        if (!isSafeRelativePath(dir))
            continue;
        if (!best.isEmpty() && best != dir)
            return false; // 多个候选，拒绝猜测
        best = dir;
    }
    if (best.isEmpty())
        return false;
    *prefix = best + QLatin1Char('/');
    return true;
}

} // namespace

bool isValidThemeId(const QString& id)
{
    if (id.isEmpty() || id == QLatin1String(".") || id == QLatin1String(".."))
        return false;
    for (const QChar& c : id) {
        if (!(c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_')
              || c == QLatin1Char('.')))
            return false;
    }
    return !id.contains(QLatin1String(".."));
}

QString peekThemeId(const QString& packagePath, QString* error)
{
    const QFileInfo info(packagePath);
    if (info.isDir()) {
        QFile file(packagePath + QLatin1Char('/') + QLatin1String(kThemeFileName));
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = tr("目录中缺少 theme.json。");
            return QString();
        }
        return parseThemeId(file.readAll(), error);
    }

    QByteArray bytes;
    if (!readPackageBytes(packagePath, &bytes, error))
        return QString();
    const viewers::ZipReader reader(bytes);
    if (!reader.isValid()) {
        if (error)
            *error = tr("无法读取主题包（不是有效的 .htmtpi/ZIP 文件）。");
        return QString();
    }
    QString prefix;
    if (!locateThemeJson(reader.entries(), &prefix)) {
        if (error)
            *error = tr("主题包中找不到 theme.json。");
        return QString();
    }
    return parseThemeId(reader.entry(prefix + QLatin1String(kThemeFileName)), error);
}

InstallOutcome install(const QString& packagePath, const QString& targetRoot)
{
    InstallOutcome outcome;
    const QFileInfo info(packagePath);
    if (!info.exists()) {
        outcome.error = tr("主题包不存在：%1").arg(packagePath);
        return outcome;
    }

    // 先解析 id（也用于目录形态的目标路径）。
    QString idError;
    const QString themeId = peekThemeId(packagePath, &idError);
    if (themeId.isEmpty()) {
        outcome.error = idError.isEmpty() ? tr("主题包无效。") : idError;
        return outcome;
    }

    const QString target = targetRoot + QLatin1Char('/') + themeId;
    outcome.overwritten = QFileInfo::exists(target);
    if (outcome.overwritten && !QDir(target).removeRecursively()) {
        outcome.error = tr("无法覆盖已安装的主题：%1").arg(themeId);
        return outcome;
    }
    if (!QDir().mkpath(targetRoot)) {
        outcome.error = tr("无法创建主题目录：%1").arg(targetRoot);
        return outcome;
    }

    bool ok = false;
    QString error;
    if (info.isDir()) {
        if (!QDir().mkpath(target)) {
            outcome.error = tr("无法创建主题目录：%1").arg(target);
            return outcome;
        }
        ok = copyDirectory(packagePath, target, &error);
    } else {
        QByteArray bytes;
        if (!readPackageBytes(packagePath, &bytes, &error)) {
            outcome.error = error;
            return outcome;
        }
        const viewers::ZipReader reader(bytes);
        if (!reader.isValid()) {
            outcome.error = tr("无法读取主题包（不是有效的 .htmtpi/ZIP 文件）。");
            return outcome;
        }
        const QStringList entryPaths = reader.entries();
        if (entryPaths.size() > kMaxEntries) {
            outcome.error = tr("主题包文件数量超出限制。");
            return outcome;
        }
        QString prefix;
        if (!locateThemeJson(entryPaths, &prefix)) {
            outcome.error = tr("主题包中找不到 theme.json。");
            return outcome;
        }

        qint64 totalBytes = 0;
        ok = QDir().mkpath(target);
        for (const QString& raw : entryPaths) {
            if (!ok)
                break;
            const QString path = normalizedEntryPath(raw);
            if (!path.startsWith(prefix))
                continue;
            const QString relative = path.mid(prefix.size());
            if (relative.isEmpty() || relative.startsWith(QLatin1String("__MACOSX")))
                continue;
            if (!isSafeRelativePath(relative)) {
                error = tr("主题包包含不安全的路径，已拒绝安装。");
                ok = false;
                break;
            }
            const QByteArray data = reader.entry(raw);
            totalBytes += data.size();
            if (totalBytes > kMaxPackageBytes) {
                error = tr("主题包体积超出限制。");
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
            if (!out.open(QIODevice::WriteOnly)) {
                error = tr("无法写入主题文件：%1").arg(relative);
                ok = false;
                break;
            }
            ok = out.write(data) == data.size();
            if (!ok)
                error = tr("写入主题文件失败：%1").arg(relative);
        }
    }

    if (!ok) {
        QString ignored;
        remove(targetRoot, themeId, &ignored);
        outcome.error = error.isEmpty() ? tr("安装主题包失败。") : error;
        return outcome;
    }

    outcome.ok = true;
    outcome.themeId = themeId;
    return outcome;
}

bool remove(const QString& targetRoot, const QString& themeId, QString* error)
{
    if (!isValidThemeId(themeId)) {
        if (error)
            *error = tr("主题 id 不合法。");
        return false;
    }
    const QString target = targetRoot + QLatin1Char('/') + themeId;
    if (!QFileInfo::exists(target)) {
        if (error)
            *error = tr("主题未安装：%1").arg(themeId);
        return false;
    }
    if (!QDir(target).removeRecursively()) {
        if (error)
            *error = tr("删除主题失败：%1").arg(themeId);
        return false;
    }
    return true;
}

} // namespace editor::themepkg
