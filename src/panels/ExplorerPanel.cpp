// Hutaomu Editor - Explorer sidebar: workspace folder tree.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "ExplorerPanel.h"

#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include "themes/IconLoader.h"
#include "themes/ThemeManager.h"

namespace panels {
namespace {

// Directories first, then case-insensitive by name — like VS Code.
class DirsFirstProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        auto* model = qobject_cast<const QFileSystemModel*>(sourceModel());
        if (!model)
            return QSortFilterProxyModel::lessThan(left, right);
        const QFileInfo leftInfo = model->fileInfo(left);
        const QFileInfo rightInfo = model->fileInfo(right);
        if (leftInfo.isDir() != rightInfo.isDir())
            return leftInfo.isDir();
        return leftInfo.fileName().compare(rightInfo.fileName(),
                                           Qt::CaseInsensitive) < 0;
    }
};

} // namespace

ExplorerPanel::ExplorerPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("Sidebar"));

    m_model = new QFileSystemModel(this);
    m_model->setResolveSymlinks(true);
    // Options come online after the watcher warms up; fine for M0 sizes.
    m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);

    m_proxy = new DirsFirstProxy(this);
    m_proxy->setSourceModel(m_model);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- Header: "EXPLORER" + quick actions ---
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("sidebarHeader"));
    auto* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(14, 8, 6, 8);

    auto* titleBox = new QVBoxLayout;
    titleBox->setSpacing(1);
    m_headerLabel = new QLabel(tr("资源管理器"), m_header);
    m_headerLabel->setObjectName(QStringLiteral("sidebarTitle"));
    titleBox->addWidget(m_headerLabel);
    m_workspaceLabel = new QLabel(m_header);
    m_workspaceLabel->setObjectName(QStringLiteral("sidebarSubtitle"));
    titleBox->addWidget(m_workspaceLabel);
    headerLayout->addLayout(titleBox);
    headerLayout->addStretch();

    m_openFolderButton = new QToolButton(m_header);
    m_openFolderButton->setObjectName(QStringLiteral("sidebarButton"));
    m_openFolderButton->setToolTip(tr("打开文件夹"));
    m_openFolderButton->setAutoRaise(true);
    m_openFolderButton->setProperty("iconName", QStringLiteral("folder-open"));
    connect(m_openFolderButton, &QToolButton::clicked,
            this, &ExplorerPanel::openFolderRequested);
    headerLayout->addWidget(m_openFolderButton);

    m_refreshButton = new QToolButton(m_header);
    m_refreshButton->setObjectName(QStringLiteral("sidebarButton"));
    m_refreshButton->setToolTip(tr("刷新"));
    m_refreshButton->setAutoRaise(true);
    m_refreshButton->setProperty("iconName", QStringLiteral("refresh"));
    connect(m_refreshButton, &QToolButton::clicked,
            this, &ExplorerPanel::refreshWorkspace);
    headerLayout->addWidget(m_refreshButton);

    m_collapseButton = new QToolButton(m_header);
    m_collapseButton->setObjectName(QStringLiteral("sidebarButton"));
    m_collapseButton->setToolTip(tr("折叠文件夹"));
    m_collapseButton->setAutoRaise(true);
    m_collapseButton->setProperty("iconName", QStringLiteral("collapse-all"));
    headerLayout->addWidget(m_collapseButton);

    layout->addWidget(m_header);

    // --- Body: tree page / welcome page ---
    m_pages = new QStackedWidget(this);
    m_pages->addWidget(makeTreePage());
    m_pages->addWidget(makeWelcomePage());
    layout->addWidget(m_pages, 1);

    // No workspace yet: show the welcome page until a folder is opened.
    m_pages->setCurrentIndex(1);

    refreshIcons();
}

QWidget* ExplorerPanel::makeTreePage()
{
    m_tree = new QTreeView(this);
    m_tree->setObjectName(QStringLiteral("explorerTree"));
    m_tree->setModel(m_proxy);
    m_tree->setHeaderHidden(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setUniformRowHeights(true);
    m_tree->setExpandsOnDoubleClick(true);
    for (int column = 1; column < m_model->columnCount(); ++column)
        m_tree->hideColumn(column);

    connect(m_tree, &QTreeView::clicked, this, [this](const QModelIndex& index) {
        const QModelIndex source = m_proxy->mapToSource(index);
        const QFileInfo info = m_model->fileInfo(source);
        if (info.isFile())
            emit fileActivated(info.absoluteFilePath());
    });

    connect(m_collapseButton, &QToolButton::clicked,
            this, [this] { m_tree->collapseAll(); });

    return m_tree;
}

QWidget* ExplorerPanel::makeWelcomePage()
{
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("sidebarWelcome"));
    auto* layout = new QVBoxLayout(page);
    layout->addStretch();

    auto* hint = new QLabel(tr("未打开文件夹"), page);
    hint->setObjectName(QStringLiteral("welcomeHint"));
    hint->setAlignment(Qt::AlignHCenter);
    layout->addWidget(hint);

    auto* openButton = new QPushButton(tr("打开文件夹"), page);
    openButton->setObjectName(QStringLiteral("welcomeButton"));
    openButton->setCursor(Qt::PointingHandCursor);
    connect(openButton, &QPushButton::clicked,
            this, &ExplorerPanel::openFolderRequested);
    layout->addWidget(openButton, 0, Qt::AlignHCenter);

    layout->addStretch();
    return page;
}

void ExplorerPanel::setWorkspace(const QString& path)
{
    if (path.isEmpty() || path == m_workspacePath)
        return;
    showWorkspace(path);
}

void ExplorerPanel::showWorkspace(const QString& path)
{
    m_workspacePath = path;
    m_model->setRootPath(path);
    m_tree->setRootIndex(m_proxy->mapFromSource(m_model->index(path)));
    m_workspaceLabel->setText(QFileInfo(path).fileName());
    m_pages->setCurrentWidget(m_tree);
}

void ExplorerPanel::refreshWorkspace()
{
    if (m_workspacePath.isEmpty())
        return;
    // Re-setting the root path makes QFileSystemModel re-scan the folder.
    m_model->setRootPath(QString());
    m_model->setRootPath(m_workspacePath);
    m_tree->setRootIndex(m_proxy->mapFromSource(m_model->index(m_workspacePath)));
}

void ExplorerPanel::refreshIcons()
{
    const editor::ThemeTokens& t = editor::ThemeManager::tokens();
    const QColor glyph = editor::ThemeManager::color("sidebarText");
    for (QToolButton* button : { m_openFolderButton, m_refreshButton, m_collapseButton })
        button->setIcon(editor::IconLoader::load(button->property("iconName").toString(),
                                         glyph, t.sidebarBackground, 16));
}

} // namespace panels
