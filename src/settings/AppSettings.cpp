// Hutaomu Editor - Application settings (JSON persistence).
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "AppSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace settings {
namespace {

constexpr int kMaxRecentFiles = 10;
constexpr int kMinFontSize = 8;
constexpr int kMaxFontSize = 40;

} // namespace

AppSettings& AppSettings::instance()
{
    static AppSettings s_instance;
    return s_instance;
}

AppSettings::AppSettings(QObject* parent)
    : QObject(parent)
{
}

QString AppSettings::settingsFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/settings.json");
}

void AppSettings::load()
{
    QFile file(settingsFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return; // First launch: keep defaults.

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();

    const QJsonObject window = root.value(QStringLiteral("window")).toObject();
    mainWindowGeometry = QByteArray::fromBase64(
        window.value(QStringLiteral("geometry")).toString().toLatin1());
    mainWindowState = QByteArray::fromBase64(
        window.value(QStringLiteral("state")).toString().toLatin1());
    splitterState = QByteArray::fromBase64(
        window.value(QStringLiteral("splitter")).toString().toLatin1());
    maximized = window.value(QStringLiteral("maximized")).toBool(false);
    sidebarVisible = window.value(QStringLiteral("sidebarVisible")).toBool(true);

    const QJsonObject editor = root.value(QStringLiteral("editor")).toObject();
    fontFamily = editor.value(QStringLiteral("fontFamily")).toString();
    fontSize = editor.value(QStringLiteral("fontSize")).toInt(14);
    tabWidth = editor.value(QStringLiteral("tabWidth")).toInt(4);
    wordWrap = editor.value(QStringLiteral("wordWrap")).toBool(false);
    fontSize = qBound(kMinFontSize, fontSize, kMaxFontSize);

    const QJsonObject appearance = root.value(QStringLiteral("appearance")).toObject();
    theme = appearance.value(QStringLiteral("theme")).toString(QStringLiteral("paper"));
    markdownViewMode = appearance.value(QStringLiteral("markdownViewMode")).toString(QStringLiteral("live"));
    previewPosition = appearance.value(QStringLiteral("previewPosition")).toString(QStringLiteral("right"));
    // 兼容旧键名 outlinePosition -> outlineSide
    outlineSide = appearance.contains(QStringLiteral("outlineSide"))
                      ? appearance.value(QStringLiteral("outlineSide")).toString(QStringLiteral("left"))
                      : appearance.value(QStringLiteral("outlinePosition")).toString(QStringLiteral("left"));
    searchSide = appearance.value(QStringLiteral("searchSide")).toString(QStringLiteral("left"));
    sidebarPosition = appearance.value(QStringLiteral("sidebarPosition")).toString(QStringLiteral("left"));
    themeLayoutPromptSuppressed
        = appearance.value(QStringLiteral("themeLayoutPromptSuppressed")).toBool(false);
    themeLayoutApproved.clear();
    for (const QJsonValue& value :
         appearance.value(QStringLiteral("themeLayoutApproved")).toArray()) {
        const QString id = value.toString();
        if (!id.isEmpty())
            themeLayoutApproved.append(id);
    }
    themeLayoutDeclined.clear();
    for (const QJsonValue& value :
         appearance.value(QStringLiteral("themeLayoutDeclined")).toArray()) {
        const QString id = value.toString();
        if (!id.isEmpty())
            themeLayoutDeclined.append(id);
    }

    const QJsonArray recent = root.value(QStringLiteral("recentFiles")).toArray();
    recentFiles.clear();
    for (const QJsonValue& v : recent)
        recentFiles.append(v.toString());

    lastDir = root.value(QStringLiteral("lastDir")).toString();
    workspacePath = root.value(QStringLiteral("workspacePath")).toString();
}

void AppSettings::save()
{
    QJsonObject root;

    QJsonObject window;
    window.insert(QStringLiteral("geometry"),
                  QString::fromLatin1(mainWindowGeometry.toBase64()));
    window.insert(QStringLiteral("state"),
                  QString::fromLatin1(mainWindowState.toBase64()));
    window.insert(QStringLiteral("splitter"),
                  QString::fromLatin1(splitterState.toBase64()));
    window.insert(QStringLiteral("maximized"), maximized);
    window.insert(QStringLiteral("sidebarVisible"), sidebarVisible);
    root.insert(QStringLiteral("window"), window);

    QJsonObject editor;
    editor.insert(QStringLiteral("fontFamily"), fontFamily);
    editor.insert(QStringLiteral("fontSize"), fontSize);
    editor.insert(QStringLiteral("tabWidth"), tabWidth);
    editor.insert(QStringLiteral("wordWrap"), wordWrap);
    root.insert(QStringLiteral("editor"), editor);

    QJsonObject appearance;
    appearance.insert(QStringLiteral("theme"), theme);
    appearance.insert(QStringLiteral("markdownViewMode"), markdownViewMode);
    appearance.insert(QStringLiteral("previewPosition"), previewPosition);
    appearance.insert(QStringLiteral("outlineSide"), outlineSide);
    appearance.insert(QStringLiteral("searchSide"), searchSide);
    appearance.insert(QStringLiteral("sidebarPosition"), sidebarPosition);
    appearance.insert(QStringLiteral("themeLayoutPromptSuppressed"),
                      themeLayoutPromptSuppressed);
    QJsonArray approved;
    for (const QString& id : themeLayoutApproved)
        approved.append(id);
    appearance.insert(QStringLiteral("themeLayoutApproved"), approved);
    QJsonArray declined;
    for (const QString& id : themeLayoutDeclined)
        declined.append(id);
    appearance.insert(QStringLiteral("themeLayoutDeclined"), declined);
    root.insert(QStringLiteral("appearance"), appearance);

    QJsonArray recent;
    for (const QString& path : recentFiles)
        recent.append(path);
    root.insert(QStringLiteral("recentFiles"), recent);

    root.insert(QStringLiteral("lastDir"), lastDir);
    root.insert(QStringLiteral("workspacePath"), workspacePath);

    const QString path = settingsFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void AppSettings::addRecentFile(const QString& filePath)
{
    const QString absolute = QFileInfo(filePath).absoluteFilePath();
    recentFiles.removeAll(absolute);
    recentFiles.prepend(absolute);
    while (recentFiles.size() > kMaxRecentFiles)
        recentFiles.removeLast();
}

} // namespace settings
