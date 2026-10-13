// Hutaomu Editor - 语言模式选择器（VS Code "Select Language Mode" 同款）。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QListWidget;

namespace app {

// 可搜索的语言模式列表：名称 + 标识，回车/双击选定。
class LanguagePicker : public QDialog {
    Q_OBJECT
public:
    explicit LanguagePicker(const QString& currentLanguageId,
                            QWidget* parent = nullptr);

    QString selectedLanguageId() const { return m_selectedId; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void refreshList();
    void activateCurrent();

    QString m_currentId;
    QString m_selectedId;
    QLineEdit* m_input = nullptr;
    QListWidget* m_list = nullptr;
};

} // namespace app
