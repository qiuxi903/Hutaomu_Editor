// Hutaomu Editor - Explorer sidebar: workspace folder tree.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QWidget>

class QFileSystemModel;
class QLabel;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QToolButton;
class QTreeView;

namespace panels {

// Left sidebar: "EXPLORER" header with quick actions, either the workspace
// folder tree or a welcome page when no folder is open. Single click on a
// file activates it in the editor.
class ExplorerPanel : public QWidget {
    Q_OBJECT
public:
    explicit ExplorerPanel(QWidget* parent = nullptr);

    void refreshIcons();
    QString workspacePath() const { return m_workspacePath; }
    QWidget* headerWidget() const { return m_header; } // 拖拽换边把手

public slots:
    void setWorkspace(const QString& path);
    void refreshWorkspace();

signals:
    void fileActivated(const QString& filePath);
    void openFolderRequested();

private:
    QWidget* makeTreePage();
    QWidget* makeWelcomePage();
    void showWorkspace(const QString& path);

    QString m_workspacePath;
    QFileSystemModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxy = nullptr;
    QStackedWidget* m_pages = nullptr;
    QTreeView* m_tree = nullptr;
    QWidget* m_header = nullptr;
    QLabel* m_headerLabel = nullptr;
    QLabel* m_workspaceLabel = nullptr;
    QToolButton* m_openFolderButton = nullptr;
    QToolButton* m_refreshButton = nullptr;
    QToolButton* m_collapseButton = nullptr;
};

} // namespace panels
