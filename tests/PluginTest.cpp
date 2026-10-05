// Hutaomu Editor - Declarative plugin (L1) regression tests.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
//
// 覆盖：manifest 校验（id=publisher.name、权限与 shell 一致性、非法值拒绝）、
// .htmed 与目录两种安装、路径穿越拒绝、启停与信任状态持久化、卸载，
// 以及三类贡献点真正接入（主题进主题列表、格式被语言检测采用、命令可解析）。
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cstdio>

#include "core/ZipWriter.h"
#include "plugins/PluginManager.h"
#include "syntax/LanguageRegistry.h"
#include "themes/ThemeManager.h"

namespace {

int failures = 0;

void expect(bool condition, const char* what)
{
    if (condition)
        std::printf("  PASS  %s\n", what);
    else {
        std::printf("  FAIL  %s\n", what);
        ++failures;
    }
}

bool writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(data) == data.size();
}

QByteArray pluginJson(const QString& id, const QString& name,
                      const QByteArray& contributes = QByteArray(),
                      const QByteArray& permissions = QByteArrayLiteral("[]"))
{
    return QStringLiteral(R"({
        "id": "%1", "name": "%2", "version": "1.2.0", "publisher": "publisherx",
        "description": "test plugin", "license": "MIT",
        "permissions": %3,
        "contributes": %4
    })").arg(id, name, QString::fromUtf8(permissions),
             QString::fromUtf8(contributes.isEmpty() ? QByteArrayLiteral("{}")
                                                     : contributes))
        .toUtf8();
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);

    auto& manager = plugins::PluginManager::instance();
    const QString pluginDir = plugins::PluginManager::userPluginDirectory();
    QDir(pluginDir).removeRecursively(); // 上次残留
    manager.reload();

    std::printf("== manifest validation ==\n");
    {
        QString error;
        const auto good = plugins::parseManifest(
            pluginJson(QStringLiteral("publisherx.good"), QStringLiteral("Good")), &error);
        expect(good.isValid() && error.isEmpty(), "valid manifest parses");

        expect(!plugins::parseManifest(QByteArrayLiteral("{"), &error).isValid(),
               "broken json rejected");
        expect(!plugins::parseManifest(
                    pluginJson(QStringLiteral("nodot"), QStringLiteral("Bad")), &error)
                    .isValid(),
               "id without publisher prefix rejected");
        expect(error.contains(QStringLiteral("publisher")),
               "rejection reason mentions the id rule");
        // publisher 与 id 前缀不一致（仿冒）
        expect(!plugins::parseManifest(
                    QStringLiteral(R"({"id":"evil.thing","name":"X","version":"1.0.0",
                                      "publisher":"someone"})").toUtf8(), &error)
                    .isValid(),
               "mismatched publisher rejected");
        expect(!plugins::parseManifest(
                    QStringLiteral(R"({"id":"publisherx.x","name":"X","version":"abc",
                                      "publisher":"publisherx"})").toUtf8(), &error)
                    .isValid(),
               "non-version string rejected");
        // shell 命令但没声明权限
        const auto noPermission = plugins::parseManifest(
            pluginJson(QStringLiteral("publisherx.shellish"), QStringLiteral("S"),
                       QByteArrayLiteral(R"({"commands":[{"id":"run","title":"Run","shell":"cmd"}]})")),
            &error);
        expect(!noPermission.isValid() && error.contains(QStringLiteral("shell")),
               "shell command without permission rejected");
        // 声明的未知权限被忽略
        const auto filtered = plugins::parseManifest(
            pluginJson(QStringLiteral("publisherx.f"), QStringLiteral("F"),
                       QByteArray(), QByteArrayLiteral(R"(["root","shell"])")),
            &error);
        expect(filtered.permissions == QStringList { QStringLiteral("shell") },
               "unknown permissions filtered out");
    }

    std::printf("== preinstalled (builtin) plugins ==\n");
    {
        // 用 HUTAOMU_PLUGIN_DIRS 注入一个"预装"根目录（模拟随包分发）
        QTemporaryDir builtinDir;
        const QString builtinRoot = builtinDir.path();
        writeFile(builtinRoot + QStringLiteral("/publisherx.builtin/plugin.json"),
                  pluginJson(QStringLiteral("publisherx.builtin"),
                             QStringLiteral("Builtin Plugin")));
        qputenv("HUTAOMU_PLUGIN_DIRS", builtinRoot.toLocal8Bit());
        manager.reload();

        const plugins::Plugin builtin
            = manager.plugin(QStringLiteral("publisherx.builtin"));
        expect(builtin.manifest.id == QStringLiteral("publisherx.builtin"),
               "preinstalled plugin discovered from the builtin root");
        expect(builtin.builtin, "discovered plugin marked as preinstalled");
        expect(manager.isBuiltin(QStringLiteral("publisherx.builtin")),
               "isBuiltin() reports preinstalled");

        QString error;
        expect(!manager.uninstall(QStringLiteral("publisherx.builtin"), &error),
               "preinstalled plugin cannot be uninstalled");
        expect(error.contains(QStringLiteral("\u9884\u88c5")),
               "refusal explains it is preinstalled");
        expect(manager.setEnabled(QStringLiteral("publisherx.builtin"), false, &error),
               "preinstalled plugin can be disabled");
        expect(!manager.plugin(QStringLiteral("publisherx.builtin")).enabled,
               "disabled state applies to the preinstalled plugin");
        manager.setEnabled(QStringLiteral("publisherx.builtin"), true, &error);

        // 用户目录里的同名插件覆盖预装副本（便于更新）
        const QString userPluginRoot = plugins::PluginManager::userPluginDirectory();
        expect(writeFile(userPluginRoot
                             + QStringLiteral("/publisherx.builtin/plugin.json"),
                         pluginJson(QStringLiteral("publisherx.builtin"),
                                    QStringLiteral("Builtin Plugin (user copy)"))),
               "user copy written");
        manager.reload();
        const plugins::Plugin after
            = manager.plugin(QStringLiteral("publisherx.builtin"));
        expect(!after.builtin
                   && after.manifest.name
                          == QStringLiteral("Builtin Plugin (user copy)"),
               "user copy overrides the preinstalled one");
        QDir(userPluginRoot + QStringLiteral("/publisherx.builtin")).removeRecursively();

        qunsetenv("HUTAOMU_PLUGIN_DIRS");
        manager.reload();
        expect(!manager.exists(QStringLiteral("publisherx.builtin")),
               "plugin disappears once the builtin root is gone");
    }

    std::printf("== install from .htmed (zip) ==\n");
    {
        QTemporaryDir tmp;
        const QString package = tmp.filePath(QStringLiteral("demo.htmed"));
        const QByteArray contributes = QByteArrayLiteral(R"({
            "formats": [{"extension": "fixtureext", "language": "plaintext"}],
            "commands": [
                {"id": "wrap", "title": "切换换行", "action": "view.toggleWordWrap"},
                {"id": "reveal", "title": "显示文件", "shell": "explorer",
                 "args": "/select,{file}"}
            ],
            "themes": ["themes/mine"]
        })");
        core::ZipWriter writer(package);
        expect(writer.addFile(QStringLiteral("publisherx.zipped2/plugin.json"),
                              pluginJson(QStringLiteral("publisherx.zipped2"),
                                         QStringLiteral("Zipped2"), contributes,
                                         QByteArrayLiteral(R"(["shell"])"))),
               "zip: manifest added");
        expect(writer.addFile(QStringLiteral("publisherx.zipped2/themes/mine/theme.json"),
                              QByteArrayLiteral(R"({
                                  "id": "mine", "name": "Mine", "dark": false,
                                  "colors": { "accent": "#123456",
                                              "editorBg": "#ffffff",
                                              "editorFg": "#111111" },
                                  "metrics": { "treeRowHeight": 21 }
                              })")),
               "zip: theme added");
        expect(writer.close(), "zip closed");

        QString error;
        const auto peeked = plugins::PluginManager::peekManifest(package, &error);
        expect(peeked.isValid() && peeked.id == QStringLiteral("publisherx.zipped2"),
               "peek reads manifest without installing");

        const auto outcome = manager.installPackage(package);
        expect(outcome.ok, qPrintable(QStringLiteral("install ok: ") + outcome.error));
        expect(QFileInfo::exists(pluginDir
                                 + QStringLiteral("/publisherx.zipped2/plugin.json")),
               "plugin unpacked into the user plugin directory");
        expect(QFileInfo::exists(pluginDir
                                 + QStringLiteral("/publisherx.zipped2/themes/mine/theme.json")),
               "nested theme file unpacked");

        // 覆盖安装
        const auto again = manager.installPackage(package);
        expect(again.ok && again.overwritten, "reinstall overwrites");
    }

    std::printf("== contributions wired up ==\n");
    {
        // 主题：插件目录里的主题进入主题列表，并标记来源插件
        const QList<editor::ThemeDefinition> themes = editor::ThemeManager::availableThemes();
        bool found = false;
        for (const editor::ThemeDefinition& theme : themes) {
            if (theme.id != QStringLiteral("mine"))
                continue;
            found = true;
            expect(theme.sourcePluginId == QStringLiteral("publisherx.zipped2"),
                   "plugin theme tagged with its source plugin");
        }
        expect(found, "plugin theme appears in the theme list");

        editor::ThemeManager::applyTheme(QStringLiteral("mine"));
        expect(editor::ThemeManager::metricInt(QStringLiteral("treeRowHeight")) == 21,
               "plugin theme applies like any other theme");

        // 插件主题不能单独删除
        QString error;
        expect(!editor::ThemeManager::uninstallTheme(QStringLiteral("mine"), &error),
               "plugin-provided theme cannot be deleted on its own");

        // 格式：*.log 认领成功。映射由上层推给语言注册表（与 MainWindow 同路径）
        syntax::LanguageRegistry::instance().setExtensionOverrides(
            manager.formatsMap());
        const syntax::LanguageSpec* spec = syntax::LanguageRegistry::instance().detect(
            QStringLiteral("/tmp/x.fixtureext"), QString());
        expect(spec != nullptr, "plugin format registers an extension");

        // 命令：内置动作与外部命令都能解析出来
        const auto commands = manager.commands();
        QStringList ids;
        for (const auto& entry : commands)
            ids.append(entry.qualifiedId);
        expect(ids.contains(QStringLiteral("publisherx.zipped2.wrap")),
               "builtin-action command listed");
        expect(ids.contains(QStringLiteral("publisherx.zipped2.reveal")),
               "shell command listed");
    }

    std::printf("== enable / disable / trust / uninstall ==\n");
    {
        QString error;
        expect(manager.setEnabled(QStringLiteral("publisherx.zipped2"), false, &error),
               "disable plugin");
        bool themeGone = true;
        for (const editor::ThemeDefinition& theme : editor::ThemeManager::availableThemes()) {
            if (theme.id == QStringLiteral("mine"))
                themeGone = false;
        }
        expect(themeGone, "disabled plugin stops contributing themes");
        syntax::LanguageRegistry::instance().setExtensionOverrides(
            manager.formatsMap());
        const syntax::LanguageSpec* afterDisable
            = syntax::LanguageRegistry::instance().detect(
                QStringLiteral("/tmp/x.fixtureext"), QString());
        expect(afterDisable == nullptr,
               "disabled plugin stops claiming the extension (detection falls through)");
        manager.setEnabled(QStringLiteral("publisherx.zipped2"), true, &error);
        syntax::LanguageRegistry::instance().setExtensionOverrides(
            manager.formatsMap());

        expect(manager.plugin(QStringLiteral("publisherx.zipped2")).enabled,
               "re-enable plugin");

        manager.setShellTrusted(QStringLiteral("publisherx.zipped2"), true);
        expect(manager.isShellTrusted(QStringLiteral("publisherx.zipped2")),
               "shell trust recorded");
        manager.reload();
        expect(manager.isShellTrusted(QStringLiteral("publisherx.zipped2")),
               "shell trust survives reload (persisted)");

        expect(plugins::PluginManager::expandShellArgs(
                   QStringLiteral("/select,{file}"), QStringLiteral("C:/a b/c.txt"),
                   QStringLiteral("C:/a b"), QStringLiteral("C:/ws"))
                   == QStringLiteral("/select,C:/a b/c.txt"),
               "shell placeholder expansion");

        // statusBar / pages 贡献点（P4b）
        const auto withPanels = plugins::parseManifest(
            pluginJson(QStringLiteral("publisherx.panels"), QStringLiteral("Panels"),
                       QByteArrayLiteral(R"({
                           "statusBar": [
                               {"id": "words", "type": "wordCount", "alignment": "right"},
                               {"id": "why", "type": "unknownType"}
                           ],
                           "pages": [
                               {"id": "guide", "title": "说明", "type": "markdown",
                                "content": "README.md", "icon": "assets/icons/page.svg"},
                               {"id": "bad", "title": "Bad", "type": "webgl"}
                           ]
                       })")),
            &error);
        expect(withPanels.isValid(), "manifest with statusBar/pages parses");
        expect(withPanels.statusItems.size() == 1,
               "unknown status type ignored, known one kept");
        expect(withPanels.statusItems.first().alignment == QStringLiteral("right"),
               "status alignment parsed");
        expect(withPanels.pages.size() == 1 && withPanels.pages.first().id
                   == QStringLiteral("guide"),
               "unknown page type ignored, markdown page kept");
        expect(withPanels.pages.first().defaultSide == QStringLiteral("left"),
               "page default side defaults to left");

        expect(manager.uninstall(QStringLiteral("publisherx.zipped2"), &error),
               "uninstall plugin");
        expect(!manager.exists(QStringLiteral("publisherx.zipped2")),
               "plugin gone after uninstall");
        expect(!manager.isShellTrusted(QStringLiteral("publisherx.zipped2")),
               "trust cleared on uninstall");
    }

    std::printf("== reject unsafe packages ==\n");
    {
        QTemporaryDir tmp;
        const QString evil = tmp.filePath(QStringLiteral("evil.htmed"));
        core::ZipWriter writer(evil);
        writer.addFile(QStringLiteral("publisherx.evil/plugin.json"),
                       pluginJson(QStringLiteral("publisherx.evil"),
                                  QStringLiteral("Evil")));
        writer.addFile(QStringLiteral("publisherx.evil/../escape.txt"),
                       QByteArrayLiteral("pwned"));
        writer.close();
        const auto outcome = manager.installPackage(evil);
        expect(!outcome.ok, "traversal package rejected");
        expect(!QFileInfo::exists(pluginDir + QStringLiteral("/escape.txt")),
               "nothing written outside the plugin directory");

        const QString noManifest = tmp.filePath(QStringLiteral("empty.htmed"));
        core::ZipWriter writer2(noManifest);
        writer2.addFile(QStringLiteral("readme.txt"), QByteArrayLiteral("hi"));
        writer2.close();
        QString error;
        expect(!plugins::PluginManager::peekManifest(noManifest, &error).isValid(),
               "package without plugin.json rejected");
    }

    // 收尾：清掉测试插件
    QDir(pluginDir).removeRecursively();
    manager.reload();

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll plugin tests passed.\n");
    return 0;
}
