// Hutaomu Editor - 插件管理侧边栏面板（非模态，VS Code 扩展视图形式）。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QWidget>

class QCheckBox;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QToolButton;

namespace panels {

// 停靠在侧栏列里的插件管理卡片：
// 列表页（搜索 + 卡片 + 启用开关）⇄ 详情页（贡献点/权限/操作），
// 支持拖放 .htmed 安装、导出插件包、打开所在文件夹、右键菜单。
class PluginPanel : public QWidget {
    Q_OBJECT
public:
    explicit PluginPanel(QWidget* parent = nullptr);

    void refreshIcons();
    QWidget* headerWidget() const { return m_header; } // 拖拽换边把手

public slots:
    void reload(); // 重建列表（插件增删/启停后）

signals:
    // 启停 / 导入 / 卸载后发出；宿主刷新命令、状态栏、页面等贡献
    void pluginsChanged();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void importPackage();
    void importPackageFrom(const QString& packagePath);
    void uninstallSelected();
    void toggleSelected(const QString& pluginId, bool enabled);
    void applyFilter();
    void showDetails(const QString& pluginId);
    void showList();
    bool exportPackage(const QString& pluginId);
    void openPluginFolder(const QString& pluginId);

    QWidget* m_header = nullptr;
    QLabel* m_subtitle = nullptr;
    QLineEdit* m_search = nullptr;
    QStackedWidget* m_pages = nullptr;
    QListWidget* m_list = nullptr;
    QToolButton* m_importButton = nullptr;
    QToolButton* m_uninstallButton = nullptr;
    QToolButton* m_refreshButton = nullptr;
    QLabel* m_hint = nullptr;

    // 详情页
    QString m_detailsId;
    QLabel* m_detailsAvatar = nullptr;
    QLabel* m_detailsName = nullptr;
    QLabel* m_detailsVersion = nullptr;
    QLabel* m_detailsMeta = nullptr;
    QLabel* m_detailsDescription = nullptr;
    QLabel* m_detailsPermissions = nullptr;
    QLabel* m_detailsContrib = nullptr;
    QCheckBox* m_detailsToggle = nullptr;
    QToolButton* m_detailsUninstall = nullptr;
    QToolButton* m_detailsFolder = nullptr;
    QToolButton* m_detailsExport = nullptr;
};

} // namespace panels
