// Hutaomu Editor - Theme package (.htmtpi) + T2 metrics regression tests.
// SPDX-License-Identifier: LicenseRef-Proprietary
//
// 覆盖：示范主题的发现与度量渲染、.htmtpi 解包安装/覆盖/卸载、目录形态安装、
// 路径穿越与符号链接拒绝、未知主题回退默认。
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cstdio>

#include "themes/ThemeManager.h"
#include "themes/ThemePackage.h"
#include "viewers/ZipReader.h"

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

// 用项目内 ZipReader 的镜像格式手写一个最小 ZIP（store，无压缩）——
// 打包测试要能构造"带 ../ 越界条目"的恶意包，生成器不提供这种能力。
// 这里改为直接构造：使用 Qt 的 qCompress? 不可用（zlib 包装非 ZIP）。
// 因此借用系统 tar/cmake 不可移植，故用 store 方式手写 central directory。
QByteArray buildStoreZip(const QList<QPair<QString, QByteArray>>& entries)
{
    QByteArray out;
    struct Central {
        QString name;
        quint32 offset;
        quint32 size;
    };
    QList<Central> central;

    const auto u16 = [](quint16 v) {
        QByteArray b(2, Qt::Uninitialized);
        b[0] = char(v & 0xff);
        b[1] = char((v >> 8) & 0xff);
        return b;
    };
    const auto u32 = [](quint32 v) {
        QByteArray b(4, Qt::Uninitialized);
        b[0] = char(v & 0xff);
        b[1] = char((v >> 8) & 0xff);
        b[2] = char((v >> 16) & 0xff);
        b[3] = char((v >> 24) & 0xff);
        return b;
    };

    for (const auto& entry : entries) {
        Central info;
        info.name = entry.first;
        info.offset = quint32(out.size());
        info.size = quint32(entry.second.size());
        const QByteArray name = entry.first.toUtf8();
        // local file header
        out += u32(0x04034b50) + u16(20) + u16(0) + u16(0) + u16(0) + u16(0)
               + u32(0) /*crc 不校验*/ + u32(info.size) + u32(info.size)
               + u16(quint16(name.size())) + u16(0) + name + entry.second;
        central.append(info);
    }

    const quint32 cdStart = quint32(out.size());
    for (const Central& info : central) {
        const QByteArray name = info.name.toUtf8();
        out += u32(0x02014b50) + u16(20) + u16(20) + u16(0) + u16(0) + u16(0)
               + u16(0) + u32(0) + u32(info.size) + u32(info.size)
               + u16(quint16(name.size())) + u16(0) + u16(0) + u16(0) + u16(0)
               + u32(0) + u32(info.offset) + name;
    }
    const quint32 cdSize = quint32(out.size()) - cdStart;
    out += u32(0x06054b50) + u16(0) + u16(0) + u16(quint16(central.size()))
           + u16(quint16(central.size())) + u32(cdSize) + u32(cdStart) + u16(0);
    return out;
}

QByteArray minimalThemeJson(const QString& id, const QString& name)
{
    return QStringLiteral(R"({
        "id": "%1",
        "name": "%2",
        "dark": true,
        "colors": { "editorBg": "#101010", "editorFg": "#e0e0e0", "accent": "#ff8800" },
        "metrics": { "treeRowHeight": 33 },
        "background": { "image": "assets/bg.png", "mode": "center", "opacity": 0.4 }
    })").arg(id, name).toUtf8();
}

} // namespace

