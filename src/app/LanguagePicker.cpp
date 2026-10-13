// Hutaomu Editor - 语言模式选择器实现。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "LanguagePicker.h"

#include "syntax/LanguageRegistry.h"
#include "syntax/LanguageSpec.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace app {

LanguagePicker::LanguagePicker(const QString& currentLanguageId,
                               QWidget* parent)
    : QDialog(parent)
    , m_currentId(currentLanguageId)
{
    setObjectName(QStringLiteral("languagePicker"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowTitle(tr("选择语言模式"));
    setMinimumWidth(560);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("选择语言模式…（例如：C++ / python / markdown）"));
    m_input->installEventFilter(this);
    layout->addWidget(m_input);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("commandList"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setMinimumHeight(360);
    layout->addWidget(m_list);

    refreshList();

    connect(m_input, &QLineEdit::textChanged, this, [this] { refreshList(); });
    connect(m_list, &QListWidget::itemActivated, this,
            [this](QListWidgetItem*) { activateCurrent(); });
    connect(m_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem*) { activateCurrent(); });

    m_input->setFocus();
}

bool LanguagePicker::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) {
            QApplication::sendEvent(m_list, event);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void LanguagePicker::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    QDialog::keyPressEvent(event);
}

void LanguagePicker::refreshList()
{
    const QString needle = m_input->text().trimmed();
    m_list->clear();

    const auto languages = syntax::LanguageRegistry::instance().languages();
    QList<const syntax::LanguageSpec*> sorted(languages.cbegin(), languages.cend());
    std::sort(sorted.begin(), sorted.end(),
              [](const syntax::LanguageSpec* a, const syntax::LanguageSpec* b) {
                  return a->displayName.compare(b->displayName,
                                                Qt::CaseInsensitive) < 0;
              });

    int currentRow = -1;
    for (const syntax::LanguageSpec* spec : sorted) {
        if (!needle.isEmpty()
            && !spec->displayName.contains(needle, Qt::CaseInsensitive)
            && !spec->id.contains(needle, Qt::CaseInsensitive))
            continue;
        auto* item = new QListWidgetItem(m_list);
        const QString marker = spec->hasHighlighting()
            ? QString()
            : QStringLiteral("   ·   纯文本");
        item->setText(QStringLiteral("%1  (%2)%3")
                          .arg(spec->displayName, spec->id, marker));
        item->setToolTip(spec->extensions.isEmpty()
            ? tr("无关联扩展名")
            : tr("关联扩展名：%1").arg(
                  QStringList(spec->extensions.cbegin(), spec->extensions.cend())
                      .join(QLatin1String("  ."))));
        item->setData(Qt::UserRole, spec->id);
        if (spec->id == m_currentId) {
            item->setText(QStringLiteral("%1  (%2)  —  %3")
                              .arg(spec->displayName, spec->id, tr("当前")));
            currentRow = m_list->count() - 1;
        }
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(currentRow >= 0 ? currentRow : 0);
}

void LanguagePicker::activateCurrent()
{
    auto* item = m_list->currentItem();
    if (!item)
        return;
    m_selectedId = item->data(Qt::UserRole).toString();
    accept();
}

} // namespace app
