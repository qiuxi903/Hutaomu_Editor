// Hutaomu Editor - Command palette implementation (modelled on QuickOpenDialog).
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "CommandPalette.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace app {

CommandPalette::CommandPalette(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("commandPalette"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowTitle(tr("命令面板"));
    setMinimumWidth(560);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("输入命令名…（例如：保存 / 主题 / wrap）"));
    m_input->installEventFilter(this);
    layout->addWidget(m_input);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("commandList"));
    m_list->setAlternatingRowColors(false);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setUniformItemSizes(true);
    layout->addWidget(m_list);

    m_all = buildPaletteEntries();
    refreshList();

    connect(m_input, &QLineEdit::textChanged, this, [this] { refreshList(); });
    connect(m_list, &QListWidget::itemActivated, this,
            [this](QListWidgetItem*) { activateCurrent(); });
    m_input->setFocus();
}

void CommandPalette::setQuery(const QString& query)
{
    m_input->setText(query);
    refreshList();
}

void CommandPalette::refreshList()
{
    m_filtered = filterPaletteEntries(m_all, m_input->text());
    m_list->clear();
    for (const PaletteEntry& entry : m_filtered) {
        QString text = entry.category.isEmpty()
                           ? entry.title
                           : QStringLiteral("%1  ·  %2").arg(entry.category, entry.title);
        if (!entry.shortcut.isEmpty())
            text += QStringLiteral("    [%1]").arg(entry.shortcut);
        auto* item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, entry.id);
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void CommandPalette::activateCurrent()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_filtered.size())
        return;
    const PaletteEntry entry = m_filtered.at(row);
    emit commandChosen(entry);
    accept();
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    // 上下键在输入框里也能移动列表选择（VS Code 手感）
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Down || keyEvent->key() == Qt::Key_Up) {
            const int delta = keyEvent->key() == Qt::Key_Down ? 1 : -1;
            const int row = qBound(0, m_list->currentRow() + delta, m_list->count() - 1);
            m_list->setCurrentRow(row);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CommandPalette::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        activateCurrent();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    QDialog::keyPressEvent(event);
}

} // namespace app
