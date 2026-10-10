// Hutaomu Editor - Main window shell (VS Code style layout).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "MainWindow.h"

#include "app/FileAssociations.h"

#include "DropZoneOverlay.h"
#include "PanelColumn.h"
#include "viewers/ViewerFactory.h"
#include "PanelDragMover.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QCheckBox>
#include <QProcess>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QActionGroup>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#endif

#include "ActivityBar.h"
#include "TitleBar.h"
#include "UiFx.h"
#include "UiAnimations.h"
#include "BeautifulDialog.h"
#include "core/Document.h"
#include "editor/CodeEditor.h"
#include "editor/EditorCommands.h"
#include "panels/ExplorerPanel.h"
#include "plugins/PluginPagePanel.h"
#include "panels/MarkdownPreview.h"
#include "panels/OutlinePanel.h"
#include "panels/SearchPanel.h"
#include "CommandPalette.h"
#include "QuickOpenDialog.h"
#include "SettingsDialog.h"
#include "settings/AppSettings.h"
#include "syntax/LanguageRegistry.h"
#include "themes/IconLoader.h"
#include "themes/ThemeManager.h"

namespace app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("MainWindow"));
    setAcceptDrops(true);
    setMinimumSize(720, 480);

    buildChrome();

    // --- Status bar (VS Code order, right to left) ---
    m_languageLabel = new QLabel(this);
    m_encodingLabel = new QLabel(this);
    m_eolLabel = new QLabel(this);
    m_positionLabel = new QLabel(this);
    for (QLabel* label : { m_positionLabel, m_eolLabel, m_encodingLabel, m_languageLabel })
        label->setContentsMargins(8, 0, 8, 0);
    statusBar()->addPermanentWidget(m_positionLabel);
    statusBar()->addPermanentWidget(m_eolLabel);
    statusBar()->addPermanentWidget(m_encodingLabel);
    statusBar()->addPermanentWidget(m_languageLabel);
    statusBar()->showMessage(tr("就绪"));

    buildMenus();
    setupPanelDrag();

    const auto& s = settings::AppSettings::instance();
    if (!s.mainWindowGeometry.isEmpty())
        restoreGeometry(s.mainWindowGeometry);
    if (!s.mainWindowState.isEmpty())
        restoreState(s.mainWindowState);
    // 注意：不恢复 splitterState——旧版本布局树不同，恢复会把大纲挤没。
    m_splitter->setSizes({ 280, 900 });
    if (s.previewPosition == QStringLiteral("bottom"))
        m_editorSplitter->setOrientation(Qt::Vertical);
    m_activityBar->explorerButton->setChecked(s.sidebarVisible);
    applyLayoutPositions();

    // Frameless chrome must be applied before the window shows.
    applyWindowChrome();

    if (!s.workspacePath.isEmpty() && QDir(s.workspacePath).exists()) {
        m_explorer->setWorkspace(s.workspacePath);
        m_search->setWorkspace(s.workspacePath);
    }

    newDocument();
    updateWindowTitle();
    applyMarkdownViewMode();
    // 首次启动的文件关联引导（窗口起来后再弹，只提示一次）
    QTimer::singleShot(1200, this, &MainWindow::maybePromptFileAssociations);
    refreshPluginContributions();

    // 启动时也要应用当前主题声明的界面形态（T4）——主题在 main() 里就已生效，
    // 若只在这里等"主题切换"事件，带 layout.json 的主题在启动后不会改形态。
    applyThemeLayout();
}

void MainWindow::buildChrome()
{
    m_titleBar = new TitleBar(this);
    setMenuWidget(m_titleBar);

    auto* central = new QWidget(this);
    auto* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_activityBar = new ActivityBar(central);
    mainLayout->addWidget(m_activityBar);

    // ---- 侧边栏 = 左右两条"卡片列"（VS Code sections 模型）----
    // 每个面板是一张带标题栏的卡片；同列多卡全部可见、垂直排布；
    // 拖动卡片标题栏可换列/重排，拖到活动栏 = 收起为按钮。
    m_leftColumn = new PanelColumn(QStringLiteral("left"), central);
    m_rightColumn = new PanelColumn(QStringLiteral("right"), central);
    m_leftColumn->setMinimumWidth(180);
    m_rightColumn->setMinimumWidth(180);

    m_explorer = new panels::ExplorerPanel(this);
    m_explorer->setMinimumWidth(180);
    m_search = new panels::SearchPanel(this);
    m_outline = new panels::OutlinePanel(this);
    m_outline->setMinimumHeight(80);

    registerSidePanel(QStringLiteral("explorer"), m_explorer);
    registerSidePanel(QStringLiteral("search"), m_search);
    registerSidePanel(QStringLiteral("outline"), m_outline);
    // 同列多卡时允许压缩（否则搜索/大纲各占一半高度，挤占文件树）
    m_explorer->setMinimumHeight(120);
    m_search->setMinimumHeight(120);
    m_outline->setMinimumHeight(120);

    m_splitter = new QSplitter(Qt::Horizontal, central);
    m_splitter->setObjectName(QStringLiteral("mainSplitter"));
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(5);
    m_splitter->addWidget(m_leftColumn);
    m_splitter->addWidget(m_rightColumn);

    m_editorSplitter = new QSplitter(Qt::Horizontal, m_splitter);
    m_editorSplitter->setObjectName(QStringLiteral("editorSplitter"));
    m_editorSplitter->setHandleWidth(5);
    if (settings::AppSettings::instance().previewPosition == QStringLiteral("bottom"))
        m_editorSplitter->setOrientation(Qt::Vertical);

    m_preview = new panels::MarkdownPreview(m_editorSplitter);
    m_preview->setMinimumHeight(120);
    m_preview->setVisible(false);

    // --- Editor area: tabs with a "+" corner button ---
    m_tabs = new QTabWidget(m_splitter);
    m_tabs->setObjectName(QStringLiteral("editorTabs"));
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->setUsesScrollButtons(true);
    m_tabs->setElideMode(Qt::ElideRight);

    m_newTabButton = new QToolButton(m_tabs->tabBar());
    m_newTabButton->setObjectName(QStringLiteral("newTabButton"));
    m_newTabButton->setToolTip(tr("新建标签 (Ctrl+N)"));
    m_newTabButton->setCursor(Qt::PointingHandCursor);
    m_newTabButton->setAutoRaise(true);
    // 先给近似高度，首帧显示后按标签栏实际高度精确对齐
    m_newTabButton->setFixedSize(32, 34);
    m_newTabButton->setIcon(editor::IconLoader::load(
        QStringLiteral("tab-add"),
        editor::ThemeManager::tokens().editorForeground,
        editor::ThemeManager::tokens().editorBackground, 14));
    m_newTabButton->setIconSize(QSize(14, 14));
    connect(m_newTabButton, &QToolButton::clicked,
            this, &MainWindow::newDocument);
    m_newTabButton->raise();
    m_tabs->tabBar()->installEventFilter(this);

    connect(m_tabs, &QTabWidget::tabCloseRequested,
            this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged,
            this, [this](int index) {
                if (auto* e = editorAt(index))
                    ui::fadeIn(e);
                updateWindowTitle();
                updateStatusBar();
                applyMarkdownViewMode();
            });

    m_editorSplitter->addWidget(m_tabs);
    m_editorSplitter->addWidget(m_preview);
    m_editorSplitter->setStretchFactor(0, 2);
    m_editorSplitter->setStretchFactor(1, 1);
    m_editorSplitter->setSizes({ 640, 320 });
    m_splitter->addWidget(m_editorSplitter);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({ 280, 900 });
    mainLayout->addWidget(m_splitter, 1);

    // 预览/大纲刷新防抖
    m_refreshDebounce = new QTimer(this);
    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(300);
    connect(m_refreshDebounce, &QTimer::timeout,
            this, &MainWindow::updatePreviewAndOutline);

    setCentralWidget(central);

    // 大纲注册为活动栏页面（与资源管理器/搜索同级，可拖拽收起/恢复）
    m_activityBar->addPanelButton(QStringLiteral("outline"),
                                  QStringLiteral("activity-extensions"),
                                  tr("大纲"));
    connect(m_activityBar, &ActivityBar::panelToggled,
            this, &MainWindow::handlePanelToggled);
    m_activityBar->setPanelChecked(
        QStringLiteral("outline"),
        settings::AppSettings::instance().outlineSide != QStringLiteral("right"));

    connect(m_activityBar, &ActivityBar::explorerToggled,
            this, [this](bool checked) {
                if (checked && isPanelCollapsed(QStringLiteral("explorer")))
                    expandPanelFromActivityBar(QStringLiteral("explorer"));
            });
    connect(m_activityBar, &ActivityBar::searchToggled,
            this, [this](bool checked) {
                if (checked) {
                    if (isPanelCollapsed(QStringLiteral("search")))
                        expandPanelFromActivityBar(QStringLiteral("search"));
                    m_search->focusInput();
                }
            });
    connect(m_search, &panels::SearchPanel::resultActivated,
            this, [this](const QString& path, int line, int column, int length) {
                openPath(path);
                if (auto* e = currentEditor())
                    e->jumpToLine(line, column, length);
            });
    connect(m_activityBar, &ActivityBar::aboutRequested,
            this, &MainWindow::showAbout);
    connect(m_explorer, &panels::ExplorerPanel::fileActivated,
            this, &MainWindow::openPath);
    connect(m_explorer, &panels::ExplorerPanel::openFolderRequested,
            this, &MainWindow::openFolderDialog);

    // 大纲标题点击 -> 跳到源码对应行。
    // 队列投递：点击发生在 QTreeWidget 自己的鼠标事件处理中，若同步
    // 跳转（改编辑器光标 -> 回头改大纲当前项/滚动）会在树的事件处理
    // 尚未结束时重入修改它，连点几下即可复现崩溃。
    connect(m_outline, &panels::OutlinePanel::headingActivated,
            this, [this](int line) {
                if (line > 0 && currentEditor())
                    currentEditor()->jumpToLine(line);
            }, Qt::QueuedConnection);

    // 预览标题锚点 -> 跳到源码对应章节
    connect(m_preview, &panels::MarkdownPreview::headingClicked,
            this, [this](int headingIndex) {
                const int line = m_outline->lineNumberAt(headingIndex);
                if (line > 0 && currentEditor())
                    currentEditor()->jumpToLine(line);
            });

    // 编辑 <-> 预览 滚动同步（按比例）
    connect(m_preview->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
                if (m_syncingScroll || !m_preview->isVisible())
                    return;
                auto* editor = currentEditor();
                if (!editor || !documentFor(editor)
                    || !documentFor(editor)->language()
                    || documentFor(editor)->language()->id != QStringLiteral("markdown"))
                    return;
                QScrollBar* editorBar = editor->verticalScrollBar();
                const int editorMax = editorBar->maximum();
                const int previewMax = m_preview->verticalScrollBar()->maximum();
                if (previewMax <= 0)
                    return;
                m_syncingScroll = true;
                editorBar->setValue(qRound(value / qreal(previewMax) * editorMax));
                m_syncingScroll = false;
            });
}

