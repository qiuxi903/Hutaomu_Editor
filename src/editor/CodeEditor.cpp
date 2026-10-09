// Hutaomu Editor - Code editor widget: line numbers, current line highlight,
// syntax highlighting, find bar, language attachment.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "CodeEditor.h"

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QHash>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QPalette>
#include <QScrollBar>
#include <QTouchEvent>
#include <QTextBlock>

#include "FindBar.h"
#include "TouchScroller.h"
#include "themes/UserBackground.h"
#include "TableOverlayManager.h"
#include "markdown/MarkdownLiveHighlighter.h"
#include "syntax/TreeSitterHighlighter.h"

namespace editor {

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditor* editor)
        : QWidget(editor)
        , m_editor(editor)
    {
    }

    QSize sizeHint() const override
    {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        m_editor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor* m_editor;
};

CodeEditor::CodeEditor(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(m_tabWidth
                       * QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')));
    setTextInteractionFlags(Qt::TextEditorInteraction);

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::updateRequest,
            this, [this](const QRect&, int) { syncTableOverlayGeometry(); });
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CodeEditor::highlightCurrentLine);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CodeEditor::updateExtraSelections);
    connect(document(), &QTextDocument::contentsChange,
            this, [this](int, int, int) {
        if (m_liveHighlighter)
            m_tableScanTimer->start();
    });

    // 触屏：拖动滚动（带惯性）、轻点定位光标；捏合缩放走 native gesture
    m_touchScroller = new TouchScroller(this, viewport(), [this](const QPoint& p) {
        setTextCursor(cursorForPosition(p));
    }, this);
    // 垂直滚动条以"行"为单位：手指像素位移 ÷ 行高 = 滚动行数，保证 1:1 跟手。
    // 用字体度量而非首块高度——首块若是标题（更大字号）会严重高估行高。
    m_touchScroller->setVerticalPixelScaleFn([this] {
        const qreal h = qreal(fontMetrics().height());
        return h > 1.0 ? h : 1.0;
    });


    // MD 实时模式下的表格覆盖层管理器
    m_tableManager = new TableOverlayManager(this, this);
    m_tableScanTimer = new QTimer(this);
    m_tableScanTimer->setSingleShot(true);
    m_tableScanTimer->setInterval(150);
    connect(m_tableScanTimer, &QTimer::timeout, this, [this] {
        m_tableManager->refresh();
        syncTableOverlayGeometry();
    });

    m_findBar = new FindBar(this);
    connect(m_findBar, &FindBar::searchSelectionsChanged,
            this, [this] {
                m_searchSelections = m_findBar->searchSelections();
                updateExtraSelections();
            });

    applyTheme();
    updateLineNumberAreaWidth(0);
}

