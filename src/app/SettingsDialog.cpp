// Hutaomu Editor - Settings dialog.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QUrl>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "plugins/PluginManager.h"
#include "settings/AppSettings.h"
#include "themes/ThemeManager.h"
#include "themes/ThemePackage.h"

namespace app {

namespace {

constexpr const char* kDefaultThemeId = "paper";

// 预览图：优先用主题包内 preview.png；没有就按主题配色画一个缩略示意。
QIcon themePreviewIcon(const editor::ThemeDefinition& theme)
{
    if (!theme.previewPath.isEmpty()) {
        const QIcon icon(theme.previewPath);
        if (!icon.isNull())
            return icon;
    }
    QPixmap pixmap(120, 75);
    pixmap.fill(theme.color(QStringLiteral("editorBg")));
    QPainter painter(&pixmap);
    painter.fillRect(0, 0, 120, 9, theme.color(QStringLiteral("titleBarBg")));
    painter.fillRect(0, 9, 26, 66, theme.color(QStringLiteral("sidebarBg")));
    painter.fillRect(30, 16, 40, 5, theme.color(QStringLiteral("editorFg")));
    painter.fillRect(30, 26, 80, 3, theme.color(QStringLiteral("editorFg")));
    painter.fillRect(30, 33, 70, 3, theme.color(QStringLiteral("editorFg")));
    painter.fillRect(30, 40, 60, 3, theme.color(QStringLiteral("accent")));
    painter.fillRect(30, 50, 34, 6, theme.color(QStringLiteral("accent")));
    painter.end();
    return QIcon(pixmap);
}

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(tr("设置"));
    setMinimumWidth(520);

    const auto& s = settings::AppSettings::instance();

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    form->setSpacing(10);

    // ---- 主题画廊：预览图列表 + 导入/删除主题包 ----
    auto* themeArea = new QWidget(this);
    auto* themeLayout = new QVBoxLayout(themeArea);
    themeLayout->setContentsMargins(0, 0, 0, 0);
    themeLayout->setSpacing(6);

    m_themeList = new QListWidget(themeArea);
    m_themeList->setObjectName(QStringLiteral("themeGallery"));
    m_themeList->setViewMode(QListView::IconMode);
    m_themeList->setIconSize(QSize(136, 85));
    m_themeList->setGridSize(QSize(152, 118));
    m_themeList->setResizeMode(QListView::Adjust);
    m_themeList->setMovement(QListView::Static);
    m_themeList->setWordWrap(true);
    m_themeList->setUniformItemSizes(true);
    m_themeList->setSpacing(2);
    m_themeList->setMaximumHeight(256);
    themeLayout->addWidget(m_themeList);

    auto* themeButtons = new QHBoxLayout;
    m_importTheme = new QPushButton(tr("导入主题包…"), themeArea);
    m_importTheme->setObjectName(QStringLiteral("themeImportButton"));
    m_removeTheme = new QPushButton(tr("删除主题"), themeArea);
    m_removeTheme->setObjectName(QStringLiteral("themeRemoveButton"));
    m_exportTheme = new QPushButton(tr("导出主题包…"), themeArea);
    m_exportTheme->setObjectName(QStringLiteral("themeExportButton"));
    themeButtons->addWidget(m_importTheme);
    themeButtons->addWidget(m_exportTheme);
    themeButtons->addWidget(m_removeTheme);
    themeButtons->addStretch(1);
    themeLayout->addLayout(themeButtons);

    m_themeHint = new QLabel(themeArea);
    m_themeHint->setObjectName(QStringLiteral("themeHint"));
    m_themeHint->setWordWrap(true);
    themeLayout->addWidget(m_themeHint);

    form->addRow(tr("主题"), themeArea);