void MainWindow::applyWindowChrome()
{
    // Keep the native WS_THICKFRAME window styles (so maximize, snapping,
    // and DWM shadows behave) and only strip the visual frame via
    // WM_NCCALCSIZE; the TitleBar menu widget draws its own chrome.
}

// 标题栏命中判定：装饰区/空白 → 原生标题（Windows 提供拖动与 Snap）；
// 窗口按钮与菜单项 → 客户端区（保持可点击）。
bool MainWindow::isCaptionHit(const QPoint& globalPos) const
{
    if (!m_titleBar)
        return false;
    const QPoint local = m_titleBar->mapFromGlobal(globalPos);
    if (!m_titleBar->rect().contains(local))
        return false;
    QWidget* child = m_titleBar->childAt(local);
    if (qobject_cast<QToolButton*>(child))
        return false; // 最小化/最大化/关闭
    if (auto* bar = qobject_cast<QMenuBar*>(child))
        return bar->actionAt(bar->mapFromGlobal(globalPos)) == nullptr;
    return true;
}

// Keeps the "+" button glued to the right edge of the tab bar row.
bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_tabs->tabBar() && event->type() == QEvent::Resize)
        syncNewTabButton();
    return QMainWindow::eventFilter(watched, event);
}

#ifdef Q_OS_WIN
// Removes the native frame (WM_NCCALCSIZE) and reports narrow edge bands as
// resize borders (WM_NCHITTEST) so the frameless window keeps native resize,
// cursors, and snapping.
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    MSG* msg = static_cast<MSG*>(message);
    if (eventType != "windows_generic_MSG")
        return QMainWindow::nativeEvent(eventType, message, result);

    if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
        // Claim the whole window as client area. When maximized the window
        // rect extends beyond the work area by the border thickness; inset
        // the client so content stays fully visible.
        auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
        if (IsZoomed(msg->hwnd)) {
            const int frame = GetSystemMetrics(SM_CXFRAME)
                              + GetSystemMetrics(SM_CXPADDEDBORDER);
            params->rgrc[0].left += frame;
            params->rgrc[0].top += frame;
            params->rgrc[0].right -= frame;
            params->rgrc[0].bottom -= frame;
        }
        *result = 0;
        return true;
    }

    if (msg->message == WM_NCHITTEST) {
        // 标题栏区域报 HTCAPTION：交给 Windows 处理拖动，从而拿到原生手势
        // （拖到屏幕顶部最大化、左右 Snap、甩动、最大化时拖出自动还原）。
        // 按钮与菜单项仍走 HTCLIENT，保证可点击。
        const QPoint globalPos(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
        if (isCaptionHit(globalPos)) {
            *result = HTCAPTION;
            return true;
        }
    }

    if (msg->message == WM_NCHITTEST && !isMaximized() && !isFullScreen()) {
        const LONG x = GET_X_LPARAM(msg->lParam);
        const LONG y = GET_Y_LPARAM(msg->lParam);

        RECT rc;
        if (GetWindowRect(msg->hwnd, &rc)) {
            const int edge = qRound(6 * devicePixelRatioF());
            const bool left = x < rc.left + edge;
            const bool right = x >= rc.right - edge;
            const bool top = y < rc.top + edge;
            const bool bottom = y >= rc.bottom - edge;

            LRESULT hit = HTCLIENT;
            if (left && top)            hit = HTTOPLEFT;
            else if (right && top)      hit = HTTOPRIGHT;
            else if (left && bottom)    hit = HTBOTTOMLEFT;
            else if (right && bottom)   hit = HTBOTTOMRIGHT;
            else if (left)              hit = HTLEFT;
            else if (right)             hit = HTRIGHT;
            else if (top)               hit = HTTOP;
            else if (bottom)            hit = HTBOTTOM;

            if (hit != HTCLIENT) {
                *result = hit;
                return true;
            }
        }
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::syncNewTabButton()
{
    QTabBar* bar = m_tabs->tabBar();
    const int barHeight = bar->height();
    if (barHeight <= 0)
        return;
    const int buttonHeight = qMin(32, barHeight);
    m_newTabButton->setFixedSize(32, buttonHeight);
    m_newTabButton->move(bar->width() - 32 - 4,
                         (barHeight - buttonHeight) / 2);
    m_newTabButton->raise();
}

void MainWindow::showEvent(QShowEvent* event)
{
    syncNewTabButton();
    QMainWindow::showEvent(event);
}

// 面板卡片拖拽（VS Code sections 手感）：
//   起拖 -> 幻影标题条跟随鼠标 + 反馈层激活；
//   拖动中 -> 悬停在卡片列上刷新该列的插入指示线；悬停在活动栏上高亮
//   "收起为按钮"；其它区域轻微暗化（无效）；
//   松手 -> 按落点结算：列内插入 / 换列 / 收起；无效落点取消。
void MainWindow::setupPanelDrag()
{
    m_dropFeedback = new editor::DropFeedbackLayer(this);
    m_dragGhost = new editor::PanelDragGhost(this);

    auto makeMover = [this](QWidget* header, const QString& id) {
        auto* mover = new editor::PanelDragMover(header, id, this);
        connect(mover, &editor::PanelDragMover::dragStarted, this,
                [this](const QString& pageId) {
                    QWidget* panel = panelWidget(pageId);
                    m_dragGhost->showFor(panelWindowTitle(pageId),
                                         QCursor::pos(),
                                         panel ? qMax(220, panel->width()) : 220);
                    m_dropFeedback->begin(pageId);
                });
        connect(mover, &editor::PanelDragMover::dragMoved, this,
                [this](const QString& pageId, const QPoint& globalPos) {
                    m_dragGhost->moveTo(globalPos);

                    // 悬停目标：活动栏 or 侧栏列
                    QWidget* bar = m_activityBar;
                    const bool overBar = bar->rect().contains(
                        bar->mapFromGlobal(globalPos));
                    m_dropFeedback->updateHover(globalPos, overBar);

                    m_leftColumn->clearDropIndicator();
                    m_rightColumn->clearDropIndicator();
                    if (!overBar) {
                        // 窗口横向三等分：左/中/右。右列即使为空也可作为
                        // 落点（VS Code 允许拖到还没有面板的边）。
                        const int w = width();
                        const QPoint local = mapFromGlobal(globalPos);
                        if (local.x() < w / 3) {
                            m_leftColumn->dropTarget(panelWidget(pageId),
                                                     globalPos);
                        } else if (local.x() > w * 2 / 3) {
                            m_rightColumn->dropTarget(panelWidget(pageId),
                                                      globalPos);
                        }
                        // 中区 = 无效（编辑器），不做列指示
                    }
                });
        connect(mover, &editor::PanelDragMover::dragFinished, this,
                [this](const QString& pageId, const QPoint& globalPos) {
                    m_dragGhost->hideGhost();
                    m_dropFeedback->end();
                    m_leftColumn->clearDropIndicator();
                    m_rightColumn->clearDropIndicator();
                    finishPanelDrag(pageId, globalPos);
                });
        return mover;
    };
    m_sidebarDrag = makeMover(m_explorer->headerWidget(),
                              QStringLiteral("explorer"));
    m_outlineDrag = makeMover(m_outline->headerWidget(),
                              QStringLiteral("outline"));
    m_previewDrag = makeMover(m_preview->headerWidget(),
                              QStringLiteral("preview"));
    // 搜索结果面板同样可拖动（它有自己的标题栏把手）
    m_searchDrag = makeMover(m_search->headerWidget(),
                             QStringLiteral("search"));
}

QString MainWindow::panelWindowTitle(const QString& pageId) const
{
    if (pageId == QStringLiteral("explorer"))
        return tr("资源管理器");
    if (pageId == QStringLiteral("search"))
        return tr("全局搜索");
    if (pageId == QStringLiteral("outline"))
        return tr("大纲");
    return pageId;
}

// 拖拽落下结算：活动栏 = 收起；卡片列 = 插入/换列；无效 = 取消。
void MainWindow::finishPanelDrag(const QString& panelId, const QPoint& globalPos)
{
    // 1) 悬停在活动栏上 -> 收起为按钮（活动栏宽 48，优先于左列判定）
    QWidget* bar = m_activityBar;
    const QPoint barLocal = bar->mapFromGlobal(globalPos);
    if (barLocal.x() >= 0 && barLocal.x() < bar->width() && barLocal.y() >= 0
        && barLocal.y() < bar->height()) {
        collapsePanelToActivityBar(panelId);
        return;
    }

    // 2) 窗口横向三分判定（空列也可落，见 dragMoved 注释）
    const auto settle = [this](PanelColumn* col, const QString& id,
                               const QPoint& pos) {
        const QString before = col->dropTarget(panelWidget(id), pos);
        movePanelToColumn(id, col == m_rightColumn ? QStringLiteral("right")
                                                   : QStringLiteral("left"),
                          before);
        col->clearDropIndicator();
    };
    const int w = width();
    const QPoint local = mapFromGlobal(globalPos);
    if (local.x() < w / 3) {
        if (panelId == QStringLiteral("preview"))
            setPreviewPosition(QStringLiteral("right")); // 预览无左停靠
        else
            settle(m_leftColumn, panelId, globalPos);
        return;
    }
    if (local.x() > w * 2 / 3) {
        if (panelId == QStringLiteral("preview"))
            setPreviewPosition(QStringLiteral("right")); // 预览不进侧栏列
        else
            settle(m_rightColumn, panelId, globalPos);
        return;
    }

    // 2.5) 中区下部 1/3 = 底部落区（仅预览响应；其它面板取消）
    if (local.y() > height() * 2 / 3) {
        if (panelId == QStringLiteral("preview"))
            setPreviewPosition(QStringLiteral("bottom"));
        return;
    }

    // 3) 中区无效 -> 取消（面板原地不动）
}

void MainWindow::buildMenus()
{
    QMenuBar* mb = m_titleBar->menuBar();

    // ---- 文件 ----
    QMenu* fileMenu = mb->addMenu(tr("文件"));
    QAction* newAction = fileMenu->addAction(tr("新建文件"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newDocument);

    QAction* openAction = fileMenu->addAction(tr("打开文件..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFileDialog);

    QAction* openFolderAction = fileMenu->addAction(tr("打开文件夹..."));
    openFolderAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    connect(openFolderAction, &QAction::triggered, this, &MainWindow::openFolderDialog);

    QAction* quickOpenAction = fileMenu->addAction(tr("快速打开..."));
    quickOpenAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+P")));
    connect(quickOpenAction, &QAction::triggered, this, &MainWindow::showQuickOpen);

    QAction* searchInFilesAction = fileMenu->addAction(tr("在文件中查找..."));
    searchInFilesAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    connect(searchInFilesAction, &QAction::triggered, this, &MainWindow::showSearchPanel);

    m_recentMenu = fileMenu->addMenu(tr("最近打开"));
    connect(m_recentMenu, &QMenu::aboutToShow,
            this, &MainWindow::rebuildRecentFilesMenu);

    fileMenu->addSeparator();

    QAction* saveAction = fileMenu->addAction(tr("保存"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveCurrentDocument);

    QAction* saveAsAction = fileMenu->addAction(tr("另存为..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveCurrentDocumentAs);

    fileMenu->addSeparator();

    QAction* closeAction = fileMenu->addAction(tr("关闭标签"));
    closeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    connect(closeAction, &QAction::triggered, this, &MainWindow::closeCurrentTab);

    fileMenu->addSeparator();

    QAction* settingsAction = fileMenu->addAction(tr("设置..."));
    settingsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);

    fileMenu->addSeparator();

    QAction* exitAction = fileMenu->addAction(tr("退出"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    // ---- 编辑 ----
    QMenu* editMenu = mb->addMenu(tr("编辑"));

    QAction* undoAction = editMenu->addAction(tr("撤销"));
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this,
            [this] { if (auto* e = currentEditor()) e->undo(); });

    QAction* redoAction = editMenu->addAction(tr("重做"));
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this,
            [this] { if (auto* e = currentEditor()) e->redo(); });

    editMenu->addSeparator();

    QAction* cutAction = editMenu->addAction(tr("剪切"));
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this,
            [this] { if (auto* e = currentEditor()) e->cut(); });

    QAction* copyAction = editMenu->addAction(tr("复制"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this,
            [this] { if (auto* e = currentEditor()) e->copy(); });

    QAction* pasteAction = editMenu->addAction(tr("粘贴"));
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this,
            [this] { if (auto* e = currentEditor()) e->paste(); });

    editMenu->addSeparator();

    QAction* selectAllAction = editMenu->addAction(tr("全选"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this,
            [this] { if (auto* e = currentEditor()) e->selectAll(); });

    editMenu->addSeparator();

    QAction* findAction = editMenu->addAction(tr("查找..."));
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this,
            [this] { if (auto* e = currentEditor()) e->openFind(false); });

    QAction* replaceAction = editMenu->addAction(tr("替换..."));
    replaceAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+H")));
    connect(replaceAction, &QAction::triggered, this,
            [this] { if (auto* e = currentEditor()) e->openFind(true); });

    editMenu->addSeparator();

    QAction* commentAction = editMenu->addAction(tr("切换行注释"));
    commentAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+/")));
    connect(commentAction, &QAction::triggered, this, [this] {
        if (auto* e = currentEditor())
            editor::commands::toggleLineComment(e, documentFor(e)->language());
    });

    QAction* blockCommentAction = editMenu->addAction(tr("切换块注释"));
    blockCommentAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+/")));
    connect(blockCommentAction, &QAction::triggered, this, [this] {
        if (auto* e = currentEditor())
            editor::commands::toggleBlockComment(e, documentFor(e)->language());
    });

    editMenu->addSeparator();

    QMenu* markdownMenu = editMenu->addMenu(tr("Markdown"));
    QAction* boldAction = markdownMenu->addAction(tr("加粗"));
    connect(boldAction, &QAction::triggered, this,
            [this] { if (auto* e = currentEditor()) editor::commands::wrapSelection(e, "**"); });
    QAction* italicAction = markdownMenu->addAction(tr("斜体"));
    connect(italicAction, &QAction::triggered, this,
            [this] { if (auto* e = currentEditor()) editor::commands::wrapSelection(e, "*"); });
    QAction* codeAction = markdownMenu->addAction(tr("行内代码"));
    connect(codeAction, &QAction::triggered, this,
            [this] { if (auto* e = currentEditor()) editor::commands::wrapSelection(e, "`"); });

    // ---- 视图 ----
    m_pluginMenu = mb->addMenu(tr("插件"));

    QMenu* viewMenu = mb->addMenu(tr("视图"));

    QAction* sidebarAction = viewMenu->addAction(tr("侧边栏"));
    sidebarAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+B")));
    sidebarAction->setCheckable(true);
    sidebarAction->setChecked(settings::AppSettings::instance().sidebarVisible);
    connect(sidebarAction, &QAction::toggled, this,
            [this, sidebarAction](bool checked) {
                m_activityBar->explorerButton->setChecked(checked);
                if (m_explorer->isVisible() == checked)
                    return;
                m_explorer->setVisible(checked);
                auto& st = settings::AppSettings::instance();
                st.sidebarVisible = checked;
                st.save();
            });
    connect(m_activityBar->explorerButton, &QToolButton::toggled,
            sidebarAction, &QAction::setChecked);

    QAction* themeAction = viewMenu->addAction(tr("切换深/浅主题"));
    themeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+T")));
    connect(themeAction, &QAction::triggered, this, &MainWindow::toggleTheme);

    QAction* paletteAction = viewMenu->addAction(tr("命令面板…"));
    paletteAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    connect(paletteAction, &QAction::triggered, this, &MainWindow::showCommandPalette);

    viewMenu->addSeparator();

    QAction* zoomInAction = viewMenu->addAction(tr("放大"));
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);

    QAction* zoomOutAction = viewMenu->addAction(tr("缩小"));
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);

    QAction* resetZoomAction = viewMenu->addAction(tr("重置缩放"));
    resetZoomAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(resetZoomAction, &QAction::triggered, this, &MainWindow::resetZoom);


QMenu* helpMenu = mb->addMenu(tr("帮助"));
    QAction* assocAction = helpMenu->addAction(tr("设置文件关联…"));
    connect(assocAction, &QAction::triggered, this, &MainWindow::setupFileAssociations);
    helpMenu->addSeparator();

    QAction* aboutAction = helpMenu->addAction(tr("关于 Hutaomu Editor"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

editor::CodeEditor* MainWindow::createEditor()
{
    auto* editor = new editor::CodeEditor(this);
    auto* doc = new core::Document(editor);
    applyEditorFont(editor);

    if (settings::AppSettings::instance().wordWrap)
        editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);

    connect(editor->document(), &QTextDocument::modificationChanged, this,
            [this, editor](bool) { updateTabTitle(editor); });
    connect(editor, &QPlainTextEdit::cursorPositionChanged,
            this, &MainWindow::updateStatusBar);
    connect(doc, &core::Document::diskChanged, this,
            [this, editor] { handleDiskChange(editor); });
    connect(editor->document(), &QTextDocument::contentsChange,
            this, [this](int, int, int) { m_refreshDebounce->start(); });

    // 编辑 -> 预览 滚动同步（按比例）
    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this, editor](int value) {
                if (m_syncingScroll || editor != currentEditor() || !m_preview->isVisible())
                    return;
                auto* doc = documentFor(editor);
                if (!doc || !doc->language()
                    || doc->language()->id != QStringLiteral("markdown"))
                    return;
                QScrollBar* previewBar = m_preview->verticalScrollBar();
                const int previewMax = previewBar->maximum();
                const int editorMax = editor->verticalScrollBar()->maximum();
                if (previewMax <= 0 || editorMax <= 0)
                    return;
                m_syncingScroll = true;
                previewBar->setValue(qRound(value / qreal(editorMax) * previewMax));
                m_syncingScroll = false;
            });

    // 触控板/触屏捏合缩放
    connect(editor, &editor::CodeEditor::zoomGestureRequested,
            this, &MainWindow::zoomBy);

    connect(doc, &core::Document::fileDeleted, this,
            [this, editor] {
                statusBar()->showMessage(
                    tr("“%1”已从磁盘移除。").arg(editorDisplayName(editor)), 6000);
            });

    editor->viewport()->setMouseTracking(true);
    return editor;
}

void MainWindow::newDocument()
{
    editor::CodeEditor* editor = createEditor();
    ++m_untitledCounter;
    editor->setProperty("untitledName",
                        tr("未命名 %1").arg(m_untitledCounter));

    const int index = m_tabs->addTab(editor, editorDisplayName(editor));
    m_tabs->setCurrentIndex(index);
    editor->setFocus();
    updateStatusBar();
}

void MainWindow::openPath(const QString& filePath)
{
    QFileInfo info(filePath);
    if (!info.exists()) {
        QMessageBox::warning(this, tr("Open File"),
                             tr("文件未找到：\n%1").arg(filePath));
        return;
    }

    const QString absolute = info.absoluteFilePath();

    // Already open? Just activate that tab.
    for (int i = 0; i < m_tabs->count(); ++i) {
        QWidget* tab = m_tabs->widget(i);
        if (auto* viewer = qobject_cast<viewers::DocumentViewer*>(tab)) {
            if (viewer->filePath() == absolute) {
                m_tabs->setCurrentIndex(i);
                return;
            }
            continue;
        }
        auto* editor = editorAt(i);
        auto* doc = documentFor(editor);
        if (doc && doc->filePath() == absolute) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }

    // 非文本格式（图片/视频/音频/PDF/Office）-> 专用查看器标签页
    if (viewers::viewerKindForFile(absolute) != viewers::ViewerKind::None) {
        viewers::DocumentViewer* viewer = viewers::createViewer(absolute, this);
        if (viewer) {
            const int index = m_tabs->addTab(viewer, info.fileName());
            m_tabs->setTabToolTip(index, absolute);
            m_tabs->setCurrentIndex(index);
            viewer->setFocus();
            rememberRecentFile(absolute);
            updateStatusBar();
            return;
        }
    }

    // Opening from the command line replaces a pristine untitled tab.
    // (Removal is direct: closeTab() would spawn a fresh untitled tab.)
    if (m_tabs->count() == 1 && isBlankUntitled(editorAt(0))) {
        auto* blank = editorAt(0);
        m_tabs->removeTab(0);
        blank->deleteLater();
    }

    auto* editor = createEditor();
    auto* doc = documentFor(editor);

    QString text;
    QString error;
    if (!doc->loadFrom(absolute, &text, &error)) {
        QMessageBox::warning(this, tr("Open File"),
                             tr("无法打开 %1：\n%2").arg(absolute, error));
        delete editor;
        return;
    }

    editor->setPlainText(text);
    editor->document()->setModified(false);
    doc->setLanguage(syntax::LanguageRegistry::instance().detect(absolute, text));
    editor->setLanguage(doc->language());

    const int index = m_tabs->addTab(editor, info.fileName());
    m_tabs->setCurrentIndex(index);
    editor->setFocus();

    rememberRecentFile(absolute);
    updateTabTitle(editor);
    updateStatusBar();
}

void MainWindow::openFileDialog()
{
    const auto& s = settings::AppSettings::instance();
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Open File"), s.lastDir,
        tr("所有文件 (*);;Markdown (*.md *.markdown);;文本文件 (*.txt)"));

    for (const QString& file : files)
        openPath(file);
}

void MainWindow::openFolderDialog()
{
    const auto& s = settings::AppSettings::instance();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Open Folder"),
        s.workspacePath.isEmpty() ? s.lastDir : s.workspacePath);
    if (!dir.isEmpty())
        setWorkspace(dir);
}

void MainWindow::setWorkspace(const QString& path)
{
    m_explorer->setWorkspace(path);
    m_search->setWorkspace(path);
    auto& s = settings::AppSettings::instance();
    s.workspacePath = path;
    s.lastDir = path;
    s.save();
    statusBar()->showMessage(tr("工作区：%1").arg(path), 4000);
}

SaveResult MainWindow::promptSave(editor::CodeEditor* editor)
{
    if (!editor->document()->isModified())
        return SaveResult::Discarded;

    // 显式给中文按钮文案：标准按钮的文字来自 Qt 翻译文件，
    // 一旦 .qm 没装/没加载就会变成英文的 Save/Discard/Cancel。
    QPushButton* clicked = app::dialog::show(
        this, tr("保存更改"),
        tr("是否保存对“%1”的更改？").arg(editorDisplayName(editor)),
        tr("不保存的话，这些改动会丢失。"),
        app::dialog::Icon::Warning,
        { { tr("保存"), app::dialog::Button::Save },
          { tr("不保存"), app::dialog::Button::Discard },
          { tr("取消"), app::dialog::Button::Cancel } });
    if (clicked && clicked->text() == tr("保存"))
        return saveEditor(editor, false) ? SaveResult::Saved : SaveResult::Cancelled;
    if (clicked && clicked->text() == tr("不保存"))
        return SaveResult::Discarded;
    return SaveResult::Cancelled;
}

bool MainWindow::saveEditor(editor::CodeEditor* editor, bool saveAs)
{
    auto* doc = documentFor(editor);
    if (!doc)
        return false;

    QString target = doc->filePath();
    if (saveAs || doc->isUntitled()) {
        const auto& s = settings::AppSettings::instance();
        if (target.isEmpty()) {
            const QString name = editor->property("untitledName").toString();
            const QString dir = s.lastDir.isEmpty() ? QDir::homePath() : s.lastDir;
            target = dir + QLatin1Char('/') + name;
        }
        target = QFileDialog::getSaveFileName(
            this, tr("保存文件"), target, tr("All Files (*)"));
        if (target.isEmpty())
            return false;
    }

    QString error;
    if (!doc->save(editor->toPlainText(), target, &error)) {
        QMessageBox::warning(this, tr("保存文件"),
                             tr("无法保存 %1：\n%2").arg(target, error));
        return false;
    }

    editor->document()->setModified(false);
    rememberRecentFile(doc->filePath());

    updateTabTitle(editor);
    updateWindowTitle();
    updateStatusBar();

    if (!error.isEmpty())
        statusBar()->showMessage(error, 6000);
    return true;
}

void MainWindow::saveCurrentDocument()
{
    if (auto* editor = currentEditor())
        saveEditor(editor, false);
}

void MainWindow::saveCurrentDocumentAs()
{
    if (auto* editor = currentEditor())
        saveEditor(editor, true);
}

void MainWindow::closeCurrentTab()
{
    closeTab(m_tabs->currentIndex());
}

void MainWindow::closeTab(int index)
{
    // 非文本查看器：脏状态时询问保存（图片旋转/Office 编辑可写回）
    if (auto* viewer = qobject_cast<viewers::DocumentViewer*>(
            m_tabs->widget(index))) {
        if (viewer->isModified()) {
            const QMessageBox::StandardButton choice = QMessageBox::question(
                this, tr("保存更改"),
                tr("是否保存对“%1”的更改？")
                    .arg(m_tabs->tabText(index)),
                QMessageBox::Save | QMessageBox::Discard
                    | QMessageBox::Cancel,
                QMessageBox::Save);
            if (choice == QMessageBox::Cancel)
                return;
            if (choice == QMessageBox::Save && !viewer->save())
                return;
        }
        m_tabs->removeTab(index);
        viewer->deleteLater();
        updateWindowTitle();
        updateStatusBar();
        if (m_tabs->count() == 0)
            newDocument();
        return;
    }

    auto* editor = editorAt(index);
    if (!editor)
        return;

    if (promptSave(editor) == SaveResult::Cancelled)
        return;

    m_tabs->removeTab(index);
    editor->deleteLater();
    updateWindowTitle();
    updateStatusBar();

    if (m_tabs->count() == 0)
        newDocument();
}

void MainWindow::handleDiskChange(editor::CodeEditor* editor)
{
    auto* doc = documentFor(editor);
    if (!doc)
        return;

    if (!editor->document()->isModified()) {
        // Clean buffer: reload transparently, keep the user working.
        QString text;
        QString error;
        if (doc->loadFrom(doc->filePath(), &text, &error)) {
            editor->setPlainText(text);
            editor->document()->setModified(false);
            statusBar()->showMessage(tr("“%1”已从磁盘重新加载。")
                                         .arg(editorDisplayName(editor)), 6000);
            updateStatusBar();
        }
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("文件已在磁盘上更改"));
    box.setIcon(QMessageBox::Warning);
    box.setText(tr("“%1”已在磁盘上被修改，但当前有未保存的更改。")
                    .arg(editorDisplayName(editor)));
    QPushButton* reloadButton = box.addButton(tr("从磁盘重新加载"),
                                              QMessageBox::DestructiveRole);
    QPushButton* keepButton = box.addButton(tr("保留我的版本"),
                                            QMessageBox::RejectRole);
    box.setDefaultButton(keepButton);
    box.exec();

    if (box.clickedButton() == reloadButton) {
        QString text;
        QString error;
        if (doc->loadFrom(doc->filePath(), &text, &error)) {
            editor->setPlainText(text);
            editor->document()->setModified(false);
            updateStatusBar();
        } else {
            QMessageBox::warning(this, tr("重新加载"), error);
        }
    }
}

bool MainWindow::isBlankUntitled(editor::CodeEditor* editor) const
{
    auto* doc = documentFor(editor);
    return doc && doc->isUntitled()
           && !editor->document()->isModified()
           && editor->toPlainText().isEmpty();
}

editor::CodeEditor* MainWindow::currentEditor() const
{
    return editorAt(m_tabs->currentIndex());
}

editor::CodeEditor* MainWindow::editorAt(int index) const
{
    return qobject_cast<editor::CodeEditor*>(m_tabs->widget(index));
}

core::Document* MainWindow::documentFor(editor::CodeEditor* editor)
{
    return editor ? editor->findChild<core::Document*>() : nullptr;
}

QString MainWindow::editorDisplayName(editor::CodeEditor* editor) const
{
    auto* doc = documentFor(editor);
    if (!doc)
        return tr("未命名");
    if (doc->isUntitled())
        return editor->property("untitledName").toString();
    return QFileInfo(doc->filePath()).fileName();
}

void MainWindow::updateTabTitle(editor::CodeEditor* editor)
{
    const int index = m_tabs->indexOf(editor);
    if (index < 0)
        return;

    QString title = editorDisplayName(editor);
    if (editor->document()->isModified())
        title += QStringLiteral(" \u2022"); // bullet, VS Code dirty marker
    m_tabs->setTabText(index, title);

    if (index == m_tabs->currentIndex())
        updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    auto* editor = currentEditor();
    if (!editor) {
        setWindowTitle(QStringLiteral("Hutaomu Editor"));
        return;
    }
    QString title = editorDisplayName(editor);
    if (editor->document()->isModified())
        title += QStringLiteral(" \u2022");
    setWindowTitle(title + QStringLiteral(" - Hutaomu Editor"));
}

void MainWindow::updateStatusBar()
{
    auto* editor = currentEditor();
    if (!editor) {
        m_positionLabel->clear();
        m_eolLabel->clear();
        m_encodingLabel->clear();
        m_languageLabel->clear();
        return;
    }

    auto* doc = documentFor(editor);
    const QTextCursor cursor = editor->textCursor();
    m_positionLabel->setText(tr("行 %1，列 %2")
                                 .arg(cursor.blockNumber() + 1)
                                 .arg(cursor.positionInBlock() + 1));
    m_eolLabel->setText(core::eolDisplayName(doc ? doc->eol() : core::Eol::Lf));
    m_encodingLabel->setText(doc ? core::encodingDisplayName(doc->encoding())
                                 : core::encodingDisplayName(core::Encoding::Utf8));
    m_languageLabel->setText(doc && doc->language()
                                 ? doc->language()->displayName
                                 : tr("纯文本"));
    if (doc && doc->language() && doc->language()->id == QStringLiteral("markdown"))
        m_outline->setCurrentLine(cursor.blockNumber() + 1);
    updatePluginStatusItems(); // 插件状态栏段
}

void MainWindow::rebuildRecentFilesMenu()
{
    m_recentMenu->clear();
    const auto& s = settings::AppSettings::instance();

    if (s.recentFiles.isEmpty()) {
        QAction* empty = m_recentMenu->addAction(tr("暂无最近打开的文件"));
        empty->setEnabled(false);
        return;
    }

    for (const QString& path : s.recentFiles) {
        QAction* action = m_recentMenu->addAction(path);
        connect(action, &QAction::triggered, this,
                [this, path] { openPath(path); });
    }

    m_recentMenu->addSeparator();
    QAction* clearAction = m_recentMenu->addAction(tr("清空最近打开"));
    connect(clearAction, &QAction::triggered, this, [] {
        auto& s = settings::AppSettings::instance();
        s.recentFiles.clear();
        s.save();
    });
}

void MainWindow::rememberRecentFile(const QString& filePath)
{
    auto& s = settings::AppSettings::instance();
    s.addRecentFile(filePath);
    s.lastDir = QFileInfo(filePath).absolutePath();
    s.save();
}

void MainWindow::showSearchPanel()
{
    m_activityBar->explorerButton->blockSignals(true);
    m_activityBar->explorerButton->setChecked(false);
    m_activityBar->explorerButton->blockSignals(false);
    m_activityBar->searchButton->setChecked(true);
    if (isPanelCollapsed(QStringLiteral("search")))
        expandPanelFromActivityBar(QStringLiteral("search"));
    m_search->focusInput();
}

void MainWindow::showQuickOpen()
{
    const auto& s = settings::AppSettings::instance();
    if (s.workspacePath.isEmpty()) {
        statusBar()->showMessage(tr("请先打开一个文件夹（Ctrl+Shift+O）"), 4000);
        return;
    }
    const QStringList files = panels::collectWorkspaceFiles(s.workspacePath, 5000);
    QuickOpenDialog dialog(files, this);
    connect(&dialog, &QuickOpenDialog::fileChosen,
            this, [this](const QString& path) { openPath(path); });
    dialog.exec();
}

void MainWindow::showSettings()
{
    // 主题实时预览：选中即应用到界面（不落盘）；取消则回到进入设置前的主题。
    const QString themeBeforeDialog = editor::ThemeManager::theme();
    SettingsDialog dialog(this);
    connect(&dialog, &app::SettingsDialog::themePreviewRequested, this,
            [this](const QString& themeId) { previewTheme(themeId); });
    if (dialog.exec() != QDialog::Accepted) {
        if (editor::ThemeManager::theme() != themeBeforeDialog)
            previewTheme(themeBeforeDialog);
        return;
    }

    auto& s = settings::AppSettings::instance();
    const bool themeChanged = (s.theme != dialog.theme());
    s.theme = dialog.theme();
    s.fontFamily = dialog.fontFamily();
    s.fontSize = dialog.fontSize();
    s.tabWidth = dialog.tabWidth();
    s.wordWrap = dialog.wordWrap();
    s.markdownViewMode = dialog.markdownViewMode();
    const QString sidebarPos = dialog.sidebarPosition();
    const QString previewPos = dialog.previewPosition();
    s.save();

    if (themeChanged) {
        applyThemeAndRefresh(s.theme); // 应用对话框里选中的主题（可能是任意已安装主题）
        maybeConfirmThemeLayout();
    }
    applyFontToAllEditors(); // 含制表符宽度（见 applyEditorFont）
    toggleWordWrap(s.wordWrap);
    applyMarkdownViewMode();
    if (s.sidebarPosition != sidebarPos)
        setSidebarPosition(sidebarPos);
    if (s.previewPosition != previewPos)
        setPreviewPosition(previewPos);
    refreshPluginContributions(); // 插件可能被导入/启停/卸载
}

void MainWindow::setPreviewPosition(const QString& position)
{
    auto& s = settings::AppSettings::instance();
    s.previewPosition = position;
    s.save();
    m_editorSplitter->setOrientation(position == QStringLiteral("bottom")
                                         ? Qt::Vertical
                                         : Qt::Horizontal);
    m_editorSplitter->setSizes({ 700, 300 });
    m_editorSplitter->refresh();
}

// 兼容旧接口：大纲现在是卡片体系的一员，位置由 outlineSide 决定。
void MainWindow::setOutlinePosition(const QString& position)
{
    if (isPanelCollapsed(QStringLiteral("outline")))
        expandPanelFromActivityBar(QStringLiteral("outline"));
    movePanelToColumn(QStringLiteral("outline"), position);
}

// ---- 面板卡片注册表（VS Code sections 模型）----

// 注册面板卡片：默认进入左列并显示。
void MainWindow::registerSidePanel(const QString& pageId, QWidget* panel)
{
    m_sidePanels.insert(pageId, panel);
    m_panelCollapsed.insert(pageId, false);
    m_leftColumn->addPanel(panel);
}

PanelColumn* MainWindow::columnFor(const QString& pageId) const
{
    QWidget* panel = m_sidePanels.value(pageId);
    if (!panel)
        return nullptr;
    if (m_leftColumn->containsPanel(panel))
        return m_leftColumn;
    if (m_rightColumn->containsPanel(panel))
        return m_rightColumn;
    return nullptr; // 已收起为按钮
}

QString MainWindow::pageIdForPanel(QWidget* panel) const
{
    for (auto it = m_sidePanels.constBegin(); it != m_sidePanels.constEnd(); ++it) {
        if (it.value() == panel)
            return it.key();
    }
    return QString();
}

QWidget* MainWindow::panelWidget(const QString& pageId) const
{
    return m_sidePanels.value(pageId);
}

bool MainWindow::isPanelCollapsed(const QString& pageId) const
{
    return m_panelCollapsed.value(pageId, false);
}

// 页面换列（beforeId 空 = 追加到末尾）。
void MainWindow::movePanelToColumn(const QString& pageId, const QString& side,
                                   const QString& beforeId)
{
    QWidget* panel = m_sidePanels.value(pageId);
    if (!panel)
        return;
    PanelColumn* target = side == QStringLiteral("right") ? m_rightColumn
                                                          : m_leftColumn;
    PanelColumn* source = columnFor(pageId);
    if (source == target && beforeId.isEmpty())
        return;

    int index = target->panelCount();
    if (!beforeId.isEmpty()) {
        const QStringList order = target->panelOrder();
        index = order.indexOf(beforeId);
        if (index < 0)
            index = target->panelCount();
    }
    if (source)
        source->takePanel(panel);
    target->insertPanel(panel, index);

    // 持久化停靠侧
    auto& s = settings::AppSettings::instance();
    if (pageId == QStringLiteral("outline"))
        s.outlineSide = side;
    else if (pageId == QStringLiteral("search"))
        s.searchSide = side;
    else if (pageId == QStringLiteral("explorer"))
        s.sidebarPosition = side;
    s.save();

    m_panelCollapsed.insert(pageId, false);
    applyLayoutPositions();
}

// 收起为活动栏按钮：卡片从列中摘除。
void MainWindow::collapsePanelToActivityBar(const QString& pageId)
{
    QWidget* panel = m_sidePanels.value(pageId);
    if (!panel)
        return;
    if (PanelColumn* col = columnFor(pageId))
        col->takePanel(panel);
    panel->setParent(nullptr);
    panel->hide();
    m_panelCollapsed.insert(pageId, true);
    m_activityBar->setPanelChecked(pageId, false);
    applyLayoutPositions();
}

// 从活动栏展开为卡片：进入左列顶部。
void MainWindow::expandPanelFromActivityBar(const QString& pageId)
{
    QWidget* panel = m_sidePanels.value(pageId);
    if (!panel)
        return;
    m_leftColumn->addPanel(panel);
    panel->show();
    m_panelCollapsed.insert(pageId, false);
    m_activityBar->setPanelChecked(pageId, true);
    applyLayoutPositions();
}

// 活动栏面板按钮：未收起时再点 = 收起为按钮；已收起时点击 = 展开为卡片。
// 用户手动操作会覆盖自动显隐的标记（此后大纲的去留完全由用户控制，
// 直到下一次"文档从无标题变为有标题"才重新交给自动逻辑）。
void MainWindow::handlePanelToggled(const QString& pageId, bool checked)
{
    if (pageId == QStringLiteral("explorer"))
        return; // 资源管理器走 explorerToggled 专用路径
    m_outlineAutoCollapsed = false;
    if (checked) {
        if (isPanelCollapsed(pageId))
            expandPanelFromActivityBar(pageId);
        else
            collapsePanelToActivityBar(pageId); // 再点同页 = 收起
    } else if (!isPanelCollapsed(pageId)) {
        collapsePanelToActivityBar(pageId);
    }
}

void MainWindow::applyLayoutPositions()
{
    auto& s = settings::AppSettings::instance();
    const bool previewBottom = s.previewPosition == QStringLiteral("bottom");

    // 1. 编辑器区：tabs + 预览，按预览位置定顺序与方向
    m_tabs->setParent(nullptr);
    m_preview->setParent(nullptr);
    m_editorSplitter->addWidget(m_tabs);
    m_editorSplitter->addWidget(m_preview);
    m_editorSplitter->setOrientation(previewBottom ? Qt::Vertical
                                                   : Qt::Horizontal);

    // 2. 摘下顶层列
    m_leftColumn->setParent(nullptr);
    m_rightColumn->setParent(nullptr);
    m_editorSplitter->setParent(nullptr);

    // 3. 装配：主分栏固定为 [左列] [编辑器] [右列]。每个面板按自己的
    //    停靠侧（explorer=sidebarPosition / search=searchSide /
    //    outline=outlineSide）归入左列或右列；两列都挂回窗口树，空列
    //    隐藏。不再让某个面板的停靠侧决定整条主分栏的方向——否则拖动
    //    单个面板会把整个侧栏区搬到另一侧，"拖不回去"。
    const bool leftVisible = m_leftColumn->panelCount() > 0;
    const bool rightVisible = m_rightColumn->panelCount() > 0;
    m_splitter->addWidget(m_leftColumn);
    m_splitter->addWidget(m_editorSplitter);
    m_splitter->addWidget(m_rightColumn);
    m_leftColumn->setVisible(leftVisible);
    m_rightColumn->setVisible(rightVisible);

    // 4. 尺寸：空列 1px（不可折叠设置下的最小占位，视觉上看不见）。
    //    三段 stretch factor 必须显式设置：只有编辑器区伸缩，两个侧栏列
    //    固定宽——否则窗口变化/重新布局时列会按 sizeHint 被撑大。
    const int sideW = 280;
    QList<int> mainSizes = { leftVisible ? sideW : 1, 900,
                             rightVisible ? sideW : 1 };
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setSizes(mainSizes);
    m_splitter->setStretchFactor(0, 0); // 左列不伸缩
    m_splitter->setStretchFactor(1, 1); // 编辑器吃剩余空间
    m_splitter->setStretchFactor(2, 0); // 右列不伸缩

    m_preview->setVisible(m_previewVisible);
}

void MainWindow::setMarkdownViewMode(const QString& mode)
{
    auto& s = settings::AppSettings::instance();
    s.markdownViewMode = mode;
    s.save();
    applyMarkdownViewMode();
}

// 侧边栏（文件树/搜索）在左还是在右。
void MainWindow::setSidebarPosition(const QString& position)
{
    auto& s = settings::AppSettings::instance();
    if (s.sidebarPosition != position) {
        s.sidebarPosition = position;
        s.save();
    }
    applyLayoutPositions();
}



void MainWindow::applyMarkdownViewMode()
{
    auto* editor = currentEditor();
    auto* doc = editor ? documentFor(editor) : nullptr;
    const bool isMarkdown = doc && doc->language()
                            && doc->language()->id == QStringLiteral("markdown");
    const QString mode = settings::AppSettings::instance().markdownViewMode;

    // 为所有 Markdown 编辑器套用/移除实时预览装饰
    // （查看器标签页 editorAt 返回空，必须跳过——否则空指针崩溃）
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* e = editorAt(i);
        if (!e)
            continue;
        auto* d = documentFor(e);
        const bool eIsMd = d && d->language()
                           && d->language()->id == QStringLiteral("markdown");
        e->setLiveMarkdown(eIsMd && mode == QStringLiteral("live"));
    }

    m_previewVisible = isMarkdown && mode == QStringLiteral("split");
    m_preview->setVisible(m_previewVisible);
    // 大纲跟随当前激活的标签页：无论视图模式，切换标签即刷新
    // （此前只有 split 模式刷新，live/源码模式下切标签大纲陈旧）
    updatePreviewAndOutline();
    if (isMarkdown && mode == QStringLiteral("split"))
        ui::fadeIn(m_preview, 200);
}

void MainWindow::updatePreviewAndOutline()
{
    auto* editor = currentEditor();
    auto* doc = editor ? documentFor(editor) : nullptr;
    const bool isMarkdown = doc && doc->language()
                            && doc->language()->id == QStringLiteral("markdown");

    if (isMarkdown) {
        if (m_preview->isVisible())
            m_preview->setMarkdown(editor->toPlainText(), doc->filePath());
        m_outline->updateFromText(editor->toPlainText());
        // 重建后立即同步光标所在章节
        m_outline->setCurrentLine(editor->textCursor().blockNumber() + 1);
    } else {
        m_outline->clearOutline();
    }

    // 大纲自动显隐：文档没有标题（或非 Markdown）时收起为活动栏按钮，
    // 出现标题时自动展开。
    //   m_outlineAutoCollapsed = 上次收起是系统自动所为。用户手动收起
    //   （handlePanelToggled 清此标记）后，自动逻辑不再抢着展开/收起，
    //   大纲去留交还用户；直到"无标题 -> 有标题"的文档切换发生。
    const bool hasHeadings = isMarkdown && m_outline->count() > 0;
    if (!hasHeadings) {
        if (!isPanelCollapsed(QStringLiteral("outline"))) {
            collapsePanelToActivityBar(QStringLiteral("outline"));
            m_outlineAutoCollapsed = true;
        }
    } else if (isPanelCollapsed(QStringLiteral("outline"))
               && m_outlineAutoCollapsed) {
        expandPanelFromActivityBar(QStringLiteral("outline"));
        m_outlineAutoCollapsed = false;
    }
}

void MainWindow::cycleTabs(int direction)
{
    const int count = m_tabs->count();
    if (count < 2)
        return;
    const int next = (m_tabs->currentIndex() + direction + count) % count;
    m_tabs->setCurrentIndex(next);
}

QFont MainWindow::editorFont() const
{
    const auto& s = settings::AppSettings::instance();
    // 主题（T2 度量）指定的字体/字号优先于用户设置——Typora 风主题正是靠
    // 字体与字号达成的观感；设置页会提示"当前主题指定了字体/字号"。
    const QString themeFamilies
        = editor::ThemeManager::metric(QStringLiteral("editorFontFamily"));
    const int themeSize = editor::ThemeManager::metricInt(QStringLiteral("editorFontSize"), 0);

    QFont font;
    const QString familySpec = themeFamilies.isEmpty() ? s.fontFamily : themeFamilies;
    if (familySpec.isEmpty()) {
        // Consolas ships with Windows; styleHint falls back to any
        // monospace font on platforms where it is missing.
        font = QFont(QStringLiteral("Consolas"));
        font.setStyleHint(QFont::Monospace);
    } else {
        // 主题可给候选列表（"Georgia, Noto Serif SC, serif"）：Qt 按序回退。
        QStringList families;
        for (const QString& family : familySpec.split(QLatin1Char(','), Qt::SkipEmptyParts))
            families.append(family.trimmed());
        font = QFont(families.value(0));
        if (families.size() > 1)
            font.setFamilies(families);
    }
    font.setPointSize(themeSize > 0 ? themeSize : s.fontSize);
    return font;
}

void MainWindow::applyEditorFont(editor::CodeEditor* editor)
{
    if (!editor)
        return; // 查看器标签页不是编辑器
    editor->setEditorFont(editorFont());
    editor->setTabWidth(settings::AppSettings::instance().tabWidth);
    // 新开的编辑器也要继承当前主题形态（居中限宽 / 标题下划线）
    const editor::ThemeLayout& layout = editor::ThemeManager::currentLayout();
    const bool centered = layout.centered.value_or(false);
    editor->setContentMaxWidth(centered ? layout.maxWidth.value_or(0) : 0);
    editor->setHeadingUnderline(layout.headingUnderline.value_or(false));
}

void MainWindow::applyFontToAllEditors()
{
    for (int i = 0; i < m_tabs->count(); ++i)
        applyEditorFont(editorAt(i));
}

void MainWindow::zoomBy(double factor)
{
    if (zoomViewer(factor))
        return;
    auto& s = settings::AppSettings::instance();
    s.fontSize = qBound(8, int(qRound(s.fontSize * factor)), 40);
    applyFontToAllEditors();
}

// 当前标签若是支持缩放的查看器（docx/pptx/图片），缩放作用于查看器
bool MainWindow::zoomViewer(double factor)
{
    auto* viewer = qobject_cast<viewers::DocumentViewer*>(m_tabs->currentWidget());
    if (!viewer || !viewer->supportsZoom())
        return false;
    if (factor > 1.0)
        viewer->zoomIn();
    else if (factor < 1.0)
        viewer->zoomOut();
    else
        viewer->resetZoom();
    return true;
}

void MainWindow::zoomIn()
{
    if (zoomViewer(1.1))
        return;
    auto& s = settings::AppSettings::instance();
    s.fontSize = qMin(40, s.fontSize + 1);
    applyFontToAllEditors();
}

void MainWindow::zoomOut()
{
    if (zoomViewer(0.9))
        return;
    auto& s = settings::AppSettings::instance();
    s.fontSize = qMax(8, s.fontSize - 1);
    applyFontToAllEditors();
}

void MainWindow::resetZoom()
{
    if (zoomViewer(1.0))
        return;
    auto& s = settings::AppSettings::instance();
    s.fontSize = 14;
    applyFontToAllEditors();
}

void MainWindow::toggleWordWrap(bool wrap)
{
    auto& s = settings::AppSettings::instance();
    s.wordWrap = wrap;
    const QPlainTextEdit::LineWrapMode mode =
        wrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto* e = editorAt(i))
            e->setLineWrapMode(mode);
    }
}

