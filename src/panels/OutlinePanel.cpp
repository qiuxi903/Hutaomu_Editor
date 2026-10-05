// Hutaomu Editor - Document outline (Markdown headings) as a tree.
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "OutlinePanel.h"

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "editor/TouchScroller.h"

namespace panels {

namespace {
// 主题应用后重建调色板相关缓存（背景色随高亮色联动）
void refreshHighlightColor(QBrush& brush, QWidget* w)
{
    QColor sel = w->palette().color(QPalette::Highlight);
    if (!sel.isValid())
        sel = QColor(0x35, 0x73, 0x62);
    sel.setAlpha(46);
    brush = QBrush(sel);
}
} // namespace

OutlinePanel::OutlinePanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("outlinePanel"));
    refreshHighlightColor(m_activeBackground, this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("outlineHeader"));
    m_header = header;
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 8, 6, 8);
    m_titleLabel = new QLabel(tr("大纲"), header);
    m_titleLabel->setObjectName(QStringLiteral("outlineTitle"));
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();
    layout->addWidget(header);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("outlineTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setRootIsDecorated(false);
    m_tree->setIndentation(16);
    m_tree->setUniformRowHeights(true);
    m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_tree, 1);

    // 触屏：大纲树支持拖动滚动（像素单位，无需换算）；轻点 = 选中并跳转
    m_touchScroller = new editor::TouchScroller(m_tree, m_tree->viewport(),
                                                [this](const QPoint& p) {
                                                    if (QTreeWidgetItem* item = m_tree->itemAt(p)) {
                                                        m_tree->setCurrentItem(item);
                                                        emit headingActivated(
                                                            item->data(0, Qt::UserRole).toInt());
                                                    }
                                                },
                                                this);
    static_cast<editor::TouchScroller*>(m_touchScroller)->setPanThreshold(16);

    m_headingRegex = QRegularExpression(QStringLiteral("^(#{1,6})\\s+(.+?)\\s*#*\\s*$"));

    connect(m_tree, &QTreeWidget::itemClicked,
            this, [this](QTreeWidgetItem* item, int) {
                emit headingActivated(item->data(0, Qt::UserRole).toInt());
            });
    connect(m_tree, &QTreeWidget::itemActivated,
            this, [this](QTreeWidgetItem* item, int) {
                emit headingActivated(item->data(0, Qt::UserRole).toInt());
            });
}

bool OutlinePanel::event(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::StyleChange) {
        refreshHighlightColor(m_activeBackground, this);
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items.at(i)->font(0).bold())
                m_items.at(i)->setBackground(0, m_activeBackground);
        }
    }
    return QWidget::event(event);
}

void OutlinePanel::updateFromText(const QString& markdown)
{
    // 解析标题（行号、文本、层级）。与上次完全一致时跳过重建：
    // 打字/防抖刷新期间频繁 clear+重建会让大纲闪动、滚动位置丢失。
    QList<int> lines;
    QList<QString> texts;
    QList<int> levels;

    const QStringList lines_ = markdown.split(QLatin1Char('\n'));
    QList<QTreeWidgetItem*> stack;
    QList<int> stackLevels;

    int fenceState = 0;
    for (int i = 0; i < lines_.size(); ++i) {
        const QString& line = lines_.at(i);
        const QString trimmed = line.trimmed();
        const bool backtick = trimmed.startsWith(QStringLiteral("```"));
        const bool tilde = trimmed.startsWith(QStringLiteral("~~~"));
        if (fenceState == 0 && (backtick || tilde))
            fenceState = backtick ? 1 : 2;
        else if ((fenceState == 1 && backtick) || (fenceState == 2 && tilde))
            fenceState = 0;
        if (fenceState != 0)
            continue;

        const QRegularExpressionMatch m = m_headingRegex.match(line);
        if (!m.hasMatch())
            continue;

        lines.append(i + 1);
        texts.append(m.captured(2));
        levels.append(m.captured(1).size());
    }

    if (lines == m_lines && texts == m_texts && levels == m_levels)
        return;

    int previousLine = -1;
    if (QTreeWidgetItem* current = m_tree->currentItem())
        previousLine = current->data(0, Qt::UserRole).toInt();
    const int savedScroll = m_tree->verticalScrollBar()->value();

    m_tree->clear();
    m_items.clear();
    m_lines = lines;
    m_texts = texts;
    m_levels = levels;

    // 层级栈：栈顶为最近的低级别标题
    stack.append(m_tree->invisibleRootItem());
    stackLevels.append(0);
    for (int i = 0; i < lines.size(); ++i) {
        const int level = levels.at(i);
        while (stackLevels.size() > 1 && stackLevels.last() >= level) {
            stack.removeLast();
            stackLevels.removeLast();
        }
        QTreeWidgetItem* item = makeItem(texts.at(i), level, lines.at(i), stack.last());
        stack.append(item);
        stackLevels.append(level);
    }

    m_tree->expandAll();

    // 恢复之前的章节定位；章节未变时保持滚动位置（不重置到顶部/居中）。
    // setCurrentLine 内部对树几何的触碰已是延迟一拍（Qt 6.10 缓存规避）。
    int restored = -1;
    if (previousLine > 0) {
        for (int i = 0; i < m_lines.size(); ++i) {
            if (m_lines.at(i) <= previousLine)
                restored = i;
            else
                break;
        }
    }
    const int restoreLine = restored >= 0 ? m_lines.at(restored) : -1;
    const bool keepScroll = restoreLine > 0 && m_followedLine == restoreLine;
    setCurrentLine(restoreLine);
    if (keepScroll)
        m_tree->verticalScrollBar()->setValue(savedScroll);
}