    m_fontCombo = new QComboBox(this);
    m_fontCombo->addItem(tr("默认（Consolas / 等宽）"), QString());
    m_fontCombo->insertSeparator(m_fontCombo->count());
    const QStringList families = QFontDatabase::families();
    for (const QString& family : families) {
        if (QFontDatabase::isFixedPitch(family)) {
            m_fontCombo->addItem(family, family);
            if (family == s.fontFamily)
                m_fontCombo->setCurrentIndex(m_fontCombo->count() - 1);
        }
    }
    form->addRow(tr("编辑器字体"), m_fontCombo);

    m_fontSize = new QSpinBox(this);
    m_fontSize->setRange(8, 40);
    m_fontSize->setValue(s.fontSize);
    form->addRow(tr("编辑器字号"), m_fontSize);

    m_fontHint = new QLabel(this);
    m_fontHint->setObjectName(QStringLiteral("themeFontHint"));
    m_fontHint->setWordWrap(true);
    form->addRow(QString(), m_fontHint);

    m_tabWidth = new QSpinBox(this);
    m_tabWidth->setRange(1, 16);
    m_tabWidth->setValue(s.tabWidth);
    form->addRow(tr("制表符宽度"), m_tabWidth);

    m_wordWrap = new QCheckBox(tr("启用自动换行"), this);
    m_wordWrap->setChecked(s.wordWrap);
    form->addRow(QString(), m_wordWrap);

    m_mdModeCombo = new QComboBox(this);
    m_mdModeCombo->addItem(tr("实时预览"), QStringLiteral("live"));
    m_mdModeCombo->addItem(tr("分栏预览"), QStringLiteral("split"));
    m_mdModeCombo->addItem(tr("源码模式"), QStringLiteral("source"));
    m_mdModeCombo->setCurrentIndex(m_mdModeCombo->findData(s.markdownViewMode));
    form->addRow(tr("Markdown 视图"), m_mdModeCombo);

    // ---- 布局：各面板位置（也可直接拖动面板头部换边） ----
    m_sidebarPosCombo = new QComboBox(this);
    m_sidebarPosCombo->addItem(tr("左侧"), QStringLiteral("left"));
    m_sidebarPosCombo->addItem(tr("右侧"), QStringLiteral("right"));
    m_sidebarPosCombo->setCurrentIndex(m_sidebarPosCombo->findData(s.sidebarPosition));
    form->addRow(tr("侧边栏（文件树/搜索）"), m_sidebarPosCombo);


    m_previewPosCombo = new QComboBox(this);
    m_previewPosCombo->addItem(tr("编辑器右侧"), QStringLiteral("right"));
    m_previewPosCombo->addItem(tr("编辑器底部"), QStringLiteral("bottom"));
    m_previewPosCombo->setCurrentIndex(m_previewPosCombo->findData(s.previewPosition));
    form->addRow(tr("分栏预览"), m_previewPosCombo);

    // ---- 插件（L1 声明式） ----
    auto* pluginArea = new QWidget(this);
    auto* pluginLayout = new QVBoxLayout(pluginArea);
    pluginLayout->setContentsMargins(0, 0, 0, 0);
    pluginLayout->setSpacing(6);

    m_pluginList = new QListWidget(pluginArea);
    m_pluginList->setObjectName(QStringLiteral("pluginList"));
    m_pluginList->setMaximumHeight(130);
    pluginLayout->addWidget(m_pluginList);

    auto* pluginButtons = new QHBoxLayout;
    m_importPlugin = new QPushButton(tr("导入插件包…"), pluginArea);
    m_importPlugin->setObjectName(QStringLiteral("pluginImportButton"));
    m_uninstallPlugin = new QPushButton(tr("卸载插件"), pluginArea);
    m_uninstallPlugin->setObjectName(QStringLiteral("pluginUninstallButton"));
    pluginButtons->addWidget(m_importPlugin);
    pluginButtons->addWidget(m_uninstallPlugin);
    pluginButtons->addStretch(1);
    pluginLayout->addLayout(pluginButtons);

    m_pluginHint = new QLabel(pluginArea);
    m_pluginHint->setObjectName(QStringLiteral("pluginHint"));
    m_pluginHint->setWordWrap(true);
    pluginLayout->addWidget(m_pluginHint);