void MainWindow::applyThemeAndRefresh(const QString& themeId)
{
    auto& s = settings::AppSettings::instance();
    previewTheme(themeId);
    // 主题不存在时 applyTheme 会回退默认主题，这里记录**实际生效**的 id，
    // 避免设置里留下一个应用不出来的名字。
    s.theme = editor::ThemeManager::theme();
    s.save();
}

void MainWindow::previewTheme(const QString& themeId)
{
    if (themeId == editor::ThemeManager::theme())
        return; // 已是当前主题：避免设置页打开时的无谓重刷
    app::fx::beginThemeFade(this); // 旧外观截屏，主题切换后淡出
    editor::ThemeManager::applyTheme(themeId);
    editor::IconLoader::clearCache(); // 主题资源（logo/图标覆盖）随之切换
    applyThemeLayout();               // 主题形态（T4）

    m_titleBar->refreshIcons();
    m_activityBar->refreshIcons();
    m_explorer->refreshIcons();
    syncNewTabButton();
    m_newTabButton->setIcon(editor::IconLoader::load(
        QStringLiteral("tab-add"),
        editor::ThemeManager::tokens().editorForeground,
        editor::ThemeManager::tokens().editorBackground, 14));

    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto* e = editorAt(i))
            e->applyTheme();
    }
    for (QWidget* page : m_pluginPages) {
        if (auto* pluginPage = qobject_cast<plugins::PluginPagePanel*>(page))
            pluginPage->refreshTheme();
    }
    applyFontToAllEditors(); // 主题可指定编辑器字体/字号（T2）
    updatePreviewAndOutline();
    app::fx::endThemeFade(); // 旧外观淡出
}

