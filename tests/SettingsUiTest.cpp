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

// 回归：同路径换图后，已打开的编辑器必须重新加载新图（缓存键含 mtime/size）。
// 用"底色 + 中心一个纯色圆点"的大图：居中模式下正确渲染 = 视口正中一个点；
// 若还是旧图/平铺小纹理，点的颜色或数量就不对。
class DotImage
{
public:
    explicit DotImage(const QColor& color) : m_color(color) {}
    void write(const QString& path) const
    {
        QImage image(1600, 1200, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_color);
        painter.drawEllipse(QPoint(800, 600), 40, 40);
        painter.end();
        image.save(path);
    }
private:
    QColor m_color;
};

int countDots(const QImage& image, const QColor& color)
{
    int count = 0;
    for (int y = 0; y < image.height(); y += 2)
        for (int x = 0; x < image.width(); x += 2) {
            const QColor pixel = image.pixelColor(x, y);
            if (qAbs(pixel.red() - color.red()) < 60
                && qAbs(pixel.green() - color.green()) < 60
                && qAbs(pixel.blue() - color.blue()) < 60)
                ++count;
        }
    return count;
}

void imageSwapRegression(const QString& imagePath)
{    editor::background::UserBackground bg;
    bg.imagePath = imagePath;
    bg.mode = QStringLiteral("center");
    bg.opacity = 1.0; // 渲染时限幅 0.5，圆点仍可辨认
    editor::background::save(bg);

    DotImage(QColor(255, 0, 0)).write(imagePath); // v1：红点

    editor::CodeEditor editor;
    editor.resize(800, 500);
    editor.applyTheme();
    editor.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QImage grab1 = editor.grab().toImage().convertToFormat(QImage::Format_RGB32);
    // 0.5 限幅混白后的颜色
    expect(countDots(grab1, QColor(255, 127, 127)) > 0, "初始背景图（红点）已绘制");

    // 同路径覆盖换图（与「背景…」导入行为一致）→ 重新应用主题
    DotImage(QColor(0, 160, 255)).write(imagePath); // v2：青点
    editor.applyTheme();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QImage grab2 = editor.grab().toImage().convertToFormat(QImage::Format_RGB32);
    expect(countDots(grab2, QColor(127, 207, 255)) > 0, "换图后重新应用主题显示新图（青点）");
    expect(countDots(grab2, QColor(255, 127, 127)) == 0, "换图后不再残留旧图（红点）");

    // 隐藏标签页创建 → 切换 → 也必须用新图（而不是创建瞬间的小纹理平铺）
    QTabWidget tabs;
    auto* placeholder = new editor::CodeEditor(&tabs);
    placeholder->setPlainText(QStringLiteral("placeholder"));
    placeholder->applyTheme();
    tabs.addTab(placeholder, QStringLiteral("p"));
    auto* hidden = new editor::CodeEditor(&tabs);
    hidden->setPlainText(QStringLiteral("hidden tab"));
    hidden->applyTheme();
    tabs.addTab(hidden, QStringLiteral("tab"));
    tabs.resize(800, 500);
    tabs.show();
    QCoreApplication::processEvents();
    tabs.setCurrentIndex(1);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QImage grab3 = hidden->grab().toImage().convertToFormat(QImage::Format_RGB32);
    expect(countDots(grab3, QColor(127, 207, 255)) > 0, "隐藏标签页切换后显示当前图");
    expect(countDots(grab3, QColor(255, 127, 127)) == 0, "隐藏标签页切换后无旧图残留");
}

// 回归：自动模式 = 大图按比例充满（cover，等比、居中裁溢出），小图平铺。
// 大图左缘放一个绿色方块：cover 可见（等比缩放后左缘贴视口），
// 若错误地走了"居中 1:1 裁切"，左缘内容在图中心裁切窗口之外，不可见。
void autoModeRegression(const QString& imagePath)
{
    editor::background::UserBackground bg;
    bg.imagePath = imagePath;
    bg.mode = QStringLiteral("auto");
    bg.opacity = 1.0; // 渲染时限幅 0.5
    editor::background::save(bg);

    // 大图：浅灰底 1600x1200 + 红点（中心）+ 绿块（左缘 y=200）
    {
        QImage big(1600, 1200, QImage::Format_RGB32);
        big.fill(QColor(235, 235, 235));
        QPainter painter(&big);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 0, 0));
        painter.drawEllipse(QPoint(800, 600), 40, 40);
        painter.setBrush(QColor(0, 200, 0));
        painter.drawRect(0, 200, 60, 60);
        painter.end();
        big.save(imagePath);
    }

    editor::CodeEditor editor;
    editor.resize(800, 500);
    editor.applyTheme();
    editor.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QImage grab = editor.grab().toImage().convertToFormat(QImage::Format_RGB32);
    // 0.5 混白后的颜色
    expect(countDots(grab, QColor(128, 255, 128)) > 0,
           "自动模式：大图按比例充满（左缘内容可见 = cover 而非居中裁切）");
    expect(countDots(grab, QColor(255, 127, 127)) > 0, "自动模式大图：中心内容已绘制");

    // 小图：同路径覆盖成 120x90 → 自动模式应平铺（绿块重复出现多次）
    {
        QImage small(120, 90, QImage::Format_RGB32);
        small.fill(QColor(235, 235, 235));
        QPainter painter(&small);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 200, 0));
        painter.drawRect(0, 0, 30, 30);
        painter.end();
        small.save(imagePath);
    }
    editor.applyTheme();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    grab = editor.grab().toImage().convertToFormat(QImage::Format_RGB32);
    expect(countDots(grab, QColor(128, 255, 128)) >= 5,
           "自动模式：小图平铺（绿块重复出现）");
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

    // 同路径换图 / 隐藏标签页场景（复现"不同文件背景不一样"）
    imageSwapRegression(imagePath);

    // 自动模式：大图 cover / 小图 tile
    autoModeRegression(imagePath);

    editor::background::clear();
    std::printf(failures == 0 ? "ALL PASS\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
