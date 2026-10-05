// Hutaomu Editor - Main window shell (VS Code style layout).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QHash>
#include <QLabel>
#include <QMainWindow>

#include "plugins/PluginManifest.h"
#include "plugins/PluginManager.h"
#include <QStringList>

class QSplitter;
class QTabWidget;
class QToolButton;

namespace core {
class Document;
}

namespace editor {
class CodeEditor;
class DropFeedbackLayer;
class PanelDragGhost;
class PanelDragMover;
enum class Theme;
}

namespace app {
class ActivityBar;
class PanelColumn;
class PanelDragGhost;
class TitleBar;
}

namespace panels {
class ExplorerPanel;
class MarkdownPreview;
class OutlinePanel;
class SearchPanel;
}

class QStackedWidget;

namespace app {

enum class SaveResult {
    Saved,
    Discarded,
    Cancelled,
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openPath(const QString& filePath);
    void setMarkdownViewMode(const QString& mode);
    void setPreviewPosition(const QString& position);
    void setOutlinePosition(const QString& position);   // left / right
    void setSidebarPosition(const QString& position);   // left / right
    void applyLayoutPositions();                        // 统一重排三面板位置
    bool isPanelCollapsed(const QString& pageId) const; // 面板是否收起为活动栏按钮（测试可达）
    QFont editorFontForTest() const { return editorFont(); } // 测试可达：当前生效字体（含主题覆盖）
    void handlePanelToggled(const QString& pageId, bool checked); // 活动栏页面切换
    void finishPanelDrag(const QString& panelId, const QPoint& globalPos); // 拖拽落点结算（测试可达）
    // 应用指定主题并刷新所有依赖主题的部件（图标/标签/编辑器/预览）；测试可达。
    void applyThemeAndRefresh(const QString& themeId);
    // 只应用到界面、不写设置（设置页实时预览用）。
    void previewTheme(const QString& themeId);
    // 应用当前主题声明的界面形态（T4；主题没有 layout.json 时恢复默认形态）。
    void applyThemeLayout();
    // 插件贡献点：把格式映射推给语言注册表 + 重建"插件"菜单
    // （启动后与设置页关闭后调用）。
    void refreshPluginContributions();
    void rebuildPluginMenu();
    // 执行插件命令（内置动作或 shell 外部命令）；测试可达。
    bool runPluginCommand(const QString& qualifiedId, QString* error = nullptr);
    // 内置动作 id 列表（执行表见 CommandIndex::builtinActionTable）
    static QStringList builtinActions();
    // 测试可达：当前是否处于"该主题的布局已生效"状态。
    bool themeLayoutActive() const;

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
#ifdef Q_OS_WIN
    // Native edge-resize hit testing for the frameless window.
    bool nativeEvent(const QByteArray& eventType, void* message,
                     qintptr* result) override;
#endif

private slots:
    void newDocument();
    void openFileDialog();
    void openFolderDialog();
    void saveCurrentDocument();
    void saveCurrentDocumentAs();
    void closeCurrentTab();
    void cycleTabs(int direction);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void toggleWordWrap(bool wrap);
    void toggleTheme();
    void showAbout();
    // 文件关联：写当前用户关联并如实报告被系统"默认应用"占住的项
    void setupFileAssociations();
    // 首次启动引导（只提示一次）
    void maybePromptFileAssociations();
    void updateStatusBar();
    void rebuildRecentFilesMenu();
    void showSearchPanel();
    void showQuickOpen();
    void showCommandPalette();
    void showSettings();
    void zoomBy(double factor);
    void applyMarkdownViewMode();
    void updatePreviewAndOutline();

private:
    void buildChrome();
    void setupPanelDrag(); // 面板头部拖拽换边
    void buildMenus();
    void applyWindowChrome();
    void syncNewTabButton();

    editor::CodeEditor* createEditor();
    editor::CodeEditor* currentEditor() const;
    editor::CodeEditor* editorAt(int index) const;
    static core::Document* documentFor(editor::CodeEditor* editor);
    QString editorDisplayName(editor::CodeEditor* editor) const;

    SaveResult promptSave(editor::CodeEditor* editor);
    bool saveEditor(editor::CodeEditor* editor, bool saveAs);
    void closeTab(int index);
    void handleDiskChange(editor::CodeEditor* editor);
    bool isBlankUntitled(editor::CodeEditor* editor) const;

    void updateTabTitle(editor::CodeEditor* editor);
    void updateWindowTitle();
    void setWorkspace(const QString& path);
    void applyEditorFont(editor::CodeEditor* editor);
    void applyFontToAllEditors();
    QFont editorFont() const;
    void rememberRecentFile(const QString& filePath);
    void saveSettings();