bool MainWindow::themeLayoutActive() const
{
    return m_layoutApplied;
}

void MainWindow::repolish(QWidget* widget)
{
    if (!widget)
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void MainWindow::captureLayoutSnapshot()
{
    LayoutSnapshot& snap = m_layoutSnapshot;
    snap.valid = true;
    snap.activityBar = m_activityBar->isVisible();
    snap.statusBar = statusBar()->isVisible();
    snap.explorer = !isPanelCollapsed(QStringLiteral("explorer"));
    snap.search = !isPanelCollapsed(QStringLiteral("search"));
    snap.outline = !isPanelCollapsed(QStringLiteral("outline"));
    snap.preview = m_previewVisible;
    snap.tabBar = m_tabs->tabBar()->isVisible();
}

void MainWindow::restoreLayoutSnapshot()
{
    if (!m_layoutApplied)
        return;

    LayoutSnapshot snap;
    if (m_layoutSnapshot.valid) {
        snap = m_layoutSnapshot; // 回到"主题介入之前"的形态
        m_layoutSnapshot = LayoutSnapshot {};
    } else {
        snap.valid = true;       // 没有快照：按默认形态 + 用户设置还原
        snap.activityBar = true;
        snap.statusBar = true;
        snap.explorer = settings::AppSettings::instance().sidebarVisible;
        snap.search = settings::AppSettings::instance().searchSide == QStringLiteral("left")
                      || settings::AppSettings::instance().searchSide
                             == QStringLiteral("right");
        snap.outline = true;
        snap.preview = false;
        snap.tabBar = true;
    }

    m_activityBar->setVisible(snap.activityBar);
    statusBar()->setVisible(snap.statusBar);
    setPanelExpanded(QStringLiteral("explorer"), snap.explorer);
    setPanelExpanded(QStringLiteral("search"), snap.search);
    setPanelExpanded(QStringLiteral("outline"), snap.outline);
    m_previewVisible = snap.preview;
    m_tabs->tabBar()->setVisible(snap.tabBar);
    m_tabs->setProperty("tabStyle", QString());
    repolish(m_tabs);
    for (QWidget* panel : m_sidePanels) {
        panel->setProperty("cardStyle", QString());
        repolish(panel);
    }
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto* e = editorAt(i)) {
            e->setContentMaxWidth(0);
            e->setHeadingUnderline(false);
        }
    }
    m_layoutApplied = false;
    applyLayoutPositions();
    applyMarkdownViewMode(); // 预览显隐回到按视图模式推导
}