    form->addRow(tr("插件"), pluginArea);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    rebuildThemeList(s.theme);
    connect(m_themeList, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem*, QListWidgetItem*) {
                updateThemeHints();
                emit themePreviewRequested(theme()); // 实时预览
            });
    connect(m_importTheme, &QPushButton::clicked, this, &SettingsDialog::importThemePackage);
    connect(m_exportTheme, &QPushButton::clicked, this, &SettingsDialog::exportCurrentTheme);
    connect(m_removeTheme, &QPushButton::clicked, this, &SettingsDialog::removeSelectedTheme);
    updateThemeHints();

    rebuildPluginList();
    connect(m_pluginList, &QListWidget::itemChanged, this,
            [this](QListWidgetItem* item) {
                toggleSelectedPlugin(item->checkState() == Qt::Checked);
            });
    connect(m_importPlugin, &QPushButton::clicked, this,
            &SettingsDialog::importPluginPackage);
    connect(m_uninstallPlugin, &QPushButton::clicked, this,
            &SettingsDialog::uninstallSelectedPlugin);
}

// 导出当前选中的主题为 .htmtpi（内置主题也能导出，便于二次创作）
void SettingsDialog::exportCurrentTheme()
{
    const QString id = theme();
    const editor::ThemeDefinition definition = editor::ThemeManager::themeDefinition(id);
    const QString suggested = QDir(QDir::homePath()).filePath(
        (definition.name.isEmpty() ? id : definition.name) + QStringLiteral(".htmtpi"));
    const QString path = QFileDialog::getSaveFileName(
        this, tr("导出主题包"), suggested, tr("Hutaomu 主题包 (*.htmtpi)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!editor::ThemeManager::exportThemePackage(id, path, &error)) {
        QMessageBox::warning(this, tr("导出主题包"),
                             error.isEmpty() ? tr("导出失败。") : error);
        return;
    }
    QMessageBox::information(this, tr("导出主题包"),
                             tr("已导出到：\n%1\n\n可以直接分享，或用"
                                "「导入主题包…」在别的机器上安装。")
                                 .arg(path));
}

namespace {

// 安装确认页：展示插件身份、贡献点与申请的权限（对标 VS Code 的权限提示）
bool confirmPluginInstall(QWidget* parent, const plugins::PluginManifest& manifest)
{
    QStringList contributions;
    if (!manifest.commands.isEmpty())
        contributions.append(QObject::tr("%1 个命令").arg(manifest.commands.size()));
    if (!manifest.formats.isEmpty()) {
        QStringList extensions;
        for (const plugins::PluginFormat& format : manifest.formats)
            extensions.append(QStringLiteral("*.") + format.extension);
        contributions.append(
            QObject::tr("文件格式：%1").arg(extensions.join(QStringLiteral("、"))));
    }
    if (!manifest.themePaths.isEmpty())
        contributions.append(QObject::tr("%1 套主题").arg(manifest.themePaths.size()));
    if (contributions.isEmpty())
        contributions.append(QObject::tr("（无可识别的贡献点）"));

    QStringList permissions;
    if (manifest.declaresShellPermission())
        permissions.append(QObject::tr("执行外部程序（每次首次执行前会再确认）"));
    if (manifest.permissions.contains(QStringLiteral("fs.read")))
        permissions.append(QObject::tr("读取文件（脚本插件阶段才会启用）"));
    if (manifest.permissions.contains(QStringLiteral("fs.write")))
        permissions.append(QObject::tr("写入文件（脚本插件阶段才会启用）"));
    if (manifest.permissions.contains(QStringLiteral("net")))
        permissions.append(QObject::tr("访问网络（脚本插件阶段才会启用）"));
    if (permissions.isEmpty())
        permissions.append(QObject::tr("无（纯声明式，不含可执行代码）"));

    QMessageBox box(parent);
    box.setWindowTitle(QObject::tr("安装插件"));
    box.setIcon(QMessageBox::Question);
    box.setText(QObject::tr("安装 %1 ？").arg(manifest.name));
    box.setInformativeText(QObject::tr(
        "标识：%1\n版本：%2\n发布者：%3\n许可证：%4\n\n"
        "将添加：\n· %5\n\n申请的权限：\n· %6\n\n"
        "%7")
        .arg(manifest.id, manifest.version, manifest.publisher,
             manifest.license.isEmpty() ? QObject::tr("未声明") : manifest.license,
             contributions.join(QStringLiteral("\n· ")),
             permissions.join(QStringLiteral("\n· ")),
             manifest.usageHint()));
    QPushButton* install = box.addButton(QObject::tr("安装"), QMessageBox::AcceptRole);
    box.addButton(QObject::tr("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(install);
    box.exec();
    return box.clickedButton() == install;
}

} // namespace

void SettingsDialog::rebuildPluginList()
{
    if (!m_pluginList)
        return;
    m_pluginList->blockSignals(true);
    m_pluginList->clear();
    const auto all = plugins::PluginManager::instance().plugins();
    for (const plugins::Plugin& plugin : all) {
        QStringList bits;
        bits.append(QStringLiteral("v%1").arg(plugin.manifest.version));
        if (!plugin.manifest.publisher.isEmpty())
            bits.append(plugin.manifest.publisher);
        if (!plugin.manifest.commands.isEmpty())
            bits.append(tr("%1 命令").arg(plugin.manifest.commands.size()));
        if (!plugin.manifest.formats.isEmpty())
            bits.append(tr("%1 格式").arg(plugin.manifest.formats.size()));
        if (!plugin.manifest.themePaths.isEmpty())
            bits.append(tr("%1 主题").arg(plugin.manifest.themePaths.size()));
        const QString badge = plugin.builtin ? tr("预装 · ") : QString();
        auto* item = new QListWidgetItem(
            QStringLiteral("%1%2  ·  %3")
                .arg(badge, plugin.manifest.name,
                     bits.join(QStringLiteral(" · "))),
            m_pluginList);
        item->setData(Qt::UserRole, plugin.manifest.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(plugin.enabled ? Qt::Checked : Qt::Unchecked);
        if (plugin.builtin)
            item->setToolTip(tr("随程序预装的插件：可以禁用，不能卸载"));
        else if (plugin.manifest.usesShell() && plugin.manifest.declaresShellPermission())
            item->setToolTip(tr("包含外部命令；首次执行时会弹窗确认"));
        if (!plugin.manifest.description.isEmpty())
            item->setToolTip(plugin.manifest.description);
    }
    m_pluginList->blockSignals(false);

    m_pluginHint->setText(
        all.isEmpty()
            ? tr("还没有插件。插件是声明式包（.htmed），可以带来主题、文件格式与命令；"
                 "不含可执行代码。")
            : tr("勾选启用 / 取消勾选禁用。导入插件包会先展示它申请的权限。"));
    m_uninstallPlugin->setEnabled(m_pluginList->currentItem() != nullptr);
}

void SettingsDialog::toggleSelectedPlugin(bool enabled)
{
    auto* item = m_pluginList->currentItem();
    if (!item)
        return;
    m_uninstallPlugin->setEnabled(
        !plugins::PluginManager::instance().isBuiltin(
            item->data(Qt::UserRole).toString()));
    const QString id = item->data(Qt::UserRole).toString();
    QString error;
    if (!plugins::PluginManager::instance().setEnabled(id, enabled, &error))
        QMessageBox::warning(this, tr("插件"), error);
    rebuildPluginList();
}

void SettingsDialog::importPluginPackage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("导入插件包"), QDir::homePath(),
        tr("Hutaomu 插件包 (*.htmed *.zip)"));
    if (path.isEmpty())
        return;

    QString error;
    const plugins::PluginManifest manifest
        = plugins::PluginManager::peekManifest(path, &error);
    if (!manifest.isValid()) {
        QMessageBox::warning(this, tr("导入插件包"),
                             error.isEmpty() ? tr("插件包无效。") : error);
        return;
    }
    if (auto current = plugins::PluginManager::instance().plugin(manifest.id);
        !current.manifest.id.isEmpty()) {
        const auto answer = QMessageBox::question(
            this, tr("导入插件包"),
            tr("已安装插件「%1」（v%2），是否覆盖更新？")
                .arg(current.manifest.name, current.manifest.version));
        if (answer != QMessageBox::Yes)
            return;
    }
    if (!confirmPluginInstall(this, manifest))
        return;

    const auto outcome = plugins::PluginManager::instance().installPackage(path);
    if (!outcome.ok) {
        QMessageBox::warning(this, tr("导入插件包"),
                             outcome.error.isEmpty() ? tr("安装失败。") : outcome.error);
        return;
    }
    rebuildPluginList();
}

void SettingsDialog::uninstallSelectedPlugin()
{
    auto* item = m_pluginList->currentItem();
    if (!item)
        return;
    const QString id = item->data(Qt::UserRole).toString();
    const plugins::Plugin plugin = plugins::PluginManager::instance().plugin(id);
    if (plugin.builtin) {
        QMessageBox::information(this, tr("卸载插件"),
                                 tr("「%1」是随程序预装的插件，不能卸载。"
                                    "如果不想用它，取消勾选即可禁用。")
                                     .arg(plugin.manifest.name));
        return;
    }
    const auto answer = QMessageBox::question(
        this, tr("卸载插件"),
        tr("确定卸载「%1」？它带来的主题/格式/命令会一并消失。")
            .arg(plugin.manifest.name));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!plugins::PluginManager::instance().uninstall(id, &error))
        QMessageBox::warning(this, tr("卸载插件"),
                             error.isEmpty() ? tr("卸载失败。") : error);
    rebuildPluginList();
}

void SettingsDialog::rebuildThemeList(const QString& selectId)
{
    m_themeList->clear();
    m_installedIds.clear();
    for (const editor::ThemeDefinition& theme : editor::ThemeManager::availableThemes()) {
        auto* item = new QListWidgetItem(themePreviewIcon(theme), theme.name, m_themeList);
        item->setData(Qt::UserRole, theme.id);
        item->setToolTip(theme.description.isEmpty()
                             ? theme.id
                             : theme.description + QLatin1Char('\n') + theme.id);
        if (!theme.builtin)
            m_installedIds.append(theme.id);
    }
    const QString wanted = selectId.isEmpty() ? settings::AppSettings::instance().theme
                                              : selectId;
    for (int i = 0; i < m_themeList->count(); ++i) {
        if (m_themeList->item(i)->data(Qt::UserRole).toString() == wanted) {
            m_themeList->setCurrentRow(i);
            break;
        }
    }
    if (!m_themeList->currentItem() && m_themeList->count() > 0)
        m_themeList->setCurrentRow(0);
}

void SettingsDialog::updateThemeHints()
{
    const QString id = theme();
    const editor::ThemeDefinition theme = editor::ThemeManager::themeDefinition(id);

    QStringList capabilities;
    if (theme.capabilities.contains(QStringLiteral("colors")))
        capabilities.append(tr("配色"));
    if (theme.capabilities.contains(QStringLiteral("metrics")))
        capabilities.append(tr("排版度量"));
    if (theme.capabilities.contains(QStringLiteral("assets")))
        capabilities.append(tr("图标与背景"));
    if (theme.capabilities.contains(QStringLiteral("layout")))
        capabilities.append(tr("界面布局"));

    QString hint = theme.description;
    if (!capabilities.isEmpty()) {
        if (!hint.isEmpty())
            hint += QLatin1Char('\n');
        hint += tr("能力：%1").arg(capabilities.join(QStringLiteral(" · ")));
    }
    if (theme.capabilities.contains(QStringLiteral("layout"))) {
        if (!hint.isEmpty())
            hint += QLatin1Char('\n');
        const QString summary = theme.layout.summary();
        hint += summary.isEmpty()
                    ? tr("该主题包含界面布局设置。")
                    : tr("界面布局：%1。").arg(summary);
    }
    m_themeHint->setText(hint);

    // 主题指定字体/字号时提示用户：它会覆盖上面的设置（T2 行为）。
    const QString families = theme.metric(QStringLiteral("editorFontFamily"));
    const int size = theme.metric(QStringLiteral("editorFontSize")).toInt();
    if (!families.isEmpty() || size > 0) {
        QStringList overrides;
        if (!families.isEmpty())
            overrides.append(tr("字体 → %1").arg(families));
        if (size > 0)
            overrides.append(tr("字号 → %1pt").arg(size));
        m_fontHint->setText(tr("当前主题指定了编辑器%1，将覆盖上方设置。")
                                .arg(overrides.join(QStringLiteral("，"))));
    } else {
        m_fontHint->clear();
    }

    m_removeTheme->setEnabled(m_installedIds.contains(id));
}

void SettingsDialog::importThemePackage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("导入主题包"), QDir::homePath(),
        tr("Hutaomu 主题包 (*.htmtpi *.zip)"));
    if (path.isEmpty())
        return;

    QString error;
    const QString id = editor::themepkg::peekThemeId(path, &error);
    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("导入主题包"),
                             error.isEmpty() ? tr("主题包无效。") : error);
        return;
    }
    for (const editor::ThemeDefinition& theme : editor::ThemeManager::availableThemes()) {
        if (theme.id != id)
            continue;
        if (theme.builtin) {
            QMessageBox::information(
                this, tr("导入主题包"),
                tr("「%1」是内置主题，不能安装同名主题。").arg(theme.name));
            return;
        }
        const auto answer = QMessageBox::question(
            this, tr("导入主题包"),
            tr("已安装同名主题「%1」，是否覆盖？").arg(theme.name));
        if (answer != QMessageBox::Yes)
            return;
        break;
    }

    QString installedId;
    if (!editor::ThemeManager::installThemePackage(path, &installedId, &error)) {
        QMessageBox::warning(this, tr("导入主题包"),
                             error.isEmpty() ? tr("安装失败。") : error);
        return;
    }
    rebuildThemeList(installedId);
    updateThemeHints();
    emit themePreviewRequested(installedId); // 导入后立刻看到效果
}

