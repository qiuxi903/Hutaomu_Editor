// Hutaomu Editor - Drop feedback layer for panel drags.
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QString>
#include <QWidget>

namespace editor {

// 拖动面板时的落点反馈层（全窗口透明，仅绘制）：
//   - 悬停在侧栏列上：列内画插入指示线（PanelColumn 自己画，本层不管）；
//   - 悬停在活动栏上：活动栏高亮 + "收起为按钮"提示（本层负责）；
//   - 悬停其它区域：轻微暗化（表示无效落点）。
// 纯指示层（WA_TransparentForMouseEvents），事件流由头部把手驱动。
class DropFeedbackLayer : public QWidget {
    Q_OBJECT
public:
    enum class Target { None, ActivityBar };

    explicit DropFeedbackLayer(QWidget* parent = nullptr);

    void begin(const QString& panelId);
    void end();
    // 每次拖动移动调用；hoverActivityBar 返回是否悬停在活动栏上
    void updateHover(const QPoint& globalPos, bool overActivityBar);
    Target target() const { return m_target; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_panelId;
    Target m_target = Target::None;
};

} // namespace editor