// 面板展开/收起（走现有活动栏机制，保证按钮状态与卡片列一致）
void MainWindow::setPanelExpanded(const QString& pageId, bool expanded)
{
    QWidget* panel = panelWidget(pageId);
    if (!panel)
        return;
    if (expanded) {
        if (!columnFor(pageId))
            expandPanelFromActivityBar(pageId);
        panel->show();
        m_activityBar->setPanelChecked(pageId, true);
    } else if (columnFor(pageId)) {
        collapsePanelToActivityBar(pageId);
    }
}

void MainWindow::applyThemeLayout()
{
    auto& s = settings::AppSettings::instance();
    const editor::ThemeLayout& layout = editor::ThemeManager::currentLayout();
    const bool declined
        = s.themeLayoutDeclined.contains(editor::ThemeManager::theme());

    if (layout.isDefault() || declined) {
        restoreLayoutSnapshot(); // 主题不管形态（或用户选了"只应用配色"）
        return;
    }

    if (!m_layoutSnapshot.valid)
        captureLayoutSnapshot();
    m_layoutApplied = true;

    m_activityBar->setVisible(layout.activityBarVisible.value_or(true));
    statusBar()->setVisible(layout.statusBarVisible.value_or(true));

    if (layout.sidebarVisible)
        for (const QString& id : { QStringLiteral("explorer"), QStringLiteral("search") })
            setPanelExpanded(id, *layout.sidebarVisible);
    if (layout.outlineVisible)
        setPanelExpanded(QStringLiteral("outline"), *layout.outlineVisible);
    if (layout.previewVisible) {
        m_previewVisible = *layout.previewVisible;
    }

    // 标签形态
    if (m_tabs->property("tabStyle").toString() != layout.tabsStyle) {
        m_tabs->setProperty("tabStyle", layout.tabsStyle);
        repolish(m_tabs);
    }
    m_tabs->tabBar()->setVisible(layout.tabsStyle != QLatin1String("hidden"));

    // 侧栏卡片化
    const QString cardStyle = layout.sidebarStyle.isEmpty()
                                  ? QStringLiteral("plain")
                                  : layout.sidebarStyle;
    for (QWidget* panel : m_sidePanels) {
        if (panel->property("cardStyle").toString() != cardStyle) {
            panel->setProperty("cardStyle", cardStyle);
            repolish(panel);
        }
    }

    // 编辑器：居中限宽 + 标题下划线
    const bool centered = layout.centered.value_or(false);
    const int maxWidth = centered ? layout.maxWidth.value_or(0) : 0;
    const bool headingUnderline = layout.headingUnderline.value_or(false);
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto* e = editorAt(i)) {
            e->setContentMaxWidth(maxWidth);
            e->setHeadingUnderline(headingUnderline);
        }
    }

    applyLayoutPositions();
    updatePreviewAndOutline();
}

