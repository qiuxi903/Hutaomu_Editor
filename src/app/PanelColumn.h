// Hutaomu Editor - One sidebar column: a vertical stack of panel cards.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

class QVBoxLayout;

namespace app {

// 侧边栏的一"列"：若干面板卡片垂直堆叠、全部同时可见（VS Code 的
// sidebar sections 模型）。卡片 = 面板自身（带标题栏），列负责：
//   - 卡片的增删与顺序（insertPanel / takePanel / panelOrder）；
//   - 拖放插入位置的判定与指示线（dropIndicatorPos / hitTestId）；
//   - 空列自动隐藏。
class PanelColumn : public QWidget {
    Q_OBJECT
public:
    explicit PanelColumn(const QString& id, QWidget* parent = nullptr);

    void addPanel(QWidget* panel);                       // 追加到末尾
    void insertPanel(QWidget* panel, int index);         // 插入到 index
    void takePanel(QWidget* panel);                      // 摘除
    bool containsPanel(QWidget* panel) const;
    int panelCount() const;
    QStringList panelOrder() const;                      // 面板 objectName 序

    // 拖放指示：给出全局坐标，返回"插入到哪个面板之前"（objectName），
    // 空串 = 追加到末尾；同时设置内部指示线位置。
    QString dropTarget(QWidget* dragging, const QPoint& globalPos);
    void clearDropIndicator();
    // 拖拽落点命中的面板 id（拖到活动栏收起时用）
    QString hitTestId(const QPoint& globalPos) const;

    int idealWidth() const { return 280; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void relayout();
    QWidget* panelAt(int index) const;
    QWidget* panelForId(const QString& id) const; // objectName -> 面板

    QString m_id;
    QVBoxLayout* m_layout = nullptr;
    QList<QWidget*> m_panels;
    QWidget* m_indicatorBefore = nullptr; // 指示线画在该面板之前
    bool m_indicatorActive = false;
};

} // namespace app
