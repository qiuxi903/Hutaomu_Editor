// Hutaomu Editor - Quick file open (Ctrl+P).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "QuickOpenDialog.h"

#include <QDir>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace app {

QuickOpenDialog::QuickOpenDialog(const QStringList& files, QWidget* parent)
    : QDialog(parent)
    , m_files(files)
{
    setObjectName(QStringLiteral("quickOpen"));
    setWindowTitle(tr("快速打开"));
    setModal(true);
    setMinimumSize(560, 420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("quickOpenInput"));
    m_input->setPlaceholderText(tr("输入文件名过滤，回车打开..."));
    layout->addWidget(m_input);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("quickOpenList"));
    layout->addWidget(m_list, 1);

    connect(m_input, &QLineEdit::textChanged,
            this, [this] { refreshList(); });
    connect(m_list, &QListWidget::itemActivated,
            this, [this](QListWidgetItem* item) {
                emit fileChosen(item->data(Qt::UserRole).toString());
                accept();
            });

    m_input->installEventFilter(this);
    refreshList();
}

bool QuickOpenDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Down) {
            m_list->setFocus();
            m_list->setCurrentRow(qMax(0, m_list->currentRow()));
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            QListWidgetItem* item = m_list->currentItem();
            if (!item && m_list->count() > 0)
                item = m_list->item(0);
            if (item) {
                emit fileChosen(item->data(Qt::UserRole).toString());
                accept();
            }
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void QuickOpenDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Down && m_list->count() > 0) {
        m_list->setFocus();
        if (m_list->currentRow() < 0)
            m_list->setCurrentRow(0);
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QListWidgetItem* item = m_list->currentItem();
        if (m_list->currentRow() < 0 && m_list->count() > 0)
            item = m_list->item(0);
        if (item) {
            emit fileChosen(item->data(Qt::UserRole).toString());
            accept();
        }
        return;
    }
    QDialog::keyPressEvent(event);
}

void QuickOpenDialog::refreshList()
{
    m_list->clear();
    const QString needle = m_input->text().trimmed();

    int added = 0;
    for (const QString& path : m_files) {
        if (!needle.isEmpty() && !path.contains(needle, Qt::CaseInsensitive))
            continue;
        auto* item = new QListWidgetItem(path, m_list);
        item->setData(Qt::UserRole, path);
        if (++added >= 500)
            break;
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

} // namespace app