// 主题自带界面形态时，应用后问一次"保留布局 / 只应用配色"（可勾选不再提示）。
QStringList MainWindow::builtinActions()
{
    QStringList ids;
    for (const app::BuiltinActionInfo& info : app::builtinActionTable())
        ids.append(QString::fromLatin1(info.id));
    return ids;
}

bool MainWindow::executeBuiltinAction(const QString& action, QString* error)
{
    if (action == QStringLiteral("editor.new")) { newDocument(); return true; }
    if (action == QStringLiteral("editor.save")) { saveCurrentDocument(); return true; }
    if (action == QStringLiteral("editor.saveAs")) { saveCurrentDocumentAs(); return true; }
    if (action == QStringLiteral("editor.close")) { closeCurrentTab(); return true; }
    if (action == QStringLiteral("editor.find")) {
        if (auto* e = currentEditor())
            e->openFind(false);
        return true;
    }
    if (action == QStringLiteral("editor.quickOpen")) { showQuickOpen(); return true; }
    if (action == QStringLiteral("editor.zoomIn")) { zoomIn(); return true; }
    if (action == QStringLiteral("editor.zoomOut")) { zoomOut(); return true; }
    if (action == QStringLiteral("editor.resetZoom")) { resetZoom(); return true; }
    if (action == QStringLiteral("view.toggleSidebar")) {
        m_activityBar->explorerButton->toggle();
        return true;
    }
    if (action == QStringLiteral("view.toggleTheme")) { toggleTheme(); return true; }
    if (action == QStringLiteral("view.toggleWordWrap")) {
        toggleWordWrap(!settings::AppSettings::instance().wordWrap);
        return true;
    }
    if (action == QStringLiteral("view.toggleOutline")) {
        handlePanelToggled(QStringLiteral("outline"),
                           isPanelCollapsed(QStringLiteral("outline")));
        return true;
    }
    if (action == QStringLiteral("view.markdownLive")) {
        setMarkdownViewMode(QStringLiteral("live"));
        return true;
    }
    if (action == QStringLiteral("view.markdownSplit")) {
        setMarkdownViewMode(QStringLiteral("split"));
        return true;
    }
    if (action == QStringLiteral("view.markdownSource")) {
        setMarkdownViewMode(QStringLiteral("source"));
        return true;
    }
    if (action == QStringLiteral("app.settings")) { showSettings(); return true; }
    if (action == QStringLiteral("app.about")) { showAbout(); return true; }
    if (action == QStringLiteral("app.openFolder")) { openFolderDialog(); return true; }
    if (action == QStringLiteral("app.openFile")) { openFileDialog(); return true; }
    // 主题切换：theme.set:<id>
    if (action.startsWith(QStringLiteral("theme.set:"))) {
        const QString id = action.mid(qstrlen("theme.set:"));
        if (editor::ThemeManager::themeDefinition(id).id.isEmpty()) {
            if (error)
                *error = tr("主题不存在：%1").arg(id);
            return false;
        }
        applyThemeAndRefresh(id);
        return true;
    }
    if (error)
        *error = tr("未知的内置动作：%1").arg(action);
    return false;
}

