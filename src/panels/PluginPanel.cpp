// Hutaomu Editor - 插件管理侧边栏面板实现。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "PluginPanel.h"

#include "plugins/PluginManager.h"
#include "themes/IconLoader.h"
#include "themes/ThemeManager.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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

// 卡片左侧的字母头像：accent 底（低透明度）+ accent 色首字母
QLabel* makeAvatar(const QString& name, QWidget* parent)
{
    const QColor accent = editor::ThemeManager::color(QStringLiteral("accent"));
    QColor background = accent;
    background.setAlpha(32);
    auto* avatar = new QLabel(parent);
    avatar->setFixedSize(38, 38);
    avatar->setAlignment(Qt::AlignCenter);
    const QString letter = name.isEmpty()
                               ? QStringLiteral("P")
                               : name.left(1).toUpper();
    avatar->setText(letter);
    QFont font = avatar->font();
    font.setPointSize(font.pointSize() + 3);
    font.setBold(true);
    avatar->setFont(font);
    avatar->setStyleSheet(
        QStringLiteral("background:%1; color:%2; border-radius:9px;")
            .arg(background.name(QColor::HexArgb), accent.name()));
    return avatar;
}

// 状态徽章：预装（accent 底）/ 已停用（灰底）
QLabel* makeBadge(bool builtin, bool enabled, QWidget* parent)
{
    const QColor accent = editor::ThemeManager::color(QStringLiteral("accent"));
    auto* badge = new QLabel(parent);
    badge->setObjectName(QStringLiteral("pluginBadge"));
    if (builtin) {
        QColor background = accent;
        background.setAlpha(36);
        badge->setText(QObject::tr("预装"));
        badge->setStyleSheet(QStringLiteral(
            "background:%1; color:%2; border-radius:6px; padding:1px 7px;"
            "font-size:10px; font-weight:600;")
            .arg(background.name(QColor::HexArgb), accent.name()));
    } else if (!enabled) {
        const QColor gray = editor::ThemeManager::color(QStringLiteral("titleBarMutedFg"));
        QColor background = gray;
        background.setAlpha(40);
        badge->setText(QObject::tr("已停用"));
        badge->setStyleSheet(QStringLiteral(
            "background:%1; color:%2; border-radius:6px; padding:1px 7px;"
            "font-size:10px; font-weight:600;")
            .arg(background.name(QColor::HexArgb), gray.name()));
    } else {
        badge->setVisible(false);
    }
    return badge;
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
    m_subtitle = new QLabel(m_header);
    m_subtitle->setObjectName(QStringLiteral("sidebarSubtitle"));
    titleBox->addWidget(m_subtitle);
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

    // --- 搜索框（VS Code 扩展视图同位）---
    auto* searchRow = new QWidget(this);
    searchRow->setObjectName(QStringLiteral("searchRow"));
    auto* searchLayout = new QHBoxLayout(searchRow);
    searchLayout->setContentsMargins(8, 6, 8, 6);
    m_search = new QLineEdit(searchRow);
    m_search->setObjectName(QStringLiteral("searchInput"));
    m_search->setPlaceholderText(tr("搜索插件..."));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged,
            this, &PluginPanel::applyFilter);
    searchLayout->addWidget(m_search);
    layout->addWidget(searchRow);

    // --- Body ---
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("pluginList"));
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        auto* item = m_list->currentItem();
        m_uninstallButton->setEnabled(item != nullptr);
    });
    layout->addWidget(m_list, 1);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("pluginHint"));
    m_hint->setWordWrap(true);
    m_hint->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_hint->setContentsMargins(16, 24, 16, 8);
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

void PluginPanel::applyFilter()
{
    const QString needle = m_search->text().trimmed();
    for (int i = 0; i < m_list->count(); ++i) {
        auto* item = m_list->item(i);
        if (needle.isEmpty()) {
            item->setHidden(false);
            continue;
        }
        const QString haystack = item->data(Qt::UserRole + 2).toString();
        item->setHidden(!haystack.contains(needle, Qt::CaseInsensitive));
    }
}

