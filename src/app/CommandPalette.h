// Hutaomu Editor - Command palette (Ctrl+Shift+P): fuzzy-searchable commands.
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QDialog>
#include <QList>

#include "app/CommandIndex.h"

class QLineEdit;
class QListWidget;

namespace app {

// 命令面板：输入即过滤（子序列模糊匹配），回车执行。
// 条目来自 CommandIndex（内置动作 + 插件命令 + 主题切换），
// 执行交给 MainWindow（内置动作/插件命令分别走各自的分发路径）。
class CommandPalette : public QDialog {
    Q_OBJECT
public:
    explicit CommandPalette(QWidget* parent = nullptr);

    // 测试可达：当前过滤结果（按得分排序）
    QList<PaletteEntry> currentEntries() const { return m_filtered; }
    void setQuery(const QString& query); // 测试可达：等价于用户输入

signals:
    void commandChosen(const PaletteEntry& entry);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void refreshList();
    void activateCurrent();

    QList<PaletteEntry> m_all;
    QList<PaletteEntry> m_filtered;
    QLineEdit* m_input = nullptr;
    QListWidget* m_list = nullptr;
};

} // namespace app
