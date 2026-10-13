// Hutaomu Editor - 插件管理侧边栏面板实现。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "PluginPanel.h"

#include "core/ZipWriter.h"
#include "plugins/PluginManager.h"
#include "themes/IconLoader.h"
#include "themes/ThemeManager.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>
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

// 卡片/详情共用的字母头像
QLabel* makeAvatar(const QString& name, int size, QWidget* parent)
{
    const QColor accent = editor::ThemeManager::color(QStringLiteral("accent"));
    QColor background = accent;
    background.setAlpha(32);
    auto* avatar = new QLabel(parent);
    avatar->setFixedSize(size, size);
    avatar->setAlignment(Qt::AlignCenter);
    const QString letter = name.isEmpty()
                               ? QStringLiteral("P")
                               : name.left(1).toUpper();
    avatar->setText(letter);
    QFont font = avatar->font();
    font.setPointSize(qMax(9, font.pointSize() + (size >= 48 ? 6 : 3)));
    font.setBold(true);
    avatar->setFont(font);
    avatar->setStyleSheet(
        QStringLiteral("background:%1; color:%2; border-radius:%3px;")
            .arg(background.name(QColor::HexArgb), accent.name())
            .arg(size / 4));
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

// 把插件目录打包成 .htmed（ZIP；目录相对路径用 '/'）
bool zipDirectory(const QString& rootPath, const QString& targetZip, QString* error)
{
    core::ZipWriter zip(targetZip);
    QDirIterator it(rootPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFile file(it.filePath());
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = QObject::tr("无法读取 %1").arg(it.filePath());
            return false;
        }
        QString archivePath = QDir(rootPath).relativeFilePath(it.filePath());
        archivePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
        zip.addFile(archivePath, file.readAll());
    }
    zip.close();
    if (zip.failed()) {
        if (error)
            *error = zip.errorString();
        return false;
    }
    return true;
}

} // namespace

