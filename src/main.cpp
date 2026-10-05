// Hutaomu Editor - Application entry point.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include <QApplication>
#include <QFile>
#include <QLibraryInfo>
#include <QStyleFactory>
#include <QTranslator>

#include "app/MainWindow.h"
#include <QTimer>
#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "app/CommandIndex.h"
#include "app/FileAssociations.h"
#include "plugins/PluginManager.h"
#include "settings/AppSettings.h"
#include "themes/ThemeManager.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

#ifdef Q_OS_WIN
    // 安装包 installer.iss 里声明了 AppMutex=HutaomuEditorMutex：
    // 安装/卸载时据此发现"程序还在运行"，提示用户关闭，而不是留下被占用的
    // DLL 删不掉（实测：程序运行时卸载会残留 platforms/styles 等目录）。
    CreateMutexW(nullptr, FALSE, L"HutaomuEditorMutex");
#endif
    QApplication::setOrganizationName(QStringLiteral("Hutaomu"));
    QApplication::setApplicationName(QStringLiteral("Hutaomu Editor"));
    QApplication::setApplicationVersion(QStringLiteral(HUTAOMU_VERSION));

#ifdef Q_OS_WIN
    // 中文界面默认字体：微软雅黑比 Segoe UI 的 fallback 更耐看。
    QFont uiFont(QStringLiteral("Microsoft YaHei UI"), 9);
    QApplication::setFont(uiFont);
#endif

    // 加载 Qt 自身的中文翻译（对话框标准按钮等）
    // Qt 自带对话框（文件选择、QMessageBox 标准按钮等）的中文：Qt6 的翻译文件名是
    // qtbase_<locale>.qm，Qt5 时代是 qt_<locale>.qm，windeployqt 两种都可能产出，
    // 这里逐个尝试，任一命中即可。
    QTranslator qtTranslator;
    const QString appDirTranslations
        = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
    bool loaded = false;
    for (const QString& name : { QStringLiteral("qtbase_zh_CN"), QStringLiteral("qt_zh_CN") }) {
        if (qtTranslator.load(name, appDirTranslations)) {
            loaded = true;
            break;
        }
    }
    if (!loaded) {
        const QString qtTranslations = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
        for (const QString& name : { QStringLiteral("qtbase_zh_CN"), QStringLiteral("qt_zh_CN") }) {
            if (qtTranslator.load(name, qtTranslations)) {
                loaded = true;
                break;
            }
        }
    }
    if (!loaded
        && qtTranslator.load(QLocale::system(), QStringLiteral("qtbase"), QStringLiteral("_"),
                             QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        loaded = true;
    }
    if (loaded)
        QApplication::installTranslator(&qtTranslator);

    auto& s = settings::AppSettings::instance();
    s.load();

    editor::ThemeManager::applyTheme(s.theme);

    // 控制台中文输出（Windows 控制台默认 GBK，会把 UTF-8 打成乱码）
#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
#endif

    // --list-plugins：打印插件与贡献点后退出（排查"插件装了没生效"时的第一步）
    const QStringList arguments = app.arguments();
    if (arguments.contains(QStringLiteral("--list-plugins"))) {
        auto& manager = plugins::PluginManager::instance();
        std::printf("插件目录：%s\n", qPrintable(plugins::PluginManager::userPluginDirectory()));
        const auto all = manager.plugins();
        if (all.isEmpty())
            std::printf("（没有已安装的插件）\n");
        for (const plugins::Plugin& plugin : all) {
            std::printf("%s  %s v%s  [%s]\n", qPrintable(plugin.manifest.id),
                        qPrintable(plugin.manifest.name),
                        qPrintable(plugin.manifest.version),
                        plugin.enabled ? "启用" : "禁用");
            for (const plugins::PluginCommand& command : plugin.manifest.commands)
                std::printf("    命令  %s.%s  ->  %s%s\n", qPrintable(plugin.manifest.id),
                            qPrintable(command.id),
                            command.action.isEmpty() ? "shell: " : "action: ",
                            qPrintable(command.action.isEmpty() ? command.shell
                                                                : command.action));
            for (const plugins::PluginFormat& format : plugin.manifest.formats)
                std::printf("    格式  *.%s  ->  语言 %s\n", qPrintable(format.extension),
                            qPrintable(format.languageId));
            for (const QString& themePath : plugin.manifest.themePaths)
                std::printf("    主题  %s\n", qPrintable(themePath));
            for (const plugins::PluginStatusItem& item : plugin.manifest.statusItems)
                std::printf("    状态栏  %s（%s，%s）\n", qPrintable(item.id),
                            qPrintable(item.type), qPrintable(item.alignment));
            for (const plugins::PluginPage& page : plugin.manifest.pages)
                std::printf("    页面  %s（%s：%s）\n", qPrintable(page.title),
                            qPrintable(page.type), qPrintable(page.content));
        }
        return 0;
    }

    // --check-associations：逐项打印文件关联状态（新系统上一条命令自检）
    if (arguments.contains(QStringLiteral("--check-associations"))) {
        const auto statuses = app::associations::statusForAll();
        for (const app::associations::ExtensionStatus& status : statuses) {
            std::printf("%-12s %-22s %s%s\n", qPrintable(status.extension),
                        status.progId.isEmpty() ? "（未关联）" : qPrintable(status.progId),
                        status.isOurs ? "本程序" : "其他程序",
                        status.heldByUserChoice ? "（系统默认应用锁定，需手动改一次）" : "");
        }
        return 0;
    }

    // --set-associations：以当前用户身份登记关联（免管理员），并打印结果
    if (arguments.contains(QStringLiteral("--set-associations"))) {
        QString error;
        const QStringList written = app::associations::writeUserAssociations(&error);
        if (!error.isEmpty()) {
            std::fprintf(stderr, "%s\n", qPrintable(error));
            return 1;
        }
        std::printf("已登记 %d 个扩展名：%s\n", int(written.size()),
                    qPrintable(written.join(QLatin1Char(' '))));
        return 0;
    }

    // --list-commands：列出命令面板里的全部条目（排查/文档用）
    if (arguments.contains(QStringLiteral("--list-commands"))) {
        const auto entries = app::buildPaletteEntries();
        for (const app::PaletteEntry& entry : entries) {
            std::printf("%-46s %-14s %s\n", qPrintable(entry.id),
                        qPrintable(entry.category), qPrintable(entry.title));
        }
        std::printf("（共 %d 条）\n", int(entries.size()));
        return 0;
    }

    app::MainWindow window;

    // Files passed on the command line (e.g. "Open with...").
    for (int i = 1; i < argc; ++i) {
        const QString path = QString::fromLocal8Bit(argv[i]);
        if (QFile::exists(path))
            window.openPath(path);
    }

    if (s.maximized)
        window.showMaximized();
    else
        window.show();

    // --run-plugin-command <完整命令 id>：执行后正常关闭（脚本化验证用）
    const int commandIndex = arguments.indexOf(QStringLiteral("--run-plugin-command"));
    if (commandIndex > 0 && commandIndex + 1 < arguments.size()) {
        const QString qualifiedId = arguments.at(commandIndex + 1);
        QTimer::singleShot(400, &window, [&window, qualifiedId] {
            QString error;
            const bool ok = window.runPluginCommand(qualifiedId, &error);
            if (!ok)
                std::fprintf(stderr, "命令执行失败：%s\n", qPrintable(error));
            window.close(); // 正常关闭：设置与状态得以保存
        });
    }

    return app.exec();
}
