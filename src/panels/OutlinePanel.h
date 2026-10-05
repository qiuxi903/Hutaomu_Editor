// Hutaomu Editor - Document outline (Markdown headings) as a tree.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QRegularExpression>
#include <QWidget>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace panels {

// "大纲"面板：按层级缩进的标题树，跟踪光标所在章节（加粗+选中），
// 单击跳转源码对应行。代码围栏内的 # 不计入。
class OutlinePanel : public QWidget {
    Q_OBJECT
public:
    explicit OutlinePanel(QWidget* parent = nullptr);

    QWidget* headerWidget() const { return m_header; } // 拖拽换边把手

    void updateFromText(const QString& markdown);
    void clearOutline();
    void setCurrentLine(int lineNumber); // 高亮光标所在章节
    int lineNumberAt(int index) const;   // 第 index 个标题的行号（1 起，DFS 序）
    int count() const;

protected:
    bool event(QEvent* event) override; // 主题/调色板变化时刷新高亮底色

signals:
    void headingActivated(int lineNumber); // 1-based

private:
    QTreeWidgetItem* makeItem(const QString& text, int level, int lineNumber,
                              QTreeWidgetItem* parent);
    void applyHighlight(int lineNumber); // 按 1-based 行号应用高亮/滚动

    QWidget* m_header = nullptr;
    QLabel* m_titleLabel = nullptr;
    QTreeWidget* m_tree = nullptr;
    QBrush m_activeBackground; // 当前章节条目的背景标记（替代 setCurrentItem）
    QObject* m_touchScroller = nullptr;
    QList<QTreeWidgetItem*> m_items; // DFS 顺序
    QList<int> m_lines;              // 与 m_items 对应的行号
    QList<QString> m_texts;          // 与 m_items 对应的标题文本（重建去重）
    QList<int> m_levels;             // 与 m_items 对应的层级
    int m_followedLine = -1;         // 上次滚动跟随的章节行号（避免重复滚动）
    QRegularExpression m_headingRegex;
};

} // namespace panels