    // VS Code style chrome
    TitleBar* m_titleBar = nullptr;
    ActivityBar* m_activityBar = nullptr;
    panels::ExplorerPanel* m_explorer = nullptr;
    QSplitter* m_splitter = nullptr;
    QTabWidget* m_tabs = nullptr;
    QToolButton* m_newTabButton = nullptr;
    QSplitter* m_editorSplitter = nullptr;
    panels::MarkdownPreview* m_preview = nullptr;
    panels::OutlinePanel* m_outline = nullptr;
    panels::SearchPanel* m_search = nullptr;
    PanelColumn* m_leftColumn = nullptr;   // 左侧卡片列
    PanelColumn* m_rightColumn = nullptr;  // 右侧卡片列
    editor::PanelDragGhost* m_dragGhost = nullptr; // 拖动幻影
    class QTimer* m_refreshDebounce = nullptr;
    bool m_syncingScroll = false;
    bool m_previewVisible = false; // applyMarkdownViewMode 记录的预览可见性

    // 主题形态介入前的界面快照：主题撤销布局时用它原样还回去，
    // 避免主题改动写坏用户自己的面板/状态栏状态。
    struct LayoutSnapshot {
        bool valid = false;
        bool activityBar = true;
        bool statusBar = true;
        bool explorer = true;
        bool search = true;
        bool outline = true;
        bool preview = false;
        bool tabBar = true;
    };
    LayoutSnapshot m_layoutSnapshot;
    bool m_layoutApplied = false;

    void maybeConfirmThemeLayout(); // 应用含形态变更的主题后提示一次
    // 插件 statusBar 部件：重建段并在文本/光标变化时刷新
    void rebuildPluginStatusItems();
    void updatePluginStatusItems();
    // 插件 pages：按清单创建面板并注册活动栏按钮
    void rebuildPluginPages();
    bool executeBuiltinAction(const QString& action, QString* error);
    bool executeShellCommand(const plugins::PluginCommand& command,
                             const QString& pluginId, const QString& pluginName,
                             QString* error);
    QMenu* m_pluginMenu = nullptr;

    // 插件状态栏段：(标签, 规格, 语言过滤)。字/字符数按文档内容缓存，避免每次移动光标都全量统计。
    struct PluginStatusWidget {
        QLabel* label = nullptr;
        plugins::PluginStatusItem spec;
        QString cachedLanguageId;
        qsizetype cachedLength = -1;
        int cachedBlocks = -1;
        int cachedValue = -1;
    };
    QList<PluginStatusWidget> m_pluginStatusWidgets;
    QList<QWidget*> m_pluginPages;
    void captureLayoutSnapshot();
    void restoreLayoutSnapshot();
    void setPanelExpanded(const QString& pageId, bool expanded);
    void repolish(QWidget* widget);
    editor::PanelDragMover* m_sidebarDrag = nullptr;   // 面板头部拖拽
    editor::PanelDragMover* m_outlineDrag = nullptr;
    editor::PanelDragMover* m_previewDrag = nullptr;
    editor::PanelDragMover* m_searchDrag = nullptr;
    editor::DropFeedbackLayer* m_dropFeedback = nullptr; // 拖拽落点反馈层
    bool m_sideSplitterVisible = true; // 侧栏区整体可见性（Ctrl+B）
    bool m_outlineAutoCollapsed = false; // 上次是系统（非用户）收起的大纲

    // ---- 面板卡片注册表（VS Code sections 模型）----
    // 每个面板是一张卡片，停靠在左列或右列，同列多卡全部可见垂直排布；
    // 拖动卡片标题栏 = 移动/换列/重排；拖到活动栏 = 收起为按钮；
    // 活动栏按钮 = 展开为卡片（默认左列顶部）/再次点击收起。
    QHash<QString, QWidget*> m_sidePanels;  // pageId -> 面板 widget
    QHash<QString, bool> m_panelCollapsed;  // pageId -> 是否收起为按钮
    void registerSidePanel(const QString& pageId, QWidget* panel);
    PanelColumn* columnFor(const QString& pageId) const;
    void movePanelToColumn(const QString& pageId, const QString& side,
                           const QString& beforeId = QString());
    void collapsePanelToActivityBar(const QString& pageId);
    void expandPanelFromActivityBar(const QString& pageId);
    QString pageIdForPanel(QWidget* panel) const;
    QWidget* panelWidget(const QString& pageId) const;
    QString panelWindowTitle(const QString& pageId) const;

    // Status bar segments, right to left like VS Code.
    QLabel* m_positionLabel = nullptr;
    QLabel* m_eolLabel = nullptr;
    QLabel* m_encodingLabel = nullptr;
    QLabel* m_languageLabel = nullptr;

    class QMenu* m_recentMenu = nullptr;
    int m_untitledCounter = 0;
};

} // namespace app
