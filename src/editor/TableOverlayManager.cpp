// Hutaomu Editor - Live-rendered table overlays for markdown live mode.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "TableOverlayManager.h"

#include <QAbstractTextDocumentLayout>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>

#include "editor/TouchScroller.h"
#include "markdown/MarkdownLiveHighlighter.h"
#include "markdown/MarkdownRenderer.h"
#include "themes/ThemeManager.h"

namespace editor {

TableOverlayManager::TableOverlayManager(QPlainTextEdit* editor, QObject* parent)
    : QObject(parent)
    , m_editor(editor)
{
}

void TableOverlayManager::setHighlighter(markdown::MarkdownLiveHighlighter* highlighter)
{
    m_highlighter = highlighter;
}

QVector<int> TableOverlayManager::regionFirstLines() const
{
    QVector<int> lines;
    lines.reserve(m_regions.size());
    for (const markdown::TableRegion& region : m_regions)
        lines.append(region.firstLine);
    return lines;
}

void TableOverlayManager::refresh()
{
    m_regions = markdown::scanTableRegions(m_editor->toPlainText());

    // 同步覆盖层数量
    while (m_overlays.size() < m_regions.size()) {
        auto* browser = new QTextBrowser(m_editor->viewport());
        browser->setObjectName(QStringLiteral("tableOverlay"));
        browser->setFrameShape(QFrame::NoFrame);
        browser->setReadOnly(true);
        browser->setFocusPolicy(Qt::NoFocus);
        browser->viewport()->installEventFilter(this);
        browser->verticalScrollBar()->setVisible(false);
        browser->horizontalScrollBar()->setVisible(false);
        // 触屏：拖动=滚动编辑器，轻点=展开该表格源码
        new editor::TouchScroller(m_editor, browser->viewport(),
                                  [this, browser](const QPoint& p) {
                                      expandAt(m_overlays.indexOf(browser), p);
                                  },
                                  browser);
        m_overlays.append(browser);
    }
    while (m_overlays.size() > m_regions.size()) {
        m_overlays.last()->deleteLater();
        m_overlays.removeLast();
    }

    // 渲染内容并实测高度。折叠区高度下限 = 渲染内容高度（表格全览不被
    // 裁剪）；折叠行通过透明文字字号（rowPointSize）在真实排版里把空间
    // 撑出来，覆盖层高度 = 实测折叠高度，与真实排版严格等高。
    const qreal browserWidth = qMax(120.0, m_editor->viewport()->width() - 40.0);
    for (int i = 0; i < m_regions.size(); ++i) {
        QTextBrowser* overlay = m_overlays.at(i);

        QStringList rows;
        for (int line = m_regions.at(i).firstLine;
             line <= m_regions.at(i).lastLine; ++line) {
            const QTextBlock row = m_editor->document()->findBlockByNumber(line);
            if (row.isValid())
                rows.append(row.text());
        }

        const editor::ThemeTokens& t = editor::ThemeManager::tokens();
        const QColor codeBg = t.currentLineBackground;
        const QColor border = editor::ThemeManager::syntaxColor(QStringLiteral("punctuation"));
        overlay->document()->setDefaultStyleSheet(
            QStringLiteral("body { color:%1; margin: 2px 6px; } "
                           "td, th { color:%1; background-color:%2; "
                           "border: 1px solid %3; padding: 2px 12px; } a { color: %1; }")
                .arg(t.editorForeground.name(), codeBg.name(), border.name()));
        // 覆盖层文档会被 QTextEdit 排版到自身视口宽度（= 几何宽度），
        // 测量宽度必须与之一致，否则高度对不上
        overlay->document()->setTextWidth(browserWidth);
        overlay->setHtml(markdown::renderToHtml(rows.join(QLatin1Char('\n'))).html);

        m_regions[i].htmlHeightPx = qMax(qreal(24.0),
                                         overlay->document()->size().height());
        m_regions[i].pointToPixel = m_pointToPixel;

        // 初始估算：渲染高度均摊到每行，按 pt->px 比例换算成透明字号
        const int rowCount = m_regions.at(i).lastLine - m_regions.at(i).firstLine + 1;
        m_regions[i].rowPointSize =
            qBound(1.0,
                   m_regions.at(i).htmlHeightPx / rowCount / qMax(0.5, m_pointToPixel),
                   64.0);
    }

    // 应用字号 -> 实测折叠高度（updateActive 内部带收敛循环）
    updateActive(m_activeFirst, m_activeLast, m_activeDragging);
}

void TableOverlayManager::updateActive(int firstLine, int lastLine, bool dragging,
                                       const QPoint& dragPos)
{
    if (firstLine > lastLine)
        std::swap(firstLine, lastLine);
    m_activeFirst = firstLine;
    m_activeLast = lastLine;
    m_activeDragging = dragging;

    // 展开集合 = 选区/光标触及的区域，立即生效、立即退出（选区一离开
    // 表格就恢复渲染）。唯一的例外：拖选中光标扫过表格边缘时，同一
    // 鼠标位置在展开态映射到"表格之后"、在折叠态映射到"表格之内"，
    // 按块映射立即折叠会与再次展开互相触发形成鬼畜振荡——此时改用
    // 与状态无关的"折叠足迹视口矩形"判定：鼠标仍在足迹内则保持展开，
    // 离开足迹立即折叠。
    QSet<int> desired;
    for (int i = 0; i < m_regions.size(); ++i) {
        if (m_regions.at(i).firstLine <= lastLine
            && m_regions.at(i).lastLine >= firstLine)
            desired.insert(i);
    }

    QSet<int> expanded;
    for (int i = 0; i < m_regions.size(); ++i) {
        bool keep = desired.contains(i);
        if (!keep && dragging && m_expanded.contains(i)) {
            const int top = m_viewportTops.value(i, -1000000);
            if (dragPos.y() >= top - 6
                && dragPos.y() <= top + qRound(m_regions.at(i).collapsedHeightPx) + 6)
                keep = true; // 鼠标仍在表格足迹内：保持展开，杜绝边缘抖动
        }
        if (keep)
            expanded.insert(i);
    }
    m_expanded = expanded;

    // 应用字号 -> 实测折叠高度 -> 校正，循环到收敛（最多 3 轮）。
    // 字号换行高有取整与换行噪声；隐藏集合变化（光标进出表格）后
    // 也必须重测，否则覆盖层高度与真实占位脱节。
    const QAbstractTextDocumentLayout* docLayout =
        m_editor->document()->documentLayout();
    for (int iter = 0; iter < 3; ++iter) {
        m_hidden.clear();
        for (int i = 0; i < m_regions.size(); ++i) {
            if (!m_expanded.contains(i))
                m_hidden.append(m_regions.at(i));
        }
        if (m_highlighter) {
            m_highlighter->setHiddenRegions(m_hidden);
            // 折叠区间确定后统一施加固定行高（内部有递归守卫）
            m_highlighter->applyFoldHeights();
        }

        bool adjusted = false;
        for (int i = 0; i < m_regions.size(); ++i) {
            if (m_expanded.contains(i))
                continue; // 展开中的区域按源码行高排版，覆盖层反正隐藏
            qreal realFold = 0.0;
            for (int line = m_regions.at(i).firstLine;
                 line <= m_regions.at(i).lastLine; ++line) {
                const QTextBlock row = m_editor->document()->findBlockByNumber(line);
                if (row.isValid())
                    realFold += docLayout->blockBoundingRect(row).height();
            }
            // 覆盖层高度取较大值：绝不裁剪内容，也不无谓虚占
            m_regions[i].collapsedHeightPx =
                qMax(realFold, m_regions.at(i).htmlHeightPx);
            if (realFold + 1.0 < m_regions.at(i).htmlHeightPx) {
                m_regions[i].rowPointSize =
                    qBound(1.0,
                           m_regions.at(i).rowPointSize
                               * (m_regions.at(i).htmlHeightPx + 2.0)
                               / qMax(1.0, realFold),
                           64.0);
                adjusted = true;
            }
        }
        if (!adjusted)
            break;
    }

    // 折叠区高度确定后，再按已算好的视口坐标摆放覆盖层
    setRegionViewportTops(m_viewportTops);
}

// 唯一几何入口：CodeEditor 传入每个表格首行的视口 y 坐标。
void TableOverlayManager::setRegionViewportTops(const QVector<int>& viewportTops)
{
    m_viewportTops = viewportTops;
    if (viewportTops.size() != m_regions.size()
        || m_overlays.size() != m_regions.size())
        return;

    const int viewportHeight = m_editor->viewport()->height();
    const int viewportWidth = m_editor->viewport()->width();

    for (int i = 0; i < m_regions.size(); ++i) {
        QTextBrowser* overlay = m_overlays.at(i);
        const int top = viewportTops.at(i) + 2;
        const QRect geo(20, top,
                        qRound(viewportWidth - 40.0),
                        qRound(m_regions.at(i).collapsedHeightPx));

        // 完全滚出视口 或 正在编辑（展开源码）-> 隐藏
        // 超出视口（含上方溢出）一律隐藏，避免边缘出现错位残影
        const bool outOfView = geo.bottom() < 0 || geo.top() > viewportHeight
                               || geo.top() < -geo.height();
        const bool shouldShow = !m_expanded.contains(i) && !outOfView;

        // 展开中的表格：覆盖层隐藏且尺寸归零，避免其几何残留影响相邻表格
        if (m_expanded.contains(i)) {
            if (overlay->isVisible())
                overlay->hide();
            continue;
        }

        if (overlay->geometry() != geo)
            overlay->setGeometry(geo);
        if (overlay->isVisible() != shouldShow)
            overlay->setVisible(shouldShow);
        if (shouldShow)
            overlay->raise();
    }
}

// 点击/轻点覆盖层：把光标放到表格内对应行，触发整块展开为源码
void TableOverlayManager::expandAt(int index, const QPoint& localPos)
{
    if (index < 0 || index >= m_regions.size())
        return;
    const markdown::TableRegion& region = m_regions.at(index);
    const int rowHeight = 26;
    const int row = region.firstLine
                    + qBound(0, (localPos.y() - 4) / rowHeight,
                             region.lastLine - region.firstLine);
    const QTextBlock block = m_editor->document()->findBlockByNumber(row);
    if (!block.isValid())
        return;
    m_editor->setTextCursor(QTextCursor(block));
    m_editor->setFocus();
}

bool TableOverlayManager::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        for (int i = 0; i < m_overlays.size(); ++i) {
            if (watched == m_overlays.at(i)->viewport()) {
                auto* mouse = static_cast<QMouseEvent*>(event);
                expandAt(i, mouse->pos());
                return true;
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

void TableOverlayManager::clear()
{
    m_regions.clear();
    m_hidden.clear();
    m_viewportTops.clear();
    m_expanded.clear();
    m_activeFirst = m_activeLast = -1;
    m_activeDragging = false;
    for (QTextBrowser* overlay : m_overlays)
        overlay->hide();
    if (m_highlighter)
        m_highlighter->setHiddenRegions({});
}

} // namespace editor
