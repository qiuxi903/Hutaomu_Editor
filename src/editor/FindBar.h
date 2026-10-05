// Hutaomu Editor - In-editor find & replace bar.
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QFrame>
#include <QList>
#include <QTextEdit>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

namespace editor {

// 浮动在编辑器右上角的查找/替换条。支持大小写、全词、正则，
// 全文档匹配计数与高亮，Esc 关闭。
class FindBar : public QFrame {
    Q_OBJECT
public:
    explicit FindBar(QPlainTextEdit* editor);

    void open(bool withReplace);
    void closeBar();
    void reposition();
    QList<QTextEdit::ExtraSelection> searchSelections() const;
    bool eventFilter(QObject* watched, QEvent* event) override;

public slots:
    void findNext();
    void findPrevious();

signals:
    void searchSelectionsChanged();

private:
    enum class Mode { Find, Regex };

    void buildUi();
    void doFind(bool backward);
    void updateAllMatches();
    void replaceCurrent();
    void replaceAll();
    QTextCursor locate(bool backward) const;
    Mode mode() const;

    QPlainTextEdit* m_editor = nullptr;

    QLineEdit* m_findEdit = nullptr;
    QLineEdit* m_replaceEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QCheckBox* m_caseBox = nullptr;
    QCheckBox* m_wordBox = nullptr;
    QCheckBox* m_regexBox = nullptr;
    QPushButton* m_replaceOneButton = nullptr;
    QPushButton* m_replaceAllButton = nullptr;
    QWidget* m_replaceRow = nullptr;
    QToolButton* m_prevButton = nullptr;
    QToolButton* m_nextButton = nullptr;
    QList<QTextEdit::ExtraSelection> m_searchSelections;
};

} // namespace editor
