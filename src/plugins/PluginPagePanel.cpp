// Hutaomu Editor - Sidebar page provided by a declarative plugin.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "PluginPagePanel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "PluginManager.h"
#include "markdown/MarkdownRenderer.h"
#include "themes/ThemeManager.h"

namespace plugins {

PluginPagePanel::PluginPagePanel(const QString& pluginId, const PluginManifest& manifest,
                                 const PluginPage& page, QWidget* parent)
    : QWidget(parent)
    , m_pluginId(pluginId)
    , m_page(page)
    , m_pageId(QStringLiteral("plugin:%1:%2").arg(pluginId, page.id))
{
    setObjectName(m_pageId);
    if (manifest.themePaths.isEmpty())
        Q_UNUSED(manifest);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_header = new QLabel(page.title, this);
    m_header->setObjectName(QStringLiteral("sidebarTitle"));
    m_header->setContentsMargins(10, 8, 10, 8);
    layout->addWidget(m_header);

    const QString path = PluginManager::instance().resolveFile(pluginId, page.content);
    if (page.type == QLatin1String("fileList")) {
        m_fileList = new QListWidget(this);
        m_fileList->setObjectName(QStringLiteral("pluginFileList"));
        m_fileList->setFrameShape(QFrame::NoFrame);
        // 只列出插件包内目录的顶层文件（v1 不做递归）
        const QDir dir(path);
        if (!path.isEmpty() && dir.exists()) {
            for (const QFileInfo& info :
                 dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                   QDir::Name)) {
                auto* item = new QListWidgetItem(info.fileName(), m_fileList);
                item->setData(Qt::UserRole, info.absoluteFilePath());
                if (info.isDir())
                    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            }
        }
        connect(m_fileList, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem* item) {
                    const QString file = item->data(Qt::UserRole).toString();
                    if (!file.isEmpty() && QFileInfo(file).isFile())
                        emit fileActivated(file);
                });
        layout->addWidget(m_fileList, 1);
    } else {
        m_browser = new QTextBrowser(this);
        m_browser->setObjectName(QStringLiteral("previewBrowser")); // 复用预览配色
        m_browser->setOpenExternalLinks(true);
        m_browser->setFrameShape(QFrame::NoFrame);
        QFile file(path);
        if (!path.isEmpty() && file.open(QIODevice::ReadOnly)) {
            const QString text = QString::fromUtf8(file.readAll());
            if (page.type == QLatin1String("markdown"))
                m_browser->setHtml(markdown::renderToHtml(text).html);
            else
                m_browser->setPlainText(text);
        } else {
            m_browser->setPlainText(tr("（插件内容缺失：%1）").arg(page.content));
        }
        layout->addWidget(m_browser, 1);
    }

    refreshTheme();
}

void PluginPagePanel::refreshTheme()
{
    const editor::ThemeTokens& tokens = editor::ThemeManager::tokens();
    const QColor text = editor::ThemeManager::color(QStringLiteral("sidebarText"));
    if (m_header)
        m_header->setStyleSheet(QStringLiteral("color:%1;").arg(text.name()));
    if (m_browser) {
        m_browser->document()->setDefaultStyleSheet(
            QStringLiteral("body { color:%1; } a { color:%2; } "
                           "code { color:%3; }")
                .arg(text.name(), tokens.accent.name(),
                     editor::ThemeManager::syntaxColor(QStringLiteral("string")).name()));
        // 重新应用样式表后刷新渲染
        const QString html = m_browser->toHtml();
        if (!html.isEmpty())
            m_browser->setHtml(m_browser->toHtml().isEmpty() ? html : html);
    }
    if (m_fileList)
        m_fileList->setPalette(QPalette());
}

} // namespace plugins