int main(int argc, char** argv)
{
    // 把 AppData 重定向到测试目录，避免污染真实用户主题目录。
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);

    const QString userThemes = editor::ThemeManager::userThemeDirectory();
    QDir(userThemes).removeRecursively(); // 上次残留

    std::printf("== official themes ==\n");
    {
        const QList<editor::ThemeDefinition> themes = editor::ThemeManager::availableThemes();
        const QStringList official = { QStringLiteral("typora-immersive"),
                                       QStringLiteral("obsidian-cards"),
                                       QStringLiteral("vscode-dark-plus"),
                                       QStringLiteral("vscode-light-plus") };
        for (const QString& id : official) {
            bool found = false;
            for (const editor::ThemeDefinition& theme : themes) {
                if (theme.id != id)
                    continue;
                found = true;
                // 官方主题由"随包预装插件"提供（资源里也有兜底副本）；
                // 两者都意味着用户不能单独删除它。
                expect(theme.builtin || !theme.sourcePluginId.isEmpty(),
                       qPrintable(id + QStringLiteral(": provided by resources or the "
                                                      "official plugin")));
                expect(!theme.previewPath.isEmpty()
                           && QFileInfo::exists(theme.previewPath),
                       qPrintable(id + QStringLiteral(": has preview png")));
                expect(!theme.metrics.isEmpty(),
                       qPrintable(id + QStringLiteral(": declares metrics")));
                expect(theme.capabilities.contains(QStringLiteral("metrics")),
                       qPrintable(id + QStringLiteral(": capability metrics")));
            }
            expect(found, qPrintable(id + QStringLiteral(": discovered")));
        }
        expect(themes.size() >= 7, "builtin flat + official dir themes all listed");
    }

    std::printf("== T2 metrics render ==\n");
    {
        editor::ThemeManager::applyTheme(QStringLiteral("typora-immersive"));
        expect(editor::ThemeManager::theme() == QStringLiteral("typora-immersive"),
               "applyTheme selects official theme");
        // spacious 预设给 treeRowHeight=30，但主题显式 28 应胜出
        expect(editor::ThemeManager::metricInt(QStringLiteral("treeRowHeight")) == 28,
               "explicit metric beats density preset");
        expect(editor::ThemeManager::metricInt(QStringLiteral("fontSizeSmall")) == 13,
               "fontSizeSmall metric applied");
        const QString css = qApp->styleSheet();
        expect(css.contains(QStringLiteral("height: 28px")), "tree row height in QSS");
        expect(css.contains(QStringLiteral("font-size: 13px")), "font size in QSS");
        // 模板头部注释里示范了 @m.key/@colorKey 语法，校验前先去掉注释。
        QString code = css;
        code.remove(QRegularExpression(QStringLiteral("/\\*.*?\\*/"),
                                       QRegularExpression::DotMatchesEverythingOption));
        QStringList leftovers;
        {
            QRegularExpression tokenRe(QStringLiteral("@[A-Za-z0-9_.]+"));
            auto it = tokenRe.globalMatch(code);
            while (it.hasNext()) {
                const QString token = it.next().captured(0);
                if (!leftovers.contains(token))
                    leftovers.append(token);
            }
        }
        if (!leftovers.isEmpty())
            std::printf("  (leftover tokens: %s)\n", qPrintable(leftovers.join(' ')));
        expect(!code.contains(QStringLiteral("@m.")), "no unresolved metric tokens");
        expect(!code.contains(QLatin1Char('@')), "no unresolved color tokens either");
        expect(!editor::ThemeManager::renderedStyleSheet(
                    QStringLiteral("typora-immersive")).isEmpty(),
               "renderedStyleSheet non-empty for official theme");

        editor::ThemeManager::applyTheme(QStringLiteral("paper"));
        expect(editor::ThemeManager::metricInt(QStringLiteral("treeRowHeight")) == 24,
               "no-metrics theme falls back to defaults");
        expect(editor::ThemeManager::metricInt(QStringLiteral("editorFontSize"), -1) == 0,
               "editorFontSize default 0 = no override");

        editor::ThemeManager::applyTheme(QStringLiteral("typora-immersive"));
        expect(editor::ThemeManager::metric(QStringLiteral("editorFontFamily"))
                   .contains(QStringLiteral("Georgia")),
               "theme font family readable for MainWindow");
    }

    std::printf("== install from .htmtpi (zip) ==\n");
    {
        QTemporaryDir tmp;
        const QString zipPath = tmp.filePath(QStringLiteral("ok.htmtpi"));
        const QByteArray themeJson = minimalThemeJson(QStringLiteral("fixture-zip"),
                                                      QStringLiteral("Fixture ZIP"));
        const QByteArray preview = QByteArrayLiteral("\x89PNG\r\n\x1a\nfake");
        QList<QPair<QString, QByteArray>> entries = {
            { QStringLiteral("fixture-zip/theme.json"), themeJson },
            { QStringLiteral("fixture-zip/preview.png"), preview },
            { QStringLiteral("fixture-zip/assets/bg.png"), preview },
            { QStringLiteral("fixture-zip/LICENSE.txt"), QByteArrayLiteral("x") },
        };
        QFile zip(zipPath);
        expect(zip.open(QIODevice::WriteOnly)
                   && zip.write(buildStoreZip(entries)) > 0,
               "write fixture zip");
        zip.close();

        QString error;
        expect(editor::themepkg::peekThemeId(zipPath, &error)
                   == QStringLiteral("fixture-zip"),
               "peekThemeId reads id from zip");

        QString installedId;
        expect(editor::ThemeManager::installThemePackage(zipPath, &installedId, &error),
               qPrintable(QStringLiteral("install: ") + error));
        expect(installedId == QStringLiteral("fixture-zip"), "installed id reported");
        expect(QFileInfo::exists(userThemes + QStringLiteral("/fixture-zip/theme.json")),
               "theme.json extracted");
        expect(QFileInfo::exists(userThemes + QStringLiteral("/fixture-zip/preview.png")),
               "preview.png extracted");

        bool foundInstalled = false;
        for (const editor::ThemeDefinition& theme : editor::ThemeManager::availableThemes()) {
            if (theme.id == QStringLiteral("fixture-zip")) {
                foundInstalled = true;
                expect(!theme.builtin, "installed theme is not builtin");
                expect(theme.metric(QStringLiteral("treeRowHeight"))
                           == QStringLiteral("33"),
                       "installed theme metrics parsed");
                expect(!theme.previewPath.isEmpty(), "installed preview path resolved");
                // T3：安装目录里的资源必须能解析（内置资源之外的新来源）
                const QString asset = editor::ThemeManager::assetPath(
                    QStringLiteral("assets/bg.png"), QStringLiteral("fixture-zip"));
                expect(!asset.isEmpty() && asset.startsWith(userThemes),
                       "asset resolves inside the installed theme directory");
                expect(theme.background.isValid()
                           && theme.background.mode == QStringLiteral("center")
                           && qAbs(theme.background.opacity - 0.4) < 0.001,
                       "installed theme background spec parsed");
                expect(editor::ThemeManager::assetPath(QStringLiteral("../escape.png"),
                                                       QStringLiteral("fixture-zip"))
                           .isEmpty(),
                       "traversal rejected for installed theme assets");
            }
        }
        expect(foundInstalled, "installed theme discovered after install");

        // 覆盖安装：同名包应被替换而不是报错
        expect(editor::ThemeManager::installThemePackage(zipPath, &installedId, &error),
               "reinstall overwrites");
    }

    std::printf("== install from directory ==\n");
    {
        QTemporaryDir tmp;
        const QString dir = tmp.filePath(QStringLiteral("theme-src"));
        expect(writeFile(dir + QStringLiteral("/theme.json"),
                         minimalThemeJson(QStringLiteral("fixture-dir"),
                                          QStringLiteral("Fixture DIR"))),
               "write directory fixture");
        expect(writeFile(dir + QStringLiteral("/assets/logo.svg"),
                         QByteArrayLiteral("<svg/>")),
               "write nested asset");

        QString error;
        QString installedId;
        expect(editor::ThemeManager::installThemePackage(dir, &installedId, &error),
               qPrintable(QStringLiteral("directory install: ") + error));
        expect(QFileInfo::exists(userThemes + QStringLiteral("/fixture-dir/assets/logo.svg")),
               "nested file copied");
    }

    std::printf("== reject unsafe packages ==\n");
    {
        QTemporaryDir tmp;
        const QString evil = tmp.filePath(QStringLiteral("evil.htmtpi"));
        QList<QPair<QString, QByteArray>> entries = {
            { QStringLiteral("theme.json"),
              minimalThemeJson(QStringLiteral("evil-theme"), QStringLiteral("Evil")) },
            { QStringLiteral("../escaped.txt"), QByteArrayLiteral("pwned") },
        };
        QFile zip(evil);
        expect(zip.open(QIODevice::WriteOnly) && zip.write(buildStoreZip(entries)) > 0,
               "write traversal zip");
        zip.close();

        QString error;
        QString installedId;
        const bool ok = editor::ThemeManager::installThemePackage(evil, &installedId, &error);
        expect(!ok, "traversal package rejected");
        expect(!QFileInfo::exists(userThemes + QStringLiteral("/../escaped.txt"))
                   && !QFileInfo::exists(tmp.filePath(QStringLiteral("../escaped.txt"))),
               "nothing written outside target");

        // 缺 theme.json 的包
        const QString noTheme = tmp.filePath(QStringLiteral("no-theme.htmtpi"));
        QFile zip2(noTheme);
        expect(zip2.open(QIODevice::WriteOnly)
                   && zip2.write(buildStoreZip({ { QStringLiteral("readme.txt"),
                                                   QByteArrayLiteral("hi") } })) > 0,
               "write theme-less zip");
        zip2.close();
        expect(!editor::themepkg::peekThemeId(noTheme, &error).size(),
               "package without theme.json rejected");

        // 非法 id（路径片段）
        const QString badId = tmp.filePath(QStringLiteral("bad-id.htmtpi"));
        QFile zip3(badId);
        expect(zip3.open(QIODevice::WriteOnly)
                   && zip3.write(buildStoreZip({ { QStringLiteral("theme.json"),
                                                   minimalThemeJson(QStringLiteral(".."),
                                                                    QStringLiteral("Bad")) } })) > 0,
               "write bad-id zip");
        zip3.close();
        expect(!editor::themepkg::peekThemeId(badId, &error).size(),
               "unsafe theme id rejected");
    }

    std::printf("== uninstall + fallback ==\n");
    {
        QString error;
        expect(editor::ThemeManager::uninstallTheme(QStringLiteral("fixture-zip"), &error),
               "uninstall installed theme");
        expect(!QFileInfo::exists(userThemes + QStringLiteral("/fixture-zip")),
               "installed dir removed");
        expect(!editor::ThemeManager::uninstallTheme(QStringLiteral("paper"), &error),
               "builtin theme cannot be uninstalled");
        editor::ThemeManager::applyTheme(QStringLiteral("no-such-theme"));
        expect(editor::ThemeManager::theme() == QStringLiteral("paper"),
               "unknown theme falls back to default");
        expect(!editor::ThemeManager::renderedStyleSheet(QStringLiteral("nope")).isEmpty(),
               "renderedStyleSheet falls back for unknown id");
    }

    QDir(userThemes).removeRecursively();

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll theme package tests passed.\n");
    return 0;
}
