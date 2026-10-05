// Hutaomu Editor - Frameless title bar with embedded menus (VS Code style).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QEvent>
#include <QLabel>
#include <QMenuBar>
#include <QWidget>

class QToolButton;

namespace app {

// Rendered as the QMainWindow menu widget (setMenuWidget). Owns the window
// controls and exposes the QMenuBar so MainWindow can populate actions; the
// OS window is frameless, so this bar is also the drag handle.
class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget* parent = nullptr);

    QMenuBar* menuBar() const { return m_menuBar; }

public slots:
    void refreshIcons();
    void updateMaximizeButton();

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QToolButton* makeWindowButton(const QString& iconName, const QString& objectName);
    void beginWindowDrag(const QPoint& globalPos); // 开始拖动（含最大化还原）
    void continueWindowDrag(const QPoint& globalPos);
    void endWindowDrag();
    bool m_dragging = false;
    QPoint m_dragOffset; // 光标相对窗口左上角

    QMenuBar* m_menuBar = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_brandIcon = nullptr;
    QLabel* m_brandName = nullptr;
    QToolButton* m_minButton = nullptr;
    QToolButton* m_maxButton = nullptr;
    QToolButton* m_closeButton = nullptr;
};

} // namespace app
