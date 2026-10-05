// Hutaomu Editor - In-editor find & replace bar.
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "FindBar.h"

#include <QPlainTextEdit>
#include "themes/ThemeManager.h"
#include <QCheckBox>
#include <QEvent>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

namespace editor {

FindBar::FindBar(QPlainTextEdit* editor)
    : QFrame(editor)
    , m_editor(editor)
{
    setObjectName(QStringLiteral("findBar"));
    setFrameShape(QFrame::StyledPanel);
    buildUi();
    hide();
}

void FindBar::buildUi()
{
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(4);

    m_findEdit = new QLineEdit(this);
    m_findEdit->setObjectName(QStringLiteral("findEdit"));
    m_findEdit->setPlaceholderText(tr("查找"));
    m_findEdit->setClearButtonEnabled(true);
    layout->addWidget(m_findEdit, 0, 0);

    m_prevButton = new QToolButton(this);
    m_prevButton->setText(QStringLiteral("↑"));
    m_prevButton->setToolTip(tr("上一个 (Shift+Enter)"));
    layout->addWidget(m_prevButton, 0, 1);

    m_nextButton = new QToolButton(this);
    m_nextButton->setText(QStringLiteral("↓"));
    m_nextButton->setToolTip(tr("下一个 (Enter)"));
    layout->addWidget(m_nextButton, 0, 2);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("findStatus"));
    layout->addWidget(m_statusLabel, 0, 3);

    auto* closeButton = new QToolButton(this);
    closeButton->setText(QStringLiteral("✕"));
    closeButton->setToolTip(tr("关闭 (Esc)"));
    layout->addWidget(closeButton, 0, 4);

    m_caseBox = new QCheckBox(tr("区分大小写"), this);
    m_wordBox = new QCheckBox(tr("全词"), this);
    m_regexBox = new QCheckBox(tr("正则"), this);
    layout->addWidget(m_caseBox, 1, 0);
    auto* optsRow = new QWidget(this);
    auto* optsLayout = new QHBoxLayout(optsRow);
    optsLayout->setContentsMargins(0, 0, 0, 0);
    optsLayout->setSpacing(8);
    optsLayout->addWidget(m_wordBox);
    optsLayout->addWidget(m_regexBox);
    optsLayout->addStretch();
    layout->addWidget(optsRow, 1, 1, 1, 3);

    m_replaceRow = new QWidget(this);
    auto* replaceLayout = new QHBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(6);
    m_replaceEdit = new QLineEdit(m_replaceRow);
    m_replaceEdit->setPlaceholderText(tr("替换为"));
    replaceLayout->addWidget(m_replaceEdit);
    m_replaceOneButton = new QPushButton(tr("替换"), m_replaceRow);
    m_replaceAllButton = new QPushButton(tr("全部替换"), m_replaceRow);
    replaceLayout->addWidget(m_replaceOneButton);
    replaceLayout->addWidget(m_replaceAllButton);
    layout->addWidget(m_replaceRow, 2, 0, 1, 5);

    connect(m_findEdit, &QLineEdit::textChanged,
            this, [this] { updateAllMatches(); doFind(false); });
    connect(m_nextButton, &QToolButton::clicked, this, &FindBar::findNext);
    connect(m_prevButton, &QToolButton::clicked, this, &FindBar::findPrevious);
    connect(closeButton, &QToolButton::clicked, this, &FindBar::closeBar);
    connect(m_replaceOneButton, &QPushButton::clicked, this, &FindBar::replaceCurrent);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &FindBar::replaceAll);
    for (QCheckBox* box : { m_caseBox, m_wordBox, m_regexBox })
        connect(box, &QCheckBox::toggled, this, &FindBar::updateAllMatches);
    connect(m_editor->document(), &QTextDocument::contentsChange,
            this, [this] { if (isVisible()) updateAllMatches(); });

    m_findEdit->installEventFilter(this);
    m_replaceEdit->installEventFilter(this);
}

bool FindBar::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress
        && (watched == m_findEdit || watched == m_replaceEdit)) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (key->modifiers() & Qt::ShiftModifier)
                findPrevious();
            else
                findNext();
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            closeBar();
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

void FindBar::open(bool withReplace)
{
    show();
    m_replaceRow->setVisible(withReplace);
    adjustSize();
    reposition();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
    updateAllMatches();
}

void FindBar::reposition()
{
    const int margin = 12;
    const int x = qMax(margin, m_editor->viewport()->width() - width() - margin);
    move(x, 8);
}

void FindBar::closeBar()
{
    hide();
    m_editor->setFocus();
    emit searchSelectionsChanged();
}

FindBar::Mode FindBar::mode() const
{
    return m_regexBox->isChecked() ? Mode::Regex : Mode::Find;
}

QTextCursor FindBar::locate(bool backward) const
{
    const QString pattern = m_findEdit->text();
    if (pattern.isEmpty())
        return QTextCursor();

    QTextDocument::FindFlags flags;
    if (backward)
        flags |= QTextDocument::FindBackward;
    if (m_caseBox->isChecked())
        flags |= QTextDocument::FindCaseSensitively;
    if (m_wordBox->isChecked())
        flags |= QTextDocument::FindWholeWords;

    QTextDocument* doc = m_editor->document();
    const QTextCursor from = m_editor->textCursor();

    if (mode() == Mode::Regex) {
        QRegularExpression re(pattern,
                              m_caseBox->isChecked()
                                  ? QRegularExpression::NoPatternOption
                                  : QRegularExpression::CaseInsensitiveOption);
        if (!re.isValid())
            return QTextCursor();
        return doc->find(re, from, flags);
    }

    return doc->find(pattern, from, flags);
}