QTreeWidgetItem* OutlinePanel::makeItem(const QString& text, int level, int lineNumber,
                                        QTreeWidgetItem* parent)
{
    auto* item = new QTreeWidgetItem(QStringList(QStringLiteral("H%1  %2").arg(level).arg(text)));
    item->setData(0, Qt::UserRole, lineNumber);
    item->setToolTip(0, tr("第 %1 行").arg(lineNumber));
    if (!parent)
        parent = m_tree->invisibleRootItem();
    parent->addChild(item);
    m_items.append(item);
    m_lines.append(lineNumber);
    return item;
}

int OutlinePanel::count() const
{
    return m_items.size();
}

int OutlinePanel::lineNumberAt(int index) const
{
    return (index >= 0 && index < m_lines.size()) ? m_lines.at(index) : -1;
}

void OutlinePanel::setCurrentLine(int lineNumber)
{
    if (m_items.isEmpty()) {
        m_followedLine = -1;
        return;
    }
    int current = -1;
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines.at(i) <= lineNumber)
            current = i;
        else
            break;
    }

    if (current < 0) {
        for (QTreeWidgetItem* it : std::as_const(m_items))
            it->setBackground(0, QBrush());
        m_followedLine = -1;
        return;
    }

    // 注意：不用 QTreeWidget::setCurrentItem —— 实测 Qt 6.10 在 clear()/
    // expandAll() 之后其内部视图缓存短暂持有已删除项，任何触碰条目几何
    // 或排序比较的 API（setCurrentItem/visualItemRect）都可能解引用悬空
    // 指针（QTreeWidgetItem::operator< 读到 0xbaadf00d，gdb 已证），
    // 连点大纲/连续防抖刷新即崩。
    // 因此本函数立即只改整数状态（m_followedLine），所有对树条目的
    // 写入/几何查询统一延迟到下一拍事件循环——彼时 clear()+expandAll()
    // 之后的内部缓存已重建，且以"当次行号"重取条目，绝不持有旧指针。
    const int targetLine = m_lines.at(current);
    if (m_followedLine == targetLine)
        return;
    m_followedLine = targetLine;
    QMetaObject::invokeMethod(this, [this, targetLine] {
        applyHighlight(targetLine);
    }, Qt::QueuedConnection);
}

// 按 1-based 行号找到条目并应用加粗/底色/滚动。以行号索引当次 m_items，
// 不跨 clear 持有任何 QTreeWidgetItem 指针。
//
// 滚动定位不用 visualItemRect/scrollToItem：Qt 6.10 的 QTreeWidget 在
// clear()+expandAll() 后的若干个事件循环内，这两个 API 仍会踩到内部
// 缓存的悬空项（QTreeWidgetItem::operator< 解引用 0xbaadf00d）。
// 树关闭了排序、行高统一（setUniformRowHeights），条目位置可精确推算：
//   y = 头部高 + 前序可见条目数 × 行高，滚动条单位即像素。
void OutlinePanel::applyHighlight(int lineNumber)
{
    int current = -1;
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines.at(i) <= lineNumber)
            current = i;
        else
            break;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        QTreeWidgetItem* it = m_items.at(i);
        QFont font = it->font(0);
        font.setBold(i == current);
        it->setFont(0, font);
        it->setBackground(0, i == current ? m_activeBackground : QBrush());
    }
    if (current < 0)
        return;

    // 可见性判断与滚动全部用推算，不触碰 QTreeWidget 内部条目缓存
    const int rowHeight = m_tree->uniformRowHeights()
                              ? qMax(1, m_tree->sizeHintForRow(0))
                              : 20;
    if (rowHeight <= 1)
        return; // 树尚未布局，跳过本次滚动（下一章节能 correctly 滚）
    const int y = current * rowHeight;
    const int viewH = m_tree->viewport()->height();
    const int scroll = m_tree->verticalScrollBar()->value();
    if (y < scroll || y + rowHeight > scroll + viewH)
        m_tree->verticalScrollBar()->setValue(qMax(0, y - viewH / 2));
}

void OutlinePanel::clearOutline()
{
    m_tree->clear();
    m_items.clear();
    m_lines.clear();
    m_texts.clear();
    m_levels.clear();
    m_followedLine = -1;
}

} // namespace panels
