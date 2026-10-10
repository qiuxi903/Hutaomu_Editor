// Hutaomu Editor - Per-user background customization implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "UserBackground.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace editor::background {

namespace {

QString configPath()
{
    const QString base
        = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + QStringLiteral("/background.json");
}

} // namespace

QString storageDirectory()
{
    const QString base
        = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + QStringLiteral("/backgrounds");
}

UserBackground current()
{
    UserBackground bg;
    QFile file(configPath());
    if (!file.open(QIODevice::ReadOnly))
        return bg;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    bg.imagePath = root.value(QStringLiteral("imagePath")).toString();
    bg.mode = root.value(QStringLiteral("mode")).toString(QStringLiteral("tile"));
    bg.opacity = root.value(QStringLiteral("opacity")).toDouble(0.3);
    return bg;
}

void save(const UserBackground& bg)
{
    QJsonObject root;
    root.insert(QStringLiteral("imagePath"), bg.imagePath);
    root.insert(QStringLiteral("mode"), bg.mode);
    root.insert(QStringLiteral("opacity"), bg.opacity);
    QDir().mkpath(QFileInfo(configPath()).absolutePath());
    QFile file(configPath());
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void clear()
{
    QFile::remove(configPath());
}

QString importImage(const QString& sourcePath)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly))
        return QString();
    const QString suffix = QFileInfo(sourcePath).suffix();
    QDir().mkpath(storageDirectory());
    const QString target = storageDirectory() + QStringLiteral("/custom.")
                           + (suffix.isEmpty() ? QStringLiteral("png") : suffix);
    QFile::remove(target);
    if (!source.copy(target))
        return QString();
    return target;
}

} // namespace editor::background
