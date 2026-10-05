// Hutaomu Editor - Settings dialog.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace app {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    QString theme() const;
    QString fontFamily() const; // "" = 默认
    int fontSize() const;
    int tabWidth() const;
    bool wordWrap() const;
    QString markdownViewMode() const;
    QString sidebarPosition() const;  // left / right
    QString previewPosition() const;  // right / bottom

signals:
    // 选中主题时立即请求预览（不写设置）；调用方负责在取消时回滚。
    void themePreviewRequested(const QString& themeId);

private:
    void rebuildThemeList(const QString& selectId = QString());
    void exportCurrentTheme();
    void rebuildPluginList();
    void importPluginPackage();
    void toggleSelectedPlugin(bool enabled);
    void uninstallSelectedPlugin();
    void importThemePackage();
    void removeSelectedTheme();
    void updateThemeHints();

    QListWidget* m_themeList = nullptr;
    QPushButton* m_importTheme = nullptr;
    QPushButton* m_removeTheme = nullptr;
    QPushButton* m_exportTheme = nullptr;
    QLabel* m_themeHint = nullptr;   // 主题形态/能力提示
    QLabel* m_fontHint = nullptr;    // "当前主题指定了字体/字号"
    QStringList m_installedIds;      // 可删除（非内置）的主题 id

    QComboBox* m_fontCombo = nullptr;
    QSpinBox* m_fontSize = nullptr;
    QSpinBox* m_tabWidth = nullptr;
    QCheckBox* m_wordWrap = nullptr;
    QComboBox* m_mdModeCombo = nullptr;
    QComboBox* m_sidebarPosCombo = nullptr;
    QComboBox* m_previewPosCombo = nullptr;

    QListWidget* m_pluginList = nullptr;
    QPushButton* m_importPlugin = nullptr;
    QPushButton* m_uninstallPlugin = nullptr;
    QLabel* m_pluginHint = nullptr;
};

} // namespace app
