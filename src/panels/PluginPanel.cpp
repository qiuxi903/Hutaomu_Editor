// Hutaomu Editor - 插件管理侧边栏面板实现。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "PluginPanel.h"

#include "plugins/PluginManager.h"
#include "themes/IconLoader.h"
#include "themes/ThemeManager.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace panels {

namespace {

// 安装确认：展示插件身份、贡献点与申请的权限（对标 VS Code 的权限提示）
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

PluginPanel::PluginPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("pluginPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- Header（拖拽换边把手）---
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("sidebarHeader"));
    auto* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(14, 8, 6, 8);

    auto* titleBox = new QVBoxLayout;
    titleBox->setSpacing(1);
    auto* titleLabel = new QLabel(tr("插件"), m_header);
    titleLabel->setObjectName(QStringLiteral("sidebarTitle"));
    titleBox->addWidget(titleLabel);
    auto* subtitleLabel = new QLabel(tr("启用 / 停用 / 导入插件包"), m_header);
    subtitleLabel->setObjectName(QStringLiteral("sidebarSubtitle"));
    titleBox->addWidget(subtitleLabel);
    headerLayout->addLayout(titleBox);
    headerLayout->addStretch();

    m_importButton = new QToolButton(m_header);
    m_importButton->setObjectName(QStringLiteral("sidebarButton"));
    m_importButton->setToolTip(tr("导入插件包 (.htmed)"));
    m_importButton->setAutoRaise(true);
    m_importButton->setProperty("iconName", QStringLiteral("tab-add"));
    connect(m_importButton, &QToolButton::clicked,
            this, &PluginPanel::importPackage);
    headerLayout->addWidget(m_importButton);

    m_uninstallButton = new QToolButton(m_header);
    m_uninstallButton->setObjectName(QStringLiteral("sidebarButton"));
    m_uninstallButton->setToolTip(tr("卸载选中的插件"));
    m_uninstallButton->setAutoRaise(true);
    m_uninstallButton->setProperty("iconName", QStringLiteral("trash"));
    connect(m_uninstallButton, &QToolButton::clicked,
            this, &PluginPanel::uninstallSelected);
    headerLayout->addWidget(m_uninstallButton);

    layout->addWidget(m_header);

    // --- Body ---
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("pluginList"));
    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (!item)
            return;
        toggleSelected(item->data(Qt::UserRole).toString(),
                       item->checkState() == Qt::Checked);
    });
    layout->addWidget(m_list, 1);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("pluginHint"));
    m_hint->setWordWrap(true);
    m_hint->setContentsMargins(10, 6, 10, 8);
    layout->addWidget(m_hint);

    reload();
}

void PluginPanel::refreshIcons()
{
    const QColor fg = editor::ThemeManager::tokens().editorForeground;
    const QColor bg = editor::ThemeManager::tokens().editorBackground;
    m_importButton->setIcon(editor::IconLoader::load(
        QStringLiteral("tab-add"), fg, bg, 14));
    m_uninstallButton->setIcon(editor::IconLoader::load(
        QStringLiteral("trash"), fg, bg, 14));
}

void PluginPanel::reload()
{
    if (!m_list)
        return;
    m_list->blockSignals(true);
    m_list->clear();
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
            m_list);
        item->setData(Qt::UserRole, plugin.manifest.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(plugin.enabled ? Qt::Checked : Qt::Unchecked);
        if (plugin.builtin)
            item->setToolTip(tr("随程序预装的插件：可以禁用，不能卸载"));
        else if (plugin.manifest.usesShell() && plugin.manifest.declaresShellPermission())
            item->setToolTip(tr("包含外部命令；首次执行时会弹窗确认"));
        else if (!plugin.manifest.description.isEmpty())
            item->setToolTip(plugin.manifest.description);
    }
    m_list->blockSignals(false);

    m_hint->setText(
        all.isEmpty()
            ? tr("还没有插件。插件是声明式包（.htmed），可以带来主题、文件格式与命令；不含可执行代码。")
            : tr("勾选启用 / 取消勾选禁用。导入插件包会先展示它申请的权限。"));
    m_uninstallButton->setEnabled(m_list->currentItem() != nullptr);
}

void PluginPanel::toggleSelected(const QString& pluginId, bool enabled)
{
    Q_UNUSED(pluginId);
    // 找到与 id 对应的列表项（itemChanged 的发起者），而不是依赖"当前项"
    QListWidgetItem* target = nullptr;
    for (int i = 0; i < m_list->count() && !target; ++i)
        if (m_list->item(i)->data(Qt::UserRole).toString() == pluginId)
            target = m_list->item(i);
    if (!target)
        return;
    m_uninstallButton->setEnabled(
        !plugins::PluginManager::instance().isBuiltin(pluginId));
    QString error;
    if (!plugins::PluginManager::instance().setEnabled(pluginId, enabled, &error))
        QMessageBox::warning(this, tr("插件"), error);
    reload();
    emit pluginsChanged();
}

void PluginPanel::importPackage()
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
    reload();
    emit pluginsChanged();
}

void PluginPanel::uninstallSelected()
{
    auto* item = m_list->currentItem();
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
    reload();
    emit pluginsChanged();
}

} // namespace panels
