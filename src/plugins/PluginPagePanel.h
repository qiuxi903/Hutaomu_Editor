// Hutaomu Editor - Sidebar page provided by a declarative plugin.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QWidget>

#include "plugins/PluginManifest.h"

class QLabel;
class QListWidget;
class QTextBrowser;

namespace plugins {

// 插件页面面板：标题栏 + 静态内容（markdown/text 渲染，或包内目录的文件列表）。
// v1 只渲染插件包内的静态内容，不执行任何脚本 —— 内容来源由 PluginManager
// 解析（越界路径会被拒绝，见 resolveFile）。
class PluginPagePanel : public QWidget {
    Q_OBJECT
public:
    PluginPagePanel(const QString& pluginId, const PluginManifest& manifest,
                    const PluginPage& page, QWidget* parent = nullptr);

    QString pageId() const { return m_pageId; }
    void refreshTheme();

signals:
    // fileList 双击某个文件 → 交给 MainWindow 打开
    void fileActivated(const QString& filePath);

private:
    QString m_pluginId;
    PluginPage m_page;
    QString m_pageId;
    QLabel* m_header = nullptr;
    QTextBrowser* m_browser = nullptr;
    QListWidget* m_fileList = nullptr;
};

} // namespace plugins