PluginPanel::PluginPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("pluginPanel"));
    setAcceptDrops(true); // 拖 .htmed 进来直接安装

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

    m_refreshButton = new QToolButton(m_header);
    m_refreshButton->setObjectName(QStringLiteral("sidebarButton"));
    m_refreshButton->setToolTip(tr("重新扫描插件目录"));
    m_refreshButton->setAutoRaise(true);
    m_refreshButton->setProperty("iconName", QStringLiteral("refresh"));
    connect(m_refreshButton, &QToolButton::clicked, this, [this] {
        plugins::PluginManager::instance().reload();
        reload();
        emit pluginsChanged();
    });
    headerLayout->addWidget(m_refreshButton);

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

    // --- 搜索框 ---
    auto* searchRow = new QWidget(this);
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

    // --- 页面：列表 ⇄ 详情 ---
    m_pages = new QStackedWidget(this);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("pluginList"));
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        m_uninstallButton->setEnabled(m_list->currentItem() != nullptr);
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        showDetails(item->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) {
                auto* item = m_list->itemAt(pos);
                if (!item)
                    return;
                m_list->setCurrentItem(item);
                const QString id = item->data(Qt::UserRole).toString();
                const bool builtin = item->data(Qt::UserRole + 1).toBool();
                QMenu menu(this);
                menu.addAction(tr("查看详情"), this, [this, id] { showDetails(id); });
                menu.addAction(tr("导出插件包..."), this,
                               [this, id] { exportPackage(id); });
                menu.addAction(tr("打开所在文件夹"), this,
                               [this, id] { openPluginFolder(id); });
                menu.addSeparator();
                QAction* uninstall = menu.addAction(
                    tr("卸载"), this, [this] { uninstallSelected(); });
                uninstall->setEnabled(!builtin);
                menu.exec(m_list->mapToGlobal(pos));
            });
    m_pages->addWidget(m_list);

    // --- 详情页 ---
    auto* details = new QWidget(this);
    auto* detailsLayout = new QVBoxLayout(details);
    detailsLayout->setContentsMargins(14, 10, 14, 12);
    detailsLayout->setSpacing(10);

    auto* backRow = new QHBoxLayout;
    auto* backButton = new QToolButton(details);
    backButton->setText(tr("← 返回列表"));
    backButton->setObjectName(QStringLiteral("sidebarButton"));
    backButton->setAutoRaise(true);
    connect(backButton, &QToolButton::clicked, this, &PluginPanel::showList);
    backRow->addWidget(backButton);
    backRow->addStretch(1);
    detailsLayout->addLayout(backRow);

    auto* headRow = new QHBoxLayout;
    headRow->setSpacing(12);
    m_detailsAvatar = new QLabel(details);
    m_detailsAvatar->setAlignment(Qt::AlignCenter);
    headRow->addWidget(m_detailsAvatar);
    auto* nameBox = new QVBoxLayout;
    nameBox->setSpacing(2);
    m_detailsName = new QLabel(details);
    {
        QFont font = m_detailsName->font();
        font.setBold(true);
        font.setPointSize(font.pointSize() + 2);
        m_detailsName->setFont(font);
    }
    nameBox->addWidget(m_detailsName);
    m_detailsVersion = new QLabel(details);
    m_detailsVersion->setObjectName(QStringLiteral("sidebarSubtitle"));
    nameBox->addWidget(m_detailsVersion);
    headRow->addLayout(nameBox);
    headRow->addStretch(1);
    detailsLayout->addLayout(headRow);

    m_detailsMeta = new QLabel(details);
    m_detailsMeta->setObjectName(QStringLiteral("sidebarSubtitle"));
    m_detailsMeta->setWordWrap(true);
    detailsLayout->addWidget(m_detailsMeta);

    m_detailsDescription = new QLabel(details);
    m_detailsDescription->setWordWrap(true);
    detailsLayout->addWidget(m_detailsDescription);

    m_detailsPermissions = new QLabel(details);
    m_detailsPermissions->setWordWrap(true);
    m_detailsPermissions->setTextFormat(Qt::RichText);
    detailsLayout->addWidget(m_detailsPermissions);

    m_detailsContrib = new QLabel(details);
    m_detailsContrib->setWordWrap(true);
    m_detailsContrib->setTextFormat(Qt::RichText);
    detailsLayout->addWidget(m_detailsContrib, 1);

    auto* actionsRow = new QHBoxLayout;
    actionsRow->setSpacing(6);
    m_detailsToggle = new QCheckBox(tr("启用"), details);
    connect(m_detailsToggle, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_detailsId.isEmpty())
            return;
        toggleSelected(m_detailsId, enabled);
        showDetails(m_detailsId); // 刷新徽章/文案
    });
    actionsRow->addWidget(m_detailsToggle);
    actionsRow->addStretch(1);
    m_detailsExport = new QToolButton(details);
    m_detailsExport->setText(tr("导出包"));
    m_detailsExport->setToolTip(tr("把该插件打包为 .htmed 分享/备份"));
    connect(m_detailsExport, &QToolButton::clicked,
            this, [this] { exportPackage(m_detailsId); });
    actionsRow->addWidget(m_detailsExport);
    m_detailsFolder = new QToolButton(details);
    m_detailsFolder->setText(tr("打开文件夹"));
    m_detailsFolder->setToolTip(tr("在文件管理器中打开插件目录"));
    connect(m_detailsFolder, &QToolButton::clicked,
            this, [this] { openPluginFolder(m_detailsId); });
    actionsRow->addWidget(m_detailsFolder);
    m_detailsUninstall = new QToolButton(details);
    m_detailsUninstall->setText(tr("卸载"));
    m_detailsUninstall->setObjectName(QStringLiteral("sidebarButton"));
    connect(m_detailsUninstall, &QToolButton::clicked,
            this, &PluginPanel::uninstallSelected);
    actionsRow->addWidget(m_detailsUninstall);
    detailsLayout->addLayout(actionsRow);

    m_pages->addWidget(details);
    layout->addWidget(m_pages, 1);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("pluginHint"));
    m_hint->setWordWrap(true);
    m_hint->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_hint->setContentsMargins(16, 24, 16, 8);
    layout->addWidget(m_hint);

    reload();
}

