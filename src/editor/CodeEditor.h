// Hutaomu Editor - Code editor widget: line numbers, current line highlight,
// syntax highlighting, find bar, language attachment.
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QPlainTextEdit>
#include <QWidget>

#include "syntax/LanguageSpec.h"
#include "themes/ThemeManager.h"

class QLabel;

namespace syntax {
class TreeSitterHighlighter;
}

namespace markdown {
class MarkdownLiveHighlighter;
}

namespace editor {

class FindBar;
class LineNumberArea;
class TableOverlayManager;
class TouchScroller;

// 编辑器核心。语法高亮走 tree-sitter（无语言时退化为纯文本），
// 主题色通过 ThemeManager tokens 注入，不读 QSS。
class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CodeEditor(QWidget* parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent* event);
    int lineNumberAreaWidth() const;

    void applyTheme();
    // 制表符宽度（空格数）：设置页可改，影响制表位与 Tab 键插入
    void setTabWidth(int spaces);
    // 主题形态（T4）：内容居中限宽（0 = 关闭）；标题下划线（Markdown 实时预览）
    void setContentMaxWidth(int width);
    void setHeadingUnderline(bool on);
    void setLanguage(const syntax::LanguageSpec* spec);
    const syntax::LanguageSpec* language() const { return m_language; }

    void openFind(bool withReplace);
    void jumpToLine(int line, int column = 0, int selectLength = 0);
    void setLiveMarkdown(bool on);

private slots:
    void updateLiveActiveRange();
    void syncTableOverlayGeometry();

signals:
    void zoomGestureRequested(double factor); // 触控板/触屏捏合缩放

public slots:
    void setEditorFont(const QFont& font);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool viewportEvent(QEvent* event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect& rect, int dy);
    void highlightCurrentLine();
    void updateExtraSelections();

private:
    LineNumberArea* m_lineNumberArea;
    const syntax::LanguageSpec* m_language = nullptr;
    syntax::TreeSitterHighlighter* m_highlighter = nullptr;
    markdown::MarkdownLiveHighlighter* m_liveHighlighter = nullptr;
    QMetaObject::Connection m_liveRangeConn;
    FindBar* m_findBar = nullptr;
    QList<QTextEdit::ExtraSelection> m_searchSelections;

    // 主题形态：内容限宽时把文本列居中（视口左右留白），行号跟随列起点
    void applyContentMargins();
    int m_contentMaxWidth = 0;
    int m_tabWidth = 4;
    int m_contentLeftExtra = 0;

    // 主题背景图（T3）：合成后的视口纹理 + 源图缓存键（主题/路径变化时重读）
    void rebuildEditorBackground();
    QImage m_backgroundSource;
    QString m_backgroundKey;

    TouchScroller* m_touchScroller = nullptr;
    TableOverlayManager* m_tableManager = nullptr;
    QTimer* m_tableScanTimer = nullptr;
    bool m_mouseSelecting = false; // 左键拖选进行中（冻结渲染态防振荡）
    QPoint m_dragPos;              // 拖选中的视口内鼠标位置（边缘防抖判定用）
};

} // namespace editor
