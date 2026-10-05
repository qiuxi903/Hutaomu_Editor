// Hutaomu Editor - Workspace-wide text search panel.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace panels {

// 递归收集工作区文本文件（跳过 .git、build 等目录，跳过二进制）。
QStringList collectWorkspaceFiles(const QString& root, int maxFiles);

class SearchPanel : public QWidget {
    Q_OBJECT
public:
    explicit SearchPanel(QWidget* parent = nullptr);

    QWidget* headerWidget() const { return m_header; } // 拖拽把手

    void setWorkspace(const QString& path);
    void focusInput();

signals:
    void resultActivated(const QString& filePath, int line, int column, int length);

private:
    void runSearch();

    QWidget* m_header = nullptr; // 拖拽把手（标题栏）
    QString m_workspace;
    QLineEdit* m_input = nullptr;
    QCheckBox* m_caseBox = nullptr;
    QCheckBox* m_wordBox = nullptr;
    QCheckBox* m_regexBox = nullptr;
    QListWidget* m_results = nullptr;
    QPushButton* m_searchButton = nullptr;
};

} // namespace panels