void PluginPanel::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
            if (suffix == QLatin1String("htmed") || suffix == QLatin1String("zip")) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void PluginPanel::dropEvent(QDropEvent* event)
{
    for (const QUrl& url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == QLatin1String("htmed") || suffix == QLatin1String("zip")) {
            event->acceptProposedAction();
            importPackageFrom(path);
            return; // 一次装一个（确认页是模态的）
        }
    }
    event->ignore();
}

void PluginPanel::importPackageFrom(const QString& path)
{
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

void PluginPanel::refreshIcons()
{
    const QColor fg = editor::ThemeManager::tokens().editorForeground;
    const QColor bg = editor::ThemeManager::tokens().editorBackground;
    m_refreshButton->setIcon(editor::IconLoader::load(
        QStringLiteral("refresh"), fg, bg, 14));
    m_importButton->setIcon(editor::IconLoader::load(
        QStringLiteral("tab-add"), fg, bg, 14));
    m_uninstallButton->setIcon(editor::IconLoader::load(
        QStringLiteral("trash"), fg, bg, 14));
}

void PluginPanel::showDetails(const QString& pluginId)
{
    const plugins::Plugin plugin = plugins::PluginManager::instance().plugin(pluginId);
    if (plugin.manifest.id.isEmpty())
        return;
    m_detailsId = pluginId;

    const QColor foreground = editor::ThemeManager::tokens().editorForeground;
    const QColor muted = editor::ThemeManager::color(QStringLiteral("titleBarMutedFg"));

    // 头像（详情页大号）
    const QColor accent = editor::ThemeManager::color(QStringLiteral("accent"));
    QColor avatarBackground = accent;
    avatarBackground.setAlpha(32);
    m_detailsAvatar->setFixedSize(48, 48);
    m_detailsAvatar->setText(plugin.manifest.name.left(1).toUpper());
    {
        QFont font = m_detailsAvatar->font();
        font.setPointSize(font.pointSize() + 6);
        font.setBold(true);
        m_detailsAvatar->setFont(font);
    }
    m_detailsAvatar->setStyleSheet(
        QStringLiteral("background:%1; color:%2; border-radius:12px;")
            .arg(avatarBackground.name(QColor::HexArgb), accent.name()));

    m_detailsName->setText(plugin.manifest.name);
    m_detailsVersion->setText(QStringLiteral("v%1").arg(plugin.manifest.version));
    m_detailsMeta->setText(QStringLiteral(
        "<span style='color:%1;'>%2 · %3</span>")
        .arg(muted.name(),
             plugin.manifest.publisher,
             plugin.manifest.license.isEmpty()
                 ? tr("许可证未声明")
                 : plugin.manifest.license));
    m_detailsDescription->setText(
        plugin.manifest.description.isEmpty()
            ? tr("（无描述）")
            : plugin.manifest.description);
    m_detailsDescription->setStyleSheet(
        QStringLiteral("color:%1;").arg(foreground.name()));

    // 权限
    QStringList permissions;
    if (plugin.manifest.declaresShellPermission())
        permissions.append(tr("执行外部程序（首次执行前会再确认）"));
    for (const QString& permission : plugin.manifest.permissions) {
        if (permission == QLatin1String("shell"))
            continue;
        permissions.append(permission);
    }
    m_detailsPermissions->setText(
        QStringLiteral("<b>%1</b><br/>· %2")
            .arg(tr("申请的权限"),
                 permissions.isEmpty()
                     ? tr("无（纯声明式）")
                     : permissions.join(QStringLiteral("<br/>· "))));
    m_detailsPermissions->setStyleSheet(
        QStringLiteral("color:%1; font-size:11px;").arg(muted.name()));

    // 贡献点
    QStringList contribLines;
    if (!plugin.manifest.commands.isEmpty()) {
        for (const plugins::PluginCommand& command : plugin.manifest.commands) {
            contribLines.append(command.shortcut.isEmpty()
                ? command.title.toHtmlEscaped()
                : QStringLiteral("%1 <span style='color:%2;'>%3</span>")
                    .arg(command.title.toHtmlEscaped(),
                         muted.name(), command.shortcut.toHtmlEscaped()));
        }
    }
    if (!plugin.manifest.formats.isEmpty()) {
        QStringList extensions;
        for (const plugins::PluginFormat& format : plugin.manifest.formats)
            extensions.append(QStringLiteral("*.%1 → %2")
                .arg(format.extension, format.languageId.toHtmlEscaped()));
        contribLines.append(extensions.join(QStringLiteral("<br/>")));
    }
    if (!plugin.manifest.themePaths.isEmpty())
        contribLines.append(tr("%1 套主题").arg(plugin.manifest.themePaths.size()));
    for (const plugins::PluginPage& page : plugin.manifest.pages)
        contribLines.append(tr("页面：%1").arg(page.title.toHtmlEscaped()));
    for (const plugins::PluginStatusItem& item : plugin.manifest.statusItems)
        contribLines.append(tr("状态栏：%1").arg(item.type));
    m_detailsContrib->setText(
        contribLines.isEmpty()
            ? QStringLiteral("<span style='color:%1;'>%2</span>")
                  .arg(muted.name(), tr("（无贡献点）"))
            : contribLines.join(QStringLiteral("<br/>")));
    m_detailsContrib->setStyleSheet(
        QStringLiteral("color:%1; font-size:11px;").arg(foreground.name()));

    // 动作
    m_detailsToggle->blockSignals(true);
    m_detailsToggle->setChecked(plugin.enabled);
    m_detailsToggle->blockSignals(false);
    m_detailsUninstall->setVisible(!plugin.builtin);
    m_detailsUninstall->setToolTip(plugin.builtin
        ? tr("预装插件不可卸载，可用上面的开关停用")
        : tr("卸载该插件"));

    m_pages->setCurrentIndex(1);
}

void PluginPanel::showList()
{
    m_detailsId.clear();
    m_pages->setCurrentIndex(0);
}

bool PluginPanel::exportPackage(const QString& pluginId)
{
    const plugins::Plugin plugin = plugins::PluginManager::instance().plugin(pluginId);
    if (plugin.manifest.id.isEmpty() || plugin.rootPath.isEmpty())
        return false;
    const QString suggested = QDir(QDir::homePath()).filePath(
        plugin.manifest.id + QStringLiteral(".htmed"));
    const QString target = QFileDialog::getSaveFileName(
        this, tr("导出插件包"), suggested,
        tr("Hutaomu 插件包 (*.htmed)"));
    if (target.isEmpty())
        return false;
    QString error;
    if (!zipDirectory(plugin.rootPath, target, &error)) {
        QMessageBox::warning(this, tr("导出插件包"),
                             error.isEmpty() ? tr("导出失败。") : error);
        return false;
    }
    QMessageBox::information(this, tr("导出插件包"),
                             tr("已导出到：\n%1").arg(target));
    return true;
}

void PluginPanel::openPluginFolder(const QString& pluginId)
{
    const plugins::Plugin plugin = plugins::PluginManager::instance().plugin(pluginId);
    if (plugin.rootPath.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(plugin.rootPath));
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
        item->setData(Qt::UserRole + 2,
                      QStringLiteral("%1 %2 %3").arg(plugin.manifest.name,
                                                     plugin.manifest.id,
                                                     plugin.manifest.description));

        auto* card = new QWidget;
        auto* row = new QHBoxLayout(card);
        row->setContentsMargins(10, 9, 10, 9);
        row->setSpacing(10);

        row->addWidget(makeAvatar(plugin.manifest.name, 38, card));

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
             "文件格式与命令，不含可执行代码。\n点右上角按钮导入，或把包直接拖进本面板。")
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
    importPackageFrom(path);
}

void PluginPanel::uninstallSelected()
{
    const QString id = m_pages->currentIndex() == 1 && !m_detailsId.isEmpty()
        ? m_detailsId
        : (m_list->currentItem()
               ? m_list->currentItem()->data(Qt::UserRole).toString()
               : QString());
    if (id.isEmpty())
        return;
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
    if (m_pages->currentIndex() == 1)
        showList();
    reload();
    emit pluginsChanged();
}

} // namespace panels