void FindBar::doFind(bool backward)
{
    if (m_findEdit->text().isEmpty()) {
        m_statusLabel->clear();
        return;
    }

    QTextCursor found = locate(backward);
    if (found.isNull()) {
        // 环绕：从文档头/尾再找一次
        QTextCursor wrapCursor = m_editor->textCursor();
        wrapCursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        m_editor->setTextCursor(wrapCursor);
        found = locate(backward);
        if (found.isNull()) {
            m_statusLabel->setText(tr("无结果"));
            return;
        }
    }

    m_editor->setTextCursor(found);
}

void FindBar::findNext()
{
    if (!m_findEdit->text().isEmpty()) {
        // 若当前光标已在命中上，先移过它再向前找
        QTextCursor c = m_editor->textCursor();
        if (c.hasSelection())
            c.setPosition(c.selectionEnd());
        m_editor->setTextCursor(c);
        doFind(false);
    }
}

void FindBar::findPrevious()
{
    doFind(true);
}

QList<QTextEdit::ExtraSelection> FindBar::searchSelections() const
{
    return m_searchSelections;
}

void FindBar::updateAllMatches()
{
    m_searchSelections.clear();

    const QString pattern = m_findEdit->text();
    int count = 0;

    if (!pattern.isEmpty() && isVisible()) {
        QTextDocument* doc = m_editor->document();
        QTextDocument::FindFlags flags;
        if (m_caseBox->isChecked())
            flags |= QTextDocument::FindCaseSensitively;
        if (m_wordBox->isChecked())
            flags |= QTextDocument::FindWholeWords;

        QRegularExpression re;
        if (mode() == Mode::Regex) {
            re.setPattern(pattern);
            re.setPatternOptions(m_caseBox->isChecked()
                                     ? QRegularExpression::NoPatternOption
                                     : QRegularExpression::CaseInsensitiveOption);
            if (!re.isValid()) {
                m_statusLabel->setText(tr("正则表达式无效"));
                emit searchSelectionsChanged();
                return;
            }
        }

        QTextCursor cursor(doc);
        while (!cursor.isNull()) {
            cursor = (mode() == Mode::Regex)
                         ? doc->find(re, cursor, flags)
                         : doc->find(pattern, cursor, flags);
            if (cursor.isNull())
                break;

            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format.setBackground(editor::ThemeManager::tokens().searchHighlightBackground);
            m_searchSelections.append(selection);
            if (++count >= 2000)
                break; // 超大文档上限保护
        }
    }

    m_statusLabel->setText(count > 0
                               ? tr("%1 个匹配").arg(count)
                               : (pattern.isEmpty() ? QString() : tr("无结果")));
    emit searchSelectionsChanged();
}

void FindBar::replaceCurrent()
{
    if (m_findEdit->text().isEmpty())
        return;
    QTextCursor cursor = m_editor->textCursor();
    if (!cursor.hasSelection()) {
        findNext();
        return;
    }

    QString replacement = m_replaceEdit->text();
    if (mode() == Mode::Regex) {
        QRegularExpression re(m_findEdit->text(),
                              m_caseBox->isChecked()
                                  ? QRegularExpression::NoPatternOption
                                  : QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = re.match(cursor.selectedText());
        if (m.hasMatch()) {
            replacement = m.captured(0);
            for (int g = 1; g <= 9; ++g)
                replacement.replace(QStringLiteral("$%1").arg(g), m.captured(g));
        }
    }

    cursor.insertText(replacement);
    findNext();
}

void FindBar::replaceAll()
{
    const QString pattern = m_findEdit->text();
    if (pattern.isEmpty())
        return;

    QTextDocument* doc = m_editor->document();
    int replacements = 0;

    QTextCursor editBlock(m_editor->document());
    editBlock.beginEditBlock();

    QTextCursor cursor(doc);
    QRegularExpression re;
    if (mode() == Mode::Regex) {
        re.setPattern(pattern);
        re.setPatternOptions(m_caseBox->isChecked()
                                 ? QRegularExpression::NoPatternOption
                                 : QRegularExpression::CaseInsensitiveOption);
        if (!re.isValid()) {
            editBlock.endEditBlock();
            m_statusLabel->setText(tr("正则表达式无效"));
            return;
        }
    }

    while (true) {
        cursor = (mode() == Mode::Regex)
                     ? doc->find(re, cursor)
                     : doc->find(pattern, cursor,
                                 m_caseBox->isChecked() ? QTextDocument::FindCaseSensitively
                                                        : QTextDocument::FindFlags(0));
        if (cursor.isNull())
            break;

        QString replacement = m_replaceEdit->text();
        if (mode() == Mode::Regex) {
            const QRegularExpressionMatch m = re.match(cursor.selectedText());
            if (m.hasMatch()) {
                replacement = m.captured(0);
                for (int g = 1; g <= 9; ++g)
                    replacement.replace(QStringLiteral("$%1").arg(g), m.captured(g));
            }
        }

        cursor.insertText(replacement);
        ++replacements;
        if (replacements >= 100000)
            break;
    }

    editBlock.endEditBlock();
    m_statusLabel->setText(tr("已替换 %1 处").arg(replacements));
    updateAllMatches();
}

} // namespace editor
