// Hutaomu Editor - Panel header drag (ghost follows cursor).
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QEvent>
#include <QPointer>
#include <QPoint>
#include <QString>
#include <QWidget>

namespace editor {

// 拖拽 mime 标识（ActivityBar 收起面板时共用）
inline const char* panelIdMime() { return "application/x-hutaomu-panel-id"; }

// 让"面板头部"成为拖拽把手（VS Code 手感，鼠标跟踪实现）：
//   - 按住头部拖过阈值 -> dragStarted（宿主显示落点提示）；
//   - 拖动中 dragMoved（全局坐标；宿主刷新插入指示 + 幻影跟随）；
//   - 松手 dragFinished（宿主按落点结算换位）。
// 幻影 = 半透明标题条由宿主管理（PanelDragGhost），本类只报坐标。
class PanelDragMover : public QObject {
    Q_OBJECT
public:
    explicit PanelDragMover(QWidget* header, const QString& panelId,
                            QObject* parent);

signals:
    void dragStarted(const QString& panelId);
    void dragMoved(const QString& panelId, const QPoint& globalPos);
    void dragFinished(const QString& panelId, const QPoint& globalPos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* m_header = nullptr;
    QString m_panelId;
    bool m_pressed = false;
    bool m_dragging = false;
    QPoint m_pressGlobal;
};

// 拖动中跟随鼠标的半透明标题条（全窗口顶层，纯指示，不接事件）。
class PanelDragGhost : public QWidget {
    Q_OBJECT
public:
    explicit PanelDragGhost(QWidget* parent = nullptr);
    void showFor(const QString& title, const QPoint& globalPos, int width);
    void moveTo(const QPoint& globalPos);
    void hideGhost();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_title;
};

} // namespace editor