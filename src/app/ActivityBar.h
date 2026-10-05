// Hutaomu Editor - Left icon activity bar (VS Code style panel registry).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QHash>
#include <QWidget>

class QToolButton;
class QVBoxLayout;

namespace app {

// VS Code style icon strip on the far left. 每个侧边栏页面在活动栏上有
// 一个按钮（点击切换显示）；页面标题栏拖到活动栏上 = 收起为按钮，
// 按钮拖回侧边栏 = 恢复为面板。新页面通过 addPanelButton 注册。
class ActivityBar : public QWidget {
    Q_OBJECT
public:
    explicit ActivityBar(QWidget* parent = nullptr);

    void refreshIcons();

    // 注册一个页面按钮：pageId 与 MainWindow 的侧边栏页面对应。
    // 返回的按钮由活动栏管理生命周期。
    // 移除某前缀的面板按钮（插件页面随插件启停出现/消失）
    void removePanelButtonsForPrefix(const QString& prefix);
    QToolButton* addPanelButton(const QString& pageId, const QString& iconName,
                                const QString& toolTip, bool checked = false,
                                const QIcon& icon = QIcon());
    void setPanelChecked(const QString& pageId, bool checked);
    bool isPanelChecked(const QString& pageId) const;
    void removePanelButton(const QString& pageId);
    // 按钮垂直顺序（拖拽重排后变化）：返回按当前视觉顺序排列的 pageId
    QStringList panelOrder() const;
    void restoreOrder(const QStringList& pageIds);

    QToolButton* explorerButton = nullptr;
    QToolButton* searchButton = nullptr;

signals:
    void explorerToggled(bool checked);
    void searchToggled(bool checked);
    void panelToggled(const QString& pageId, bool checked); // 动态页面
    void aboutRequested();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QToolButton* makeButton(const QString& iconName, const QString& toolTip);
    void restyleButton(QToolButton* button);
    void restackButtons(); // 按钮拖拽重排后按视觉顺序重装

    QHash<QString, QToolButton*> m_pageButtons; // pageId -> 按钮
    QVector<QToolButton*> m_buttons;
    QString m_dropBeforeId; // 拖放插入位置（在该按钮之前）
};

} // namespace app
