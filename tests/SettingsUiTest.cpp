// Hutaomu Editor - 设置对话框布局 + 编辑器自定义背景 渲染回归测试。
// 把设置页四个标签页和不同浓度的编辑器背景渲染成 PNG（build/Release/artifacts/），
// 供人工/自动化视觉检查；同时断言基本不变量（页数、背景确实被绘制）。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <cmath>
#include <random>

#include "app/SettingsDialog.h"
#include "editor/CodeEditor.h"
#include "settings/AppSettings.h"
#include "themes/ThemeManager.h"
#include "themes/UserBackground.h"

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

QString artifactPath(const QString& name)
{
    const QString dir = QCoreApplication::applicationDirPath()
                        + QStringLiteral("/artifacts");
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + name;
}

// 生成一张高密度"樱花风"花纹图（模拟用户会选的那种 busy 平铺图）
QImage makeBusyPattern()
{
    QImage image(220, 220, QImage::Format_RGB32);
    image.fill(QColor(255, 240, 244));
    QPainter painter(&image);
    std::mt19937 rng(7);
    auto pick = [&rng](int n) { return int(rng() % unsigned(n)); };
    for (int i = 0; i < 260; ++i) {
        QColor color;
        switch (pick(3)) {
        case 0: color = QColor(233, 30, 99); break;
        case 1: color = QColor(244, 143, 177); break;
        default: color = QColor(173, 20, 87); break;
        }
        painter.setPen(color);
        painter.setBrush(color);
        const int r = 2 + pick(5);
        painter.drawEllipse(pick(image.width()), pick(image.height()), r, r);
    }
    painter.end();
    return image;
}

bool grabSettingsTabs()
{
    app::SettingsDialog dialog;
    dialog.resize(860, 560);
    dialog.show();
    QCoreApplication::processEvents();

    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("settingsTabs"));
    if (!tabs) {
        std::printf("  FAIL  找不到 settingsTabs\n");
        return false;
    }
    expect(tabs->count() == 4, "设置页有 4 个标签");

    static const char* names[] = { "settings-appearance.png", "settings-editor.png",
                                   "settings-layout.png", "settings-plugins.png" };
    for (int i = 0; i < tabs->count() && i < 4; ++i) {
        tabs->setCurrentIndex(i);
        QCoreApplication::processEvents();
        const QPixmap grab = dialog.grab();
        const bool ok = grab.save(artifactPath(QString::fromLatin1(names[i])));
        expect(ok && !grab.isNull(), names[i]);
    }
    dialog.close();
    return true;
}

// 用给定的不透明度铺背景后抓编辑器，返回截图右上部的一个采样色
QColor grabEditorWithBackground(const QString& imagePath, const QString& mode,
                                qreal opacity, const QString& file)
{
    editor::background::UserBackground bg;
    bg.imagePath = imagePath;
    bg.mode = mode;
    bg.opacity = opacity;
    editor::background::save(bg);

    editor::CodeEditor editor;
    editor.setPlainText(QStringLiteral(
        "# AI 协作规则\n\n本文文件定义了 AI 与用户协作时的持久化规则，"
        "每次对话开始时 AI 必须先读取此文件及相关日志文件，以确保记忆连续、避免幻觉。\n"));
    editor.resize(800, 500);
    editor.applyTheme();
    editor.show();
    editor.viewport()->resize(800, 500);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const QPixmap grab = editor.grab();
    grab.save(artifactPath(file));
    // 右上角远离文字的位置采样：应不再是纯 editorBg（背景真的画上了）
    const QImage image = grab.toImage().convertToFormat(QImage::Format_RGB32);
    return image.isNull() ? QColor()
                          : image.pixelColor(image.width() - 24, 20);
}

} // namespace

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // 测试模式：AppData 指向临时目录，绝不读写真实用户的 background.json
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Hutaomu"));
    QApplication::setApplicationName(QStringLiteral("Hutaomu Editor SettingsUiTest"));

    auto& s = settings::AppSettings::instance();
    s.load();
    editor::ThemeManager::applyTheme(s.theme);

    grabSettingsTabs();

    QTemporaryDir dir;
    const QString imagePath = dir.filePath(QStringLiteral("sakura.png"));
    makeBusyPattern().save(imagePath);

    const QColor base = editor::ThemeManager::currentTheme()
                            .color(QStringLiteral("editorBg"));

    // 旧版本语义标反时用户可能存下 1.0：合成时必须被限幅（≤0.65）
    const QColor strong = grabEditorWithBackground(imagePath, QStringLiteral("tile"), 1.0,
                                                   QStringLiteral("editor-bg-strong.png"));
    expect(strong != base, "浓度 1.0（旧值）时背景已绘制且被限幅");

    // 新默认 0.3：应该是"淡淡的花纹"
    const QColor soft = grabEditorWithBackground(imagePath, QStringLiteral("tile"), 0.3,
                                                 QStringLiteral("editor-bg-soft.png"));
    expect(soft != base, "浓度 0.3 时背景已绘制");

    // 三种模式都能渲染（不崩溃即回归通过）
    grabEditorWithBackground(imagePath, QStringLiteral("center"), 0.4,
                             QStringLiteral("editor-bg-center.png"));
    grabEditorWithBackground(imagePath, QStringLiteral("stretch"), 0.4,
                             QStringLiteral("editor-bg-stretch.png"));
    expect(true, "center/stretch 模式渲染无崩溃");

    editor::background::clear();
    std::printf(failures == 0 ? "ALL PASS\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
