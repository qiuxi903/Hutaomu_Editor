// Hutaomu Editor - Quick file open (Ctrl+P).
// SPDX-License-Identifier: LicenseRef-Proprietary
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