void CodeEditor::setTabWidth(int spaces)
{
    m_tabWidth = qBound(1, spaces, 16);
    // 制表位按空格数 × 空格宽（与设置页的"制表符宽度"一致）
    setTabStopDistance(m_tabWidth
                       * QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')));
    updateLineNumberAreaWidth(0);
}

void CodeEditor::setContentMaxWidth(int width)
{
    const int bounded = qMax(0, width);
    if (m_contentMaxWidth == bounded)
        return;
    m_contentMaxWidth = bounded;
    applyContentMargins();
}

void CodeEditor::setHeadingUnderline(bool on)
{
    if (m_liveHighlighter)
        m_liveHighlighter->setHeadingUnderline(on);
}

// 文本列居中：把多余宽度平分到视口左右，行号区跟着列起点走。
void CodeEditor::applyContentMargins()
{
    const int lineNumbers = lineNumberAreaWidth();
    int leftExtra = 0;
    int rightExtra = 0;
    if (m_contentMaxWidth > 0) {
        const int available = qMax(0, width() - lineNumbers);
        if (available > m_contentMaxWidth) {
            const int slack = available - m_contentMaxWidth;
            leftExtra = slack / 2;
            rightExtra = slack - leftExtra;
        }
    }
    m_contentLeftExtra = leftExtra;
    setViewportMargins(lineNumbers + leftExtra, 0, rightExtra, 0);
    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(cr.left() + leftExtra, cr.top(), lineNumbers,
                                  cr.height());
}

void CodeEditor::setEditorFont(const QFont& font)
{
    setFont(font);
    setTabStopDistance(m_tabWidth
                       * QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')));
    updateLineNumberAreaWidth(0);
    if (m_tableManager) {
        const qreal pointSize = qMax(qreal(1.0), qreal(font.pointSizeF()));
        m_tableManager->setPointToPixel(fontMetrics().height() / pointSize);
        m_tableManager->refresh();
        syncTableOverlayGeometry(); // 行高变了，覆盖层几何随之刷新
    }
}

void CodeEditor::setLanguage(const syntax::LanguageSpec* spec)
{
    if (m_language == spec)
        return;
    m_language = spec;

    if (m_highlighter) {
        m_highlighter->setDocument(nullptr); // 解绑避免悬挂
        delete m_highlighter;
        m_highlighter = nullptr;
    }
    if (m_language && m_language->hasHighlighting()) {
        m_highlighter = new syntax::TreeSitterHighlighter(document(), m_language);
    } else {
        // 切到无高亮语言时清掉既有颜色
        QList<QTextLayout::FormatRange> empty;
        QTextBlock block = document()->firstBlock();
        while (block.isValid()) {
            block.layout()->setFormats({});
            block = block.next();
        }
    }
    updateExtraSelections();
}

// 计算每个折叠表格首行在视口中的 y 坐标，交给覆盖层管理器定位。
//
// 锚点 = 首可见块的绘制位置（与行号栏同一公式，Qt 保证其准确）；其余块
// 从锚点出发按真实块高累加/回退。不能直接用 blockBoundingGeometry 的
// 绝对 y：QPlainTextDocumentLayout 懒布局下，从未绘制到的尾部块的绝对 y
// 是过期值（实测会全部堆在同一坐标），覆盖层会压住表格下方的内容。
// 按块高累加与 QPlainTextEdit 绘制内容的叠放方式同源，严格对齐；且
// 折叠行文字透明但保持自然字号（见 MarkdownLiveHighlighter），折叠区
// 的占位就是行高之和，无需任何平行模型。
void CodeEditor::syncTableOverlayGeometry()
{
    if (!m_liveHighlighter || !m_tableManager)
        return;
    const int count = m_tableManager->regionCount();
    if (count <= 0)
        return;

    const QVector<int> firstLines = m_tableManager->regionFirstLines();
    QHash<int, int> lineToRegion; // 表格首行 -> 区域序号
    for (int i = 0; i < firstLines.size(); ++i)
        lineToRegion.insert(firstLines.at(i), i);

    const QTextBlock anchor = firstVisibleBlock();
    if (!anchor.isValid())
        return;
    const qreal anchorY = blockBoundingGeometry(anchor)
                              .translated(contentOffset()).top();
    const qreal slack = viewport()->height() + 4000.0;
    QVector<int> tops(count, -1000000); // 默认推到视口外，覆盖层自行隐藏

    qreal y = anchorY;
    for (QTextBlock block = anchor; block.isValid(); block = block.next()) {
        if (y > slack)
            break;
        const int idx = lineToRegion.value(block.blockNumber(), -1);
        if (idx >= 0)
            tops[idx] = qRound(y);
        y += blockBoundingRect(block).height();
    }

    y = anchorY;
    for (QTextBlock block = anchor.previous(); block.isValid();
         block = block.previous()) {
        y -= blockBoundingRect(block).height();
        if (y < -slack)
            break;
        const int idx = lineToRegion.value(block.blockNumber(), -1);
        if (idx >= 0)
            tops[idx] = qRound(y);
    }

    m_tableManager->setRegionViewportTops(tops);
}


void CodeEditor::updateLiveActiveRange()
{
    // 高亮器可能已被移除（切到源码/分栏模式），必须判空
    if (!m_liveHighlighter)
        return;
    QTextCursor cursor = textCursor();
    int first = cursor.blockNumber();
    int last = first;
    if (cursor.hasSelection()) {
        first = document()->findBlock(cursor.selectionStart()).blockNumber();
        last = document()->findBlock(qMax(cursor.selectionStart(),
                                          cursor.selectionEnd() - 1)).blockNumber();
    }

    if (m_tableManager) {
        // 拖选进行中：选区触及的表格展开为源码（用户要看到选了什么），
        // 选区一退出立即恢复表格；边缘处用鼠标足迹判定防抖
        // （见 TableOverlayManager::updateActive）。
        m_tableManager->updateActive(first, last, m_mouseSelecting,
                                     m_mouseSelecting ? m_dragPos : QPoint());
        syncTableOverlayGeometry();
    }
    // 拖选中冻结渲染态（原始/渲染切换也伴随行高变化），释放时统一结算
    if (!m_mouseSelecting)
        m_liveHighlighter->setActiveRange(first, last);
}

void CodeEditor::setLiveMarkdown(bool on)
{
    if (on && !m_liveHighlighter) {
        m_liveHighlighter = new markdown::MarkdownLiveHighlighter(document());
        disconnect(m_liveRangeConn);
        m_liveRangeConn = connect(this, &QPlainTextEdit::cursorPositionChanged,
                                  this, &CodeEditor::updateLiveActiveRange);
        m_tableManager->setHighlighter(m_liveHighlighter);
        m_tableManager->refresh();
        updateLiveActiveRange(); // 应用当前光标/选区的展开与渲染态
        // 覆盖层宽度以视口为准，折叠行的大字号不再撑出横向滚动
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else if (!on && m_liveHighlighter) {
        disconnect(m_liveRangeConn);
        delete m_liveHighlighter;
        m_liveHighlighter = nullptr;
        m_tableManager->setHighlighter(nullptr);
        m_tableManager->clear();
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        // 清掉残留的装饰格式
        QTextBlock block = document()->firstBlock();
        while (block.isValid()) {
            block.layout()->setFormats({});
            block = block.next();
        }
        updateExtraSelections();
    }
}

void CodeEditor::openFind(bool withReplace)
{
    m_findBar->open(withReplace);
}

void CodeEditor::jumpToLine(int line, int column, int selectLength)
{
    QTextBlock block = document()->findBlockByNumber(qMax(0, line - 1));
    if (!block.isValid())
        return;
    QTextCursor cursor(block);
    const int pos = qMin(block.position() + qMax(0, column),
                         block.position() + block.length() - 1);
    cursor.setPosition(pos);
    if (selectLength > 0)
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, selectLength);
    setTextCursor(cursor);

    // 垂直滚动条以"行"为单位，但"行号 -> 像素"映射随块高（大标题、
    // 折叠表格）非线性变化，按行数换算目标值会跳偏。用 Qt 自带的
    // centerCursor（内部按像素精确换算）求出目标滚动值，再平滑动画。
    // 只要有位移就动画：行数差很小也可能对应可观的像素距离。
    const int from = verticalScrollBar()->value();
    const bool oldCenterOnScroll = centerOnScroll();
    setCenterOnScroll(true);
    centerCursor();
    setCenterOnScroll(oldCenterOnScroll);
    const int to = verticalScrollBar()->value();
    if (to != from) {
        verticalScrollBar()->setValue(from);
        auto* animation = new QPropertyAnimation(verticalScrollBar(), "value", this);
        animation->setDuration(240);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        animation->setStartValue(from);
        animation->setEndValue(to);
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    }
    setFocus();
}

void CodeEditor::applyTheme()
{
    const ThemeTokens& t = ThemeManager::tokens();

    QPalette pal = palette();
    pal.setColor(QPalette::Base, t.editorBackground);
    pal.setColor(QPalette::Text, t.editorForeground);
    pal.setColor(QPalette::Highlight, t.selectionBackground);
    pal.setColor(QPalette::HighlightedText, t.editorForeground);
    // viewport 是独立子控件，必须一并设置，否则它会保留旧调色板
    setPalette(pal);
    viewport()->setPalette(pal);

    if (m_highlighter)
        m_highlighter->refreshTheme();
    rebuildEditorBackground();
    highlightCurrentLine();
    m_lineNumberArea->update();
}

// 主题背景图：把图片按 mode/opacity 合成到视口大小的纹理上，作为 palette Base
// 画刷（QPlainTextEdit 用它填充视口，因此图片位于文字之下）。无背景图时回到纯色。
void CodeEditor::rebuildEditorBackground()
{
    const ThemeDefinition theme = ThemeManager::currentTheme();
    const ThemeBackground& spec = theme.background;

    QPalette pal = palette();
    const QColor baseColor = theme.color(QStringLiteral("editorBg"));

    const auto useSolid = [&]() {
        m_backgroundSource = QImage();
        m_backgroundKey.clear();
        pal.setBrush(QPalette::Base, QBrush(baseColor));
        setPalette(pal);
        viewport()->setPalette(pal);
    };

    // 用户自定义背景优先（设置页「背景…」选的），主题自带背景次之
    const editor::background::UserBackground userBg = editor::background::current();
    QString path;
    QString mode;
    qreal opacity;
    if (userBg.isValid() && QFileInfo::exists(userBg.imagePath)) {
        path = userBg.imagePath;
        mode = userBg.mode;
        opacity = userBg.opacity;
    } else if (spec.isValid()) {
        path = ThemeManager::assetPath(spec.image);
        mode = spec.mode;
        opacity = spec.opacity;
    } else {
        useSolid();
        return;
    }
    if (path.isEmpty()) {
        useSolid();
        return;
    }

    const QString key = ThemeManager::theme() + QLatin1Char('|') + path;
    if (key != m_backgroundKey) {
        m_backgroundSource = QImage(path);
        m_backgroundKey = key;
    }
    const QSize viewportSize = viewport()->size();
    if (m_backgroundSource.isNull() || viewportSize.isEmpty()) {
        useSolid();
        return;
    }

    const qreal dpr = viewport()->devicePixelRatioF();
    QImage canvas(viewportSize * dpr, QImage::Format_ARGB32_Premultiplied);
    canvas.setDevicePixelRatio(dpr);
    QPainter painter(&canvas);
    painter.fillRect(QRect(QPoint(0, 0), viewportSize), baseColor);
    painter.setOpacity(qBound(qreal(0.0), opacity, qreal(1.0)));
    const QRect viewRect(QPoint(0, 0), viewportSize);
    if (mode == QLatin1String("stretch")) {
        painter.drawImage(viewRect, m_backgroundSource);
    } else if (mode == QLatin1String("center")) {
        const QSize imageSize = m_backgroundSource.size() / m_backgroundSource.devicePixelRatio();
        painter.drawImage(QPoint((viewportSize.width() - imageSize.width()) / 2,
                                 (viewportSize.height() - imageSize.height()) / 2),
                          m_backgroundSource);
    } else { // tile（默认）
        for (int y = 0; y < viewportSize.height(); y += m_backgroundSource.height())
            for (int x = 0; x < viewportSize.width(); x += m_backgroundSource.width())
                painter.drawImage(QPoint(x, y), m_backgroundSource);
    }
    painter.end();

    pal.setBrush(QPalette::Base, QBrush(QPixmap::fromImage(canvas)));
    setPalette(pal);
    // 背景图同样要落到 viewport 的调色板上才会被绘制
    viewport()->setPalette(pal);
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    for (int max = qMax(1, blockCount()); max >= 10; max /= 10)
        ++digits;
    // 预留一位，避免输入时行号栏抖动
    return 14 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * (digits + 1);
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    applyContentMargins();
}

void CodeEditor::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

bool CodeEditor::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::NativeGesture) {
        auto* gesture = static_cast<QNativeGestureEvent*>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture && gesture->value() != 0) {
            emit zoomGestureRequested(1.0 + gesture->value());
            return true;
        }
    }

    // 跟踪左键拖选：拖选期间渲染态冻结；鼠标位置供表格边缘防抖判定
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
        if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            m_mouseSelecting = true;
            m_dragPos = static_cast<QMouseEvent*>(event)->position().toPoint();
        }
        break;
    case QEvent::MouseMove:
        if (m_mouseSelecting)
            m_dragPos = static_cast<QMouseEvent*>(event)->position().toPoint();
        break;
    case QEvent::MouseButtonRelease:
        if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton
            && m_mouseSelecting) {
            m_mouseSelecting = false;
            updateLiveActiveRange(); // 拖选结束：按最终光标/选区一次性结算
        }
        break;
    default:
        break;
    }
    return QPlainTextEdit::viewportEvent(event);
}

void CodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    applyContentMargins(); // 行号区位置 + 居中限宽的留白
    if (m_findBar)
        m_findBar->reposition();
    // 宽度变了，表格渲染高度随之变化，需重测折叠高度并重新摆放覆盖层
    if (m_liveHighlighter && m_tableManager)
        m_tableScanTimer->start();
    syncTableOverlayGeometry();
    if (!m_backgroundKey.isEmpty())
        rebuildEditorBackground(); // 视口尺寸变了，纹理要重铺
}

void CodeEditor::updateExtraSelections()
{
    QList<QTextEdit::ExtraSelection> selections;

    // 当前行高亮
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection line;
        line.format.setBackground(ThemeManager::tokens().currentLineBackground);
        line.format.setProperty(QTextFormat::FullWidthSelection, true);
        line.cursor = textCursor();
        line.cursor.clearSelection();
        selections.append(line);
    }

    // 搜索匹配高亮（FindBar 提供）
    selections.append(m_searchSelections);

    setExtraSelections(selections);
}

void CodeEditor::highlightCurrentLine()
{
    updateExtraSelections();
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    const ThemeTokens& t = ThemeManager::tokens();

    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), t.lineNumberBackground);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    const int currentLine = textCursor().blockNumber();
    // 活动行 = 当前光标行（源码模式下选区行也展开）；折叠区的行号不绘制
    const bool liveMode = m_liveHighlighter != nullptr;

    while (block.isValid() && top <= event->rect().bottom()) {
        bool hiddenByTable = false;
        if (liveMode) {
            for (const markdown::TableRegion& region : m_tableManager->hiddenRegions()) {
                if (blockNumber > region.firstLine
                    && blockNumber <= region.lastLine) {
                    hiddenByTable = true;
                    break;
                }
            }
        }
        if (block.isVisible() && bottom >= event->rect().top() && !hiddenByTable) {
            const QString number = QString::number(blockNumber + 1);
            painter.setPen(blockNumber == currentLine
                               ? t.lineNumberActiveForeground
                               : t.lineNumberForeground);
            painter.drawText(0, top, m_lineNumberArea->width() - 8,
                             fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

} // namespace editor