void SettingsDialog::removeSelectedTheme()
{
    const QString id = theme();
    if (!m_installedIds.contains(id))
        return;
    const editor::ThemeDefinition theme = editor::ThemeManager::themeDefinition(id);
    const auto answer = QMessageBox::question(
        this, tr("删除主题"), tr("确定删除主题「%1」？").arg(theme.name));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!editor::ThemeManager::uninstallTheme(id, &error)) {
        QMessageBox::warning(this, tr("删除主题"),
                             error.isEmpty() ? tr("删除失败。") : error);
        return;
    }
    // 删掉的正是当前选中的主题时，回到默认主题，避免 OK 后应用一个不存在的 id。
    rebuildThemeList(QString::fromLatin1(kDefaultThemeId));
    updateThemeHints();
}

QString SettingsDialog::theme() const
{
    if (auto* item = m_themeList->currentItem())
        return item->data(Qt::UserRole).toString();
    return QString::fromLatin1(kDefaultThemeId);
}

QString SettingsDialog::fontFamily() const
{
    return m_fontCombo->currentData().toString();
}

int SettingsDialog::fontSize() const
{
    return m_fontSize->value();
}

int SettingsDialog::tabWidth() const
{
    return m_tabWidth->value();
}

bool SettingsDialog::wordWrap() const
{
    return m_wordWrap->isChecked();
}

QString SettingsDialog::markdownViewMode() const
{
    return m_mdModeCombo->currentData().toString();
}

QString SettingsDialog::sidebarPosition() const
{
    return m_sidebarPosCombo->currentData().toString();
}

QString SettingsDialog::previewPosition() const
{
    return m_previewPosCombo->currentData().toString();
}

} // namespace app