bool MainWindow::executeShellCommand(const plugins::PluginCommand& command,
                                     const QString& pluginId, const QString& pluginName,
                                     QString* error)
{
    auto& manager = plugins::PluginManager::instance();

    // 组装命令行：{file} / {dir} / {workspace} 占位符替换（无对应值时清空参数，
    // 避免把字面量 "{file}" 传给外部程序）
    QString file;
    if (auto* editor = currentEditor()) {
        if (auto* document = documentFor(editor))
            file = document->filePath();
    }
    const QString dir = file.isEmpty() ? QString() : QFileInfo(file).absolutePath();
    QString args = plugins::PluginManager::expandShellArgs(
        command.args, file, dir, settings::AppSettings::instance().workspacePath);
    if (args.contains(QLatin1Char('{')) && file.isEmpty())
        args.clear();
    const QStringList argumentList
        = args.isEmpty() ? QStringList() : QProcess::splitCommand(args);
    const QString commandLine = args.isEmpty()
                                    ? command.shell
                                    : command.shell + QLatin1Char(' ') + args;

    if (!manager.isShellTrusted(pluginId)) {
        // 首次执行：展示完整命令行与来源，由用户决定是否信任（决策 #5）
        QMessageBox box(this);
        box.setWindowTitle(tr("插件要执行外部程序"));
        box.setIcon(QMessageBox::Warning);
        box.setText(tr("插件「%1」请求执行外部命令：").arg(pluginName));
        box.setInformativeText(QStringLiteral("<code>%1</code><br><br>%2")
                                   .arg(commandLine.toHtmlEscaped(),
                                        tr("只在你确认命令来源可信时才继续。")));
        auto* trust = new QCheckBox(tr("信任此插件（以后不再询问）"), &box);
        box.setCheckBox(trust);
        QPushButton* run = box.addButton(tr("执行"), QMessageBox::AcceptRole);
        box.addButton(tr("取消"), QMessageBox::RejectRole);
        box.setDefaultButton(run);
        box.exec();
        if (box.clickedButton() != run) {
            if (error)
                *error = tr("已取消。");
            return false;
        }
        if (trust->isChecked())
            manager.setShellTrusted(pluginId, true);
    }

    if (!QProcess::startDetached(command.shell, argumentList)) {
        if (error)
            *error = tr("无法启动：%1").arg(command.shell);
        return false;
    }
    statusBar()->showMessage(tr("已执行：%1").arg(commandLine), 4000);
    return true;
}

bool MainWindow::runPluginCommand(const QString& qualifiedId, QString* error)
{
    const auto entries = plugins::PluginManager::instance().commands();
    for (const auto& entry : entries) {
        if (entry.qualifiedId != qualifiedId)
            continue;
        if (!entry.command.action.isEmpty())
            return executeBuiltinAction(entry.command.action, error);
        if (!entry.command.shell.isEmpty()) {
            const plugins::Plugin plugin
                = plugins::PluginManager::instance().plugin(entry.pluginId);
            if (!plugin.manifest.declaresShellPermission()) {
                if (error)
                    *error = tr("插件未声明 shell 权限。");
                return false;
            }
            return executeShellCommand(entry.command, entry.pluginId, entry.pluginName,
                                       error);
        }
    }
    if (error)
        *error = tr("命令不存在：%1").arg(qualifiedId);
    return false;
}

// 命令面板：条目来自 CommandIndex，执行按 kind 分发到既有路径。
void MainWindow::showCommandPalette()
{
    app::CommandPalette palette(this);
    connect(&palette, &app::CommandPalette::commandChosen, this,
            [this](const app::PaletteEntry& entry) {
                QString error;
                bool ok = false;
                switch (entry.kind) {
                case app::PaletteEntry::Builtin:
                    ok = executeBuiltinAction(entry.id, &error);
                    break;
                case app::PaletteEntry::Plugin:
                    ok = runPluginCommand(entry.id, &error);
                    break;
                case app::PaletteEntry::Theme:
                    // theme.set:<id> 走同一条内置动作路径
                    ok = executeBuiltinAction(entry.id, &error);
                    break;
                }
                if (!ok && !error.isEmpty())
                    statusBar()->showMessage(error, 5000);
            });
    palette.exec();
}

void MainWindow::refreshPluginContributions()
{
    // 插件认领的文件格式 → 语言检测（插件层与语法层由这里接线）
    syntax::LanguageRegistry::instance().setExtensionOverrides(
        plugins::PluginManager::instance().formatsMap());
    rebuildPluginMenu();
    rebuildPluginStatusItems();
    rebuildPluginPages();
}

// ---- 插件 statusBar 部件 ----

void MainWindow::rebuildPluginStatusItems()
{
    // 先拆掉旧段（插件被禁用/卸载后要消失）
    for (const PluginStatusWidget& widget : m_pluginStatusWidgets) {
        if (widget.label) {
            statusBar()->removeWidget(widget.label);
            widget.label->deleteLater();
        }
    }
    m_pluginStatusWidgets.clear();

    for (const plugins::Plugin& plugin :
         plugins::PluginManager::instance().enabledPlugins()) {
        for (const plugins::PluginStatusItem& spec : plugin.manifest.statusItems) {
            auto* label = new QLabel(statusBar());
            label->setObjectName(QStringLiteral("pluginStatus_%1_%2")
                                     .arg(plugin.manifest.id, spec.id));
            label->setToolTip(spec.type);
            if (spec.alignment == QLatin1String("left"))
                statusBar()->addWidget(label);
            else
                statusBar()->addPermanentWidget(label);
            PluginStatusWidget entry;
            entry.label = label;
            entry.spec = spec;
            m_pluginStatusWidgets.append(entry);
        }
    }
    updatePluginStatusItems();
}

