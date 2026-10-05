// Hutaomu Editor - Application settings (JSON persistence).
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

namespace settings {

// One instance per process, populated once at startup and flushed on
// change-of-note (window close, theme switch, zoom). Schema is versioned
// so future migrations have a place to live.
class AppSettings : public QObject {
    Q_OBJECT
public:
    static AppSettings& instance();

    void load();
    void save();

    // Window
    QByteArray mainWindowGeometry;
    QByteArray mainWindowState;
    QByteArray splitterState;
    bool maximized = false;
    bool sidebarVisible = true;

    // Editor
    QString fontFamily; // empty = platform default monospace
    int fontSize = 14;
    int tabWidth = 4;   // reserved for M1; persisted already
    bool wordWrap = false;

    // Appearance
    QString theme = QStringLiteral("paper");
    QString markdownViewMode = QStringLiteral("live"); // live / split / source
    QString previewPosition = QStringLiteral("right"); // right / bottom
    QString outlineSide = QStringLiteral("left");      // 页面停靠侧：left / right
    QString searchSide = QStringLiteral("left");       // left / right
    QString sidebarPosition = QStringLiteral("left");  // 资源管理器停靠侧：left / right

    // 主题形态（T4）：已批准布局的主题、明确"只应用配色"的主题，
    // 以及"不再提示布局变更"的全局开关。
    QStringList themeLayoutApproved;
    QStringList themeLayoutDeclined;
    bool themeLayoutPromptSuppressed = false;

    // Files
    QStringList recentFiles;
    QString lastDir;
    QString workspacePath;

    void addRecentFile(const QString& filePath);

private:
    explicit AppSettings(QObject* parent = nullptr);

    QString settingsFilePath() const;
};

} // namespace settings
