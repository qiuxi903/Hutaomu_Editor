// Hutaomu Editor - Markdown preview pane (native QTextBrowser).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QScrollBar>
#include <QWidget>

class QLabel;
class QTextBrowser;

namespace panels {

class MarkdownPreview : public QWidget {
    Q_OBJECT
public:
    explicit MarkdownPreview(QWidget* parent = nullptr);

    // 重新渲染（文档内容变化或主题切换时）。
    void setMarkdown(const QString& text, const QString& filePath);
    void clear();
    QScrollBar* verticalScrollBar() const;
    QWidget* headerWidget() const { return m_header; } // 拖拽换边把手

signals:
    // 点击预览中的标题锚点（sec-N），N 为文档中标题的序号（0 起）
    void headingClicked(int headingIndex);

protected:
    bool event(QEvent* event) override;

private:
    QString buildPage(const QString& bodyHtml) const;

    QTextBrowser* m_browser = nullptr;
    QWidget* m_header = nullptr;
    QLabel* m_headerTitle = nullptr;
    QString m_lastHtmlBody;
    QString m_filePath;
};

} // namespace panels