void MainWindow::updatePluginStatusItems()
{
    if (m_pluginStatusWidgets.isEmpty())
        return;
    auto* editor = currentEditor();
    auto* doc = editor ? documentFor(editor) : nullptr;
    const QString languageId = doc && doc->language() ? doc->language()->id : QString();

    for (PluginStatusWidget& entry : m_pluginStatusWidgets) {
        if (!entry.label)
            continue;
        const bool visible
            = entry.spec.visibleFor.isEmpty() || entry.spec.visibleFor.contains(languageId);
        entry.label->setVisible(visible);
        if (!visible) {
            entry.label->clear();
            continue;
        }
        if (!editor) {
            entry.label->clear();
            continue;
        }

        const QString text = editor->toPlainText();
        const int blocks = editor->blockCount();
        // 字/字符数按 (文本长度, 块数) 缓存，光标移动不重算
        const auto ensureCount = [&](int type) {
            if (entry.cachedLength == text.size() && entry.cachedBlocks == blocks
                && entry.cachedValue >= 0)
                return;
            int value = 0;
            if (type == 0) { // wordCount：按空白切分的"词"数（中英混排按字块计）
                bool inWord = false;
                for (const QChar& ch : text) {
                    const bool space = ch.isSpace();
                    if (!space && !inWord)
                        ++value;
                    inWord = !space;
                }
            } else { // charCount
                value = int(text.size());
            }
            entry.cachedLength = text.size();
            entry.cachedBlocks = blocks;
            entry.cachedValue = value;
        };

        const QString type = entry.spec.type;
        if (type == QLatin1String("wordCount")) {
            ensureCount(0);
            entry.label->setText(tr("%1 字").arg(entry.cachedValue));
        } else if (type == QLatin1String("charCount")) {
            ensureCount(1);
            entry.label->setText(tr("%1 字符").arg(entry.cachedValue));
        } else if (type == QLatin1String("lineCount")) {
            entry.label->setText(tr("%1 行").arg(blocks));
        } else if (type == QLatin1String("indent")) {
            const QTextCursor cursor = editor->textCursor();
            const int spaces = cursor.positionInBlock();
            const int tabWidth = qMax(1, settings::AppSettings::instance().tabWidth);
            entry.label->setText(tr("缩进 %1（%2 空格）")
                                     .arg(spaces / tabWidth)
                                     .arg(spaces));
        } else if (type == QLatin1String("eol")) {
            entry.label->setText(core::eolDisplayName(doc ? doc->eol() : core::Eol::Lf));
        } else if (type == QLatin1String("encoding")) {
            entry.label->setText(doc ? core::encodingDisplayName(doc->encoding())
                                     : core::encodingDisplayName(core::Encoding::Utf8));
        }
    }
}

// ---- 插件 pages：侧栏页面 ----

void MainWindow::rebuildPluginPages()
{
    for (QWidget* page : m_pluginPages) {
        const QString pageId = page->objectName();
        m_sidePanels.remove(pageId);
        m_panelCollapsed.remove(pageId);
        if (PanelColumn* column = columnFor(pageId))
            column->takePanel(page);
        page->setParent(nullptr);
        page->deleteLater();
    }
    m_pluginPages.clear();
    m_activityBar->removePanelButtonsForPrefix(QStringLiteral("plugin:"));

    for (const plugins::Plugin& plugin :
         plugins::PluginManager::instance().enabledPlugins()) {
        for (const plugins::PluginPage& page : plugin.manifest.pages) {
            auto* panel = new plugins::PluginPagePanel(plugin.manifest.id,
                                                       plugin.manifest, page, this);
            const QString pageId = panel->objectName();
            QIcon icon;
            const QString iconPath
                = plugins::PluginManager::instance().resolveFile(plugin.manifest.id,
                                                                 page.icon);
            const QColor glyph = editor::ThemeManager::color(QStringLiteral("sidebarText"));
            const QPixmap pixmap = editor::IconLoader::loadFile(
                iconPath, glyph, editor::ThemeManager::tokens().activityBarBackground, 18);
            if (!pixmap.isNull())
                icon = QIcon(pixmap);
            m_activityBar->addPanelButton(pageId, QString(), page.title, false, icon);
            registerSidePanel(pageId, panel);
            connect(m_activityBar, &ActivityBar::panelToggled, this,
                    &MainWindow::handlePanelToggled);
            connect(panel, &plugins::PluginPagePanel::fileActivated, this,
                    &MainWindow::openPath);
            m_pluginPages.append(panel);
        }
    }
    applyLayoutPositions();
}

void MainWindow::rebuildPluginMenu()
{
    if (!m_pluginMenu)
        return;
    m_pluginMenu->clear();

    const auto entries = plugins::PluginManager::instance().commands();
    if (entries.isEmpty()) {
        QAction* empty = m_pluginMenu->addAction(tr("（没有已启用的插件命令）"));
        empty->setEnabled(false);
    } else {
        QString lastPlugin;
        for (const auto& entry : entries) {
            if (!lastPlugin.isEmpty() && lastPlugin != entry.pluginId)
                m_pluginMenu->addSeparator();
            if (lastPlugin != entry.pluginId) {
                QAction* title = m_pluginMenu->addAction(entry.pluginName);
                title->setEnabled(false);
                lastPlugin = entry.pluginId;
            }
            QAction* action = m_pluginMenu->addAction(
                QStringLiteral("    %1").arg(entry.command.title));
            if (!entry.command.shortcut.isEmpty())
                action->setShortcut(QKeySequence(entry.command.shortcut));
            const QString qualifiedId = entry.qualifiedId;
            connect(action, &QAction::triggered, this, [this, qualifiedId] {
                QString error;
                if (!runPluginCommand(qualifiedId, &error) && !error.isEmpty())
                    statusBar()->showMessage(error, 5000);
            });
        }
    }
    m_pluginMenu->addSeparator();
    QAction* manage = m_pluginMenu->addAction(tr("管理插件…"));
    connect(manage, &QAction::triggered, this, &MainWindow::showSettings);
}

void MainWindow::maybeConfirmThemeLayout()
{
    auto& s = settings::AppSettings::instance();
    const QString themeId = editor::ThemeManager::theme();
    const editor::ThemeLayout& layout = editor::ThemeManager::currentLayout();
    if (layout.isDefault() || s.themeLayoutPromptSuppressed
        || s.themeLayoutApproved.contains(themeId)
        || s.themeLayoutDeclined.contains(themeId))
        return;

    const editor::ThemeDefinition theme = editor::ThemeManager::currentTheme();
    QPushButton* keep = app::dialog::show(
        this, tr("主题会调整界面布局"),
        tr("「%1」会调整界面布局：%2。").arg(theme.name.isEmpty() ? themeId : theme.name,
                                             layout.summary()),
        tr("保留该布局，还是只应用配色？"),
        app::dialog::Icon::Question,
        { { tr("保留布局"), app::dialog::Button::Ok },
          { tr("只应用配色"), app::dialog::Button::Cancel } });
    // colorsOnly == keep == nullptr
    // 简化：不再提供"以后不再提示"复选框（用户可在设置里控制主题）

    if (keep == nullptr) { // 只应用配色
        if (!s.themeLayoutDeclined.contains(themeId))
            s.themeLayoutDeclined.append(themeId);
        // 同一次操作里改主意：立刻把形态还回去
        applyThemeLayout();
    } else if (!s.themeLayoutApproved.contains(themeId)) {
        s.themeLayoutApproved.append(themeId);
    }
    s.save();
}

void MainWindow::toggleTheme()
{
    // 深色 <-> 简约白 快速往返；完整主题列表在设置对话框里
    const QString next = editor::ThemeManager::currentIsDark()
                             ? QStringLiteral("paper")
                             : QStringLiteral("greenwood-dark");
    applyThemeAndRefresh(next);
}

void MainWindow::setupFileAssociations()
{
    QString error;
    const QStringList written = app::associations::writeUserAssociations(&error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("文件关联"), error);
        return;
    }

    const QList<app::associations::ExtensionStatus> pending
        = app::associations::choicesNeedingUserAction();
    QString text = tr("已为当前用户登记 %1 个文件类型的关联。\n\n").arg(written.size());
    if (pending.isEmpty()) {
        text += tr("这些类型现在都会用本程序打开（若某个类型此前被别的程序"
                   "指定过默认，可在系统「默认应用」里改）。");
    } else {
        QStringList lines;
        for (const auto& status : pending)
            lines.append(QStringLiteral("%1（当前：%2）").arg(status.extension, status.progId));
        text += tr("下面这些已被 Windows 的「默认应用」占用，系统不允许程序代改：\n"
                   "%1\n\n点「打开默认应用设置」把对应类型改成 Hutaomu Editor，"
                   "以后双击就不会再问。").arg(lines.join(QStringLiteral("\n")));
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("文件关联"));
    box.setIcon(QMessageBox::Information);
    box.setText(text);
    QPushButton* open = box.addButton(tr("打开默认应用设置"), QMessageBox::AcceptRole);
    box.addButton(tr("关闭"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == open)
        app::associations::openDefaultAppsSettings();
}

void MainWindow::maybePromptFileAssociations()
{
    if (!app::associations::shouldPromptOnStartup())
        return;
    // 只在"确实有让别人占着的类型"时提示：没装关联或都归我们就不打扰
    const QList<app::associations::ExtensionStatus> pending
        = app::associations::choicesNeedingUserAction();
    if (pending.isEmpty())
        return;

    QMessageBox box(this);
    box.setWindowTitle(tr("文件关联"));
    box.setIcon(QMessageBox::Question);
    box.setText(tr("有 %1 个文件类型被别的程序占着（例如 %2）。\n\n"
                   "Windows 不允许程序代改默认程序，需要你到系统设置里点一次。")
                    .arg(pending.size())
                    .arg(pending.first().extension + QStringLiteral("：")
                         + pending.first().progId));
    QCheckBox* never = new QCheckBox(tr("不再提示"), &box);
    box.setCheckBox(never);
    QPushButton* go = box.addButton(tr("去设置"), QMessageBox::AcceptRole);
    box.addButton(tr("以后再说"), QMessageBox::RejectRole);
    box.setDefaultButton(go);
    box.exec();
    if (never->isChecked())
        app::associations::markStartupPromptShown();
    if (box.clickedButton() == go)
        app::associations::openDefaultAppsSettings();
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("About Hutaomu Editor"),
        tr("<b>Hutaomu Editor</b> %1<br/><br/>"
           "一款原生跨平台的文本、代码与 Markdown 编辑器。<br/><br/>"
           "本程序按 <a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">"
           "GNU AGPL-3.0</a> 许可证永久开源；源码见项目仓库。<br/>"
           "基于 Qt 6 构建（LGPL-3.0），第三方组件许可详见 THIRDPARTY.md。")
            .arg(QString::fromLatin1(HUTAOMU_VERSION)));
}

void MainWindow::saveSettings()
{
    auto& s = settings::AppSettings::instance();
    s.mainWindowGeometry = saveGeometry();
    s.mainWindowState = saveState();
    s.splitterState = m_splitter->saveState();
    s.maximized = isMaximized();
    s.sidebarVisible = m_explorer->isVisible();
    s.save();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        // 查看器标签页：脏状态时询问保存
        if (auto* viewer = qobject_cast<viewers::DocumentViewer*>(
                m_tabs->widget(i))) {
            if (viewer->isModified()) {
                const QMessageBox::StandardButton choice =
                    QMessageBox::question(
                        this, tr("保存更改"),
                        tr("是否保存对“%1”的更改？").arg(m_tabs->tabText(i)),
                        QMessageBox::Save | QMessageBox::Discard
                            | QMessageBox::Cancel,
                        QMessageBox::Save);
                if (choice == QMessageBox::Cancel) {
                    event->ignore();
                    return;
                }
                if (choice == QMessageBox::Save && !viewer->save()) {
                    event->ignore();
                    return;
                }
            }
            continue;
        }
        auto* editor = editorAt(i);
        if (promptSave(editor) == SaveResult::Cancelled) {
            event->ignore();
            return;
        }
    }

    saveSettings();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile())
            openPath(url.toLocalFile());
    }
    event->acceptProposedAction();
}

} // namespace app
