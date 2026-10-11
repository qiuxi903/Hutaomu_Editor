// Hutaomu Editor - 插件管理侧边栏面板（非模态，VS Code 扩展视图形式）。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QWidget>

class QLabel;
class QListWidget;
class QToolButton;

namespace panels {

// 停靠在侧栏列里的插件管理卡片：列出已装插件（勾选 = 启用），
// 导入 / 卸载都在面板内完成，不再弹独立窗口。
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

private:
    void importPackage();
    void uninstallSelected();
    void toggleSelected(const QString& pluginId, bool enabled);

    QWidget* m_header = nullptr;
    QListWidget* m_list = nullptr;
    QToolButton* m_importButton = nullptr;
    QToolButton* m_uninstallButton = nullptr;
    QLabel* m_hint = nullptr;
};

} // namespace panels
