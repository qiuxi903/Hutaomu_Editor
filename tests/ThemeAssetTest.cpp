// Hutaomu Editor - Theme asset overrides (T3) regression tests.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
//
// 覆盖：主题包资源路径解析与越界拒绝、logo/图标覆盖生效且随主题切换、
// 背景图规格解析（mode/opacity/路径校验）、背景图确实渲染进编辑器视口、
// 无背景图主题保持纯色底色。
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPalette>
#include <QPlainTextEdit>
#include <QSet>
#include <QTemporaryDir>
#include <QTextCursor>
#include <cstdio>

#include "editor/CodeEditor.h"
#include "themes/IconLoader.h"
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

void pump()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
}

QImage pixmapImage(const QPixmap& pixmap)
{
    return pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
}

bool sameImage(const QPixmap& a, const QPixmap& b)
{
    if (a.isNull() || b.isNull())
        return false;
    return pixmapImage(a) == pixmapImage(b);
}

// 视口右下角（最后一行文字之下）的采样区域里有多少种颜色：
// 纯色底 → 1 种；叠了颗粒/点阵纹理 → 明显更多。
int backgroundColorVariety(const QPixmap& shot, const QRect& region)
{
    const QImage image = shot.toImage();
    QSet<QRgb> colors;
    for (int y = region.top(); y < region.bottom(); y += 2) {
        for (int x = region.left(); x < region.right(); x += 2) {
            if (y < 0 || x < 0 || y >= image.height() || x >= image.width())
                continue;
            colors.insert(image.pixel(x, y));
        }
    }
    return colors.size();
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::printf("== asset path resolution ==\n");
    {
        editor::ThemeManager::applyTheme(QStringLiteral("typora-immersive"));
        const QString brand = editor::ThemeManager::assetPath(
            QStringLiteral("assets/icons/brand.svg"));
        expect(!brand.isEmpty() && QFile::exists(brand),
               "theme asset resolves for theme with assets");
        const QString background = editor::ThemeManager::assetPath(
            QStringLiteral("assets/background.png"));
        expect(!background.isEmpty() && QFile::exists(background),
               "background image resolves");

        editor::ThemeManager::applyTheme(QStringLiteral("paper"));
        expect(editor::ThemeManager::assetPath(QStringLiteral("assets/icons/brand.svg"))
                   .isEmpty(),
               "theme without assets resolves to empty");
        expect(editor::ThemeManager::assetPath(QStringLiteral("../../secrets.txt"))
                   .isEmpty(),
               "path traversal rejected");
        expect(editor::ThemeManager::assetPath(QStringLiteral("/etc/passwd")).isEmpty(),
               "absolute path rejected");
        expect(editor::ThemeManager::assetPath(QStringLiteral("assets/icons/brand.svg"),
                                               QStringLiteral("typora-immersive"))
                   .size()
               > 0,
               "asset query works for an arbitrary theme id");
    }

    std::printf("== background spec ==\n");
    {
        const editor::ThemeDefinition typora = editor::ThemeManager::themeDefinition(
            QStringLiteral("typora-immersive"));
        expect(typora.background.isValid(), "background declared");
        expect(typora.background.mode == QStringLiteral("tile"), "mode parsed");
        expect(qAbs(typora.background.opacity - 0.55) < 0.001, "opacity parsed");
        expect(typora.capabilities.contains(QStringLiteral("assets")),
               "assets capability inferred from background");

        const editor::ThemeDefinition paper = editor::ThemeManager::themeDefinition(
            QStringLiteral("paper"));
        expect(!paper.background.isValid(), "no background for plain theme");
        expect(!paper.capabilities.contains(QStringLiteral("assets")),
               "no assets capability without assets");

        const editor::ThemeDefinition vscode = editor::ThemeManager::themeDefinition(
            QStringLiteral("vscode-dark-plus"));
        expect(!vscode.background.isValid(), "official theme without background ok");
    }

    std::printf("== icon override ==\n");
    {
        editor::ThemeManager::applyTheme(QStringLiteral("paper"));
        const QColor fill = editor::ThemeManager::tokens().accent;
        const QColor panel = editor::ThemeManager::tokens().titleBarBackground;
        const QPixmap builtin = editor::IconLoader::load(QStringLiteral("brand"), fill,
                                                       panel, 18);
        expect(!builtin.isNull(), "builtin brand icon loads");

        editor::ThemeManager::applyTheme(QStringLiteral("typora-immersive"));
        const QPixmap themed = editor::IconLoader::load(QStringLiteral("brand"),
                                                       editor::ThemeManager::tokens().accent,
                                                       panel, 18);
        expect(!themed.isNull(), "themed brand icon loads");
        expect(!sameImage(builtin, themed), "theme overrides the brand icon");

        // 切回无覆盖主题：必须重新拿到内置图标（缓存键含主题 id）
        editor::ThemeManager::applyTheme(QStringLiteral("paper"));
        const QPixmap again = editor::IconLoader::load(QStringLiteral("brand"), fill, panel,
                                                      18);
        expect(sameImage(builtin, again),
               "switching back restores builtin icon (cache is theme-aware)");

        // 图标覆盖缺失时回退内置：给没有覆盖的主题请求 activity-explorer
        expect(!editor::IconLoader::load(QStringLiteral("activity-explorer"), fill, panel, 16)
                    .isNull(),
               "icons without override still load from builtin");
    }

    std::printf("== background rendering ==\n");
    {
        editor::ThemeManager::applyTheme(QStringLiteral("paper"));
        editor::CodeEditor editor;
        editor.setPlainText(QStringLiteral("line one\nline two\n"));
        editor.resize(360, 240);
        editor.show();
        pump();
        const QPixmap plainShot = editor.grab();
        const QRect sampleRegion(20, 150, 300, 200); // 文字之下的空白区
        const int plainVariety = backgroundColorVariety(plainShot, sampleRegion);
        expect(plainVariety <= 2, "plain theme paints a uniform editor background");
        expect(editor.palette().brush(QPalette::Base).style() == Qt::SolidPattern,
               "plain theme uses solid palette base");

        editor::ThemeManager::applyTheme(QStringLiteral("typora-immersive"));
        editor.applyTheme();
        pump();
        const QPixmap texturedShot = editor.grab();
        const int texturedVariety = backgroundColorVariety(texturedShot, sampleRegion);
        if (texturedVariety <= plainVariety + 2)
            std::printf("  (variety plain=%d textured=%d)\n", plainVariety,
                        texturedVariety);
        expect(texturedVariety > plainVariety + 2,
               "theme background image produces texture in the viewport");
        expect(editor.palette().brush(QPalette::Base).style() == Qt::TexturePattern,
               "background image installs a palette texture brush");

        // 背景图随主题撤销：回到纯色
        editor::ThemeManager::applyTheme(QStringLiteral("paper"));
        editor.applyTheme();
        pump();
        expect(editor.palette().brush(QPalette::Base).style() == Qt::SolidPattern,
               "leaving the themed theme removes the background texture");
    }

    std::printf("== editor still readable & themed text color ==\n");
    {
        editor::ThemeManager::applyTheme(QStringLiteral("typora-immersive"));
        editor::CodeEditor editor;
        editor.setPlainText(QStringLiteral("readable text"));
        editor.resize(300, 120);
        editor.show();
        pump();
        expect(editor.palette().color(QPalette::Text)
                   == editor::ThemeManager::tokens().editorForeground,
               "text color still comes from the theme");
    }

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll theme asset tests passed.\n");
    return 0;
}
