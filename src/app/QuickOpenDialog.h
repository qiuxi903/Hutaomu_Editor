// Hutaomu Editor - Quick file open (Ctrl+P).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QDialog>

class QLineEdit;
class QListWidget;

namespace app {

class QuickOpenDialog : public QDialog {
    Q_OBJECT
public:
    explicit QuickOpenDialog(const QStringList& files, QWidget* parent = nullptr);

signals:
    void fileChosen(const QString& filePath);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void refreshList();

    QStringList m_files;
    QLineEdit* m_input = nullptr;
    QListWidget* m_list = nullptr;
};

} // namespace app