void PluginPanel::reload()
{
    if (!m_list)
        return;
    m_list->blockSignals(true);
    m_list->clear();
    const auto all = plugins::PluginManager::instance().plugins();
    const QColor foreground = editor::ThemeManager::tokens().editorForeground;
    const QColor muted = editor::ThemeManager::color(QStringLiteral("titleBarMutedFg"));
    int enabledCount = 0;

    for (const plugins::Plugin& plugin : all) {
        if (plugin.enabled)
            ++enabledCount;

        auto* item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, plugin.manifest.id);
        item->setData(Qt::UserRole + 1, plugin.builtin);
        // 搜索索引：名称 + id + 描述
        item->setData(Qt::UserRole + 2,
                      QStringLiteral("%1 %2 %3").arg(plugin.manifest.name,
                                                     plugin.manifest.id,
                                                     plugin.manifest.description));

        auto* card = new QWidget;
        auto* row = new QHBoxLayout(card);
        row->setContentsMargins(10, 9, 10, 9);
        row->setSpacing(10);

        row->addWidget(makeAvatar(plugin.manifest.name, card));

        auto* info = new QVBoxLayout;
        info->setSpacing(2);

        auto* nameRow = new QHBoxLayout;
        nameRow->setSpacing(6);
        auto* nameLabel = new QLabel(plugin.manifest.name, card);
        QFont nameFont = nameLabel->font();
        nameFont.setBold(true);
        nameLabel->setFont(nameFont);
        nameLabel->setStyleSheet(
            QStringLiteral("color:%1;").arg(foreground.name()));
        nameRow->addWidget(nameLabel);
        nameRow->addWidget(makeBadge(plugin.builtin, plugin.enabled, card));
        nameRow->addStretch(1);
        info->addLayout(nameRow);

        const QString meta = plugin.manifest.description.isEmpty()
            ? QStringLiteral("%1 · v%2").arg(plugin.manifest.publisher,
                                             plugin.manifest.version)
            : plugin.manifest.description;
        auto* metaLabel = new QLabel(card);
        const int elideWidth = qMax(80, m_list->viewport()->width() - 150);
        metaLabel->setText(
            QFontMetrics(metaLabel->font()).elidedText(
                meta, Qt::ElideRight, elideWidth));
        metaLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:11px;").arg(muted.name()));
        info->addWidget(metaLabel);

        row->addLayout(info, 1);

        auto* toggle = new QCheckBox(card);
        toggle->setChecked(plugin.enabled);
        toggle->setToolTip(plugin.enabled
                               ? tr("点击停用")
                               : tr("点击启用"));
        const QString pluginId = plugin.manifest.id;
        connect(toggle, &QCheckBox::toggled, this,
                [this, pluginId](bool enabled) { toggleSelected(pluginId, enabled); });
        row->addWidget(toggle);

        item->setSizeHint(QSize(0, qMax(56, card->sizeHint().height())));
        m_list->setItemWidget(item, card);
    }
    m_list->blockSignals(false);

    m_subtitle->setText(all.isEmpty()
        ? tr("尚未安装插件")
        : tr("%1 个插件 · %2 已启用").arg(all.size()).arg(enabledCount));
    m_hint->setText(all.isEmpty()
        ? tr("还没有插件。\n\n插件是声明式包（.htmed），可以带来主题、"
             "文件格式与命令，不含可执行代码。\n点右上角按钮导入，或把包拖进窗口。")
        : QString());
    m_hint->setVisible(all.isEmpty());
    m_list->setVisible(!all.isEmpty());
    m_uninstallButton->setEnabled(m_list->currentItem() != nullptr);
    applyFilter();
}

void PluginPanel::toggleSelected(const QString& pluginId, bool enabled)
{
    QString error;
    if (!plugins::PluginManager::instance().setEnabled(pluginId, enabled, &error))
        QMessageBox::warning(this, tr("插件"), error);
    // reload 会 clear 列表（连带删除发信号中的复选框）——延迟到事件循环重建
    QMetaObject::invokeMethod(this, &PluginPanel::reload, Qt::QueuedConnection);
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
