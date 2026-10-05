// Hutaomu Editor - Live-rendered table overlays for markdown live mode.
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QPoint>
#include <QTextBrowser>
#include <QSet>
#include <QVector>

#include "markdown/MarkdownBlocks.h"

class QPlainTextEdit;

namespace markdown {
class MarkdownLiveHighlighter;
}

namespace editor {

// 实时模式下把每个表格区域原位渲染成原生表格覆盖层。
//
// 职责划分（唯一几何来源，避免多处各算一套导致错位）：
//   CodeEditor —— 唯一知道"文档坐标 -> 视口坐标"换算的一方（QPlainTextEdit
//                 子类），算出每个表格首行的视口 y 后通过 setRegionViewportTops 传入。
//   本类       —— 只按传入坐标摆放覆盖层、控制可见性、处理点击展开。
class TableOverlayManager : public QObject {
    Q_OBJECT
public:
    explicit TableOverlayManager(QPlainTextEdit* editor, QObject* parent = nullptr);

    void setHighlighter(markdown::MarkdownLiveHighlighter* highlighter);

    void refresh();                            // 重新扫描 + 重渲染内容
    // 选区/光标决定展开集合。dragging 时 dragPos 为视口内鼠标位置，
    // 用于表格边缘的状态无关判定（防抖动）。
    void updateActive(int firstLine, int lastLine, bool dragging,
                      const QPoint& dragPos = QPoint());
    void setRegionViewportTops(const QVector<int>& viewportTops); // 唯一几何入口
    void clear();

    QVector<int> regionFirstLines() const;
    int regionCount() const { return m_regions.size(); }
    const QVector<markdown::TableRegion>& hiddenRegions() const { return m_hidden; }
    void setPointToPixel(qreal pxPerPoint) { m_pointToPixel = pxPerPoint; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void expandAt(int index, const QPoint& localPos);

    QPlainTextEdit* m_editor = nullptr;
    markdown::MarkdownLiveHighlighter* m_highlighter = nullptr;

    QVector<markdown::TableRegion> m_regions; // 全部表格区域
    QVector<markdown::TableRegion> m_hidden;  // 折叠中的（供高亮器/行号使用）
    QVector<QTextBrowser*> m_overlays;        // 与 m_regions 一一对应

    // 展开集合：光标/选区触及的区域（可多个同时展开）。
    // 拖选中光标扫过表格边缘时，同一鼠标位置在展开/折叠两个状态下会
    // 映射到表格内/表格后，若按块映射立即折叠会与再次展开互相触发形成
    // 鬼畜振荡；此时以"折叠足迹视口矩形"（与状态无关）判定去留。
    QSet<int> m_expanded;
    int m_activeFirst = -1; // 最近一次 updateActive 的行范围（refresh 复用）
    int m_activeLast = -1;
    bool m_activeDragging = false;

    QVector<int> m_viewportTops; // 各表格首行的视口 y

    qreal m_pointToPixel = 1.53;
};

} // namespace editor
