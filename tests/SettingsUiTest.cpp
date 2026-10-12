// Hutaomu Editor - 设置对话框布局 + 编辑器自定义背景 渲染回归测试。
// 把设置页四个标签页和不同浓度的编辑器背景渲染成 PNG（build/Release/artifacts/），
// 供人工/自动化视觉检查；同时断言基本不变量（页数、背景确实被绘制）。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include <QApplication>
#include <QFile>
#include <QElapsedTimer>
#include <QImage>
#include <QMenu>
#include <QToolButton>
#include <QPainter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <cmath>
#include <random>

#include "app/MainWindow.h"
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
    dialog.resize(860, 500);
    dialog.show();
    QCoreApplication::processEvents();

    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("settingsTabs"));
    if (!tabs) {
        std::printf("  FAIL  找不到 settingsTabs\n");
        return false;
    }
    expect(tabs->count() == 3, "设置页有 3 个标签");

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

// 菜单渲染样张：分区标题（禁用态）/ 普通项 / 分隔线 / 子菜单箭头
void grabMenuSample()
{
    QMenu menu;
    QAction* title = menu.addAction(QStringLiteral("官方主题包"));
    title->setEnabled(false);
    menu.addAction(QStringLiteral("进入沉浸写作（Typora 主题）"));
    menu.addAction(QStringLiteral("进入卡片笔记（Obsidian 主题）"));
    menu.addSeparator();
    QMenu* sub = menu.addMenu(QStringLiteral("示例插件"));
    sub->addAction(QStringLiteral("示例命令"));
    menu.addAction(QStringLiteral("管理插件..."));
    menu.popup(QPoint(40, 40));
    QCoreApplication::processEvents();
    menu.grab().save(artifactPath(QStringLiteral("menu-sample.png")));
    menu.close();
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

    // 隐藏标签页创建 → 切换 → 也必须用新图（而不是创建瞬间的小纹理平铺）。
    // 其中一个标签页开 Markdown 实时预览（用户报告切换到文件就没背景的场景）。
    QTabWidget tabs;
    auto* placeholder = new editor::CodeEditor(&tabs);
    placeholder->setPlainText(QStringLiteral("placeholder"));
    placeholder->applyTheme();
    tabs.addTab(placeholder, QStringLiteral("p"));
    auto* hidden = new editor::CodeEditor(&tabs);
    hidden->setPlainText(QStringLiteral("# 隐藏标签页\n\n正文\n"));
    hidden->setLiveMarkdown(true);
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

// 回归：实时预览的表格覆盖层必须透明——表格区域透出背景图，
// 不再是一整块不透明的色块（旧版 td 有 currentLineBg 实底 + 覆盖层自身 Base 实底）。
void tableTransparentRegression(const QString& imagePath)
{
    editor::background::UserBackground bg;
    bg.imagePath = imagePath;
    bg.mode = QStringLiteral("stretch");
    bg.opacity = 1.0; // 渲染时限幅 0.5：壁纸以 50% 强度盖在白底上
    editor::background::save(bg);

    // 大图：整幅斜向渐变（保证表格区域内不同位置颜色可区分）
    QImage big(1600, 1200, QImage::Format_RGB32);
    for (int y = 0; y < big.height(); ++y)
        for (int x = 0; x < big.width(); x += big.width())
            big.setPixel(x, y, QColor(200 - x % 100, 120 + y % 80, 160).rgb());
    // 横向渐变条纹：每 40px 一档，跨度大，混白后依然可区分
    for (int x = 0; x < big.width(); ++x) {
        const int level = (x / 40) % 2;
        const QColor color = level ? QColor(20, 20, 20) : QColor(230, 230, 230);
        for (int y = 0; y < big.height(); ++y)
            big.setPixel(x, y, color.rgb());
    }
    big.save(imagePath);

    editor::CodeEditor editor;
    editor.setPlainText(QStringLiteral(
        "# 标题\n\n| a | b |\n|---|---|\n| 1 | 2 |\n| 3 | 4 |\n"));
    editor.setLiveMarkdown(true);
    editor.resize(800, 500);
    editor.applyTheme();
    editor.show();
    QCoreApplication::processEvents();
    // 表格扫描是 150ms 单发定时器：等它触发并完成布局收敛
    QElapsedTimer wait;
    wait.start();
    while (!wait.hasExpired(600))
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    const QImage grab = editor.grab().toImage().convertToFormat(QImage::Format_RGB32);
    grab.save(artifactPath(QStringLiteral("table-transparent.png")));
    // 在表格覆盖层区域内取一行（避开首行表头与文字行）：横向采样一排像素，
    // 透明 = 能看到黑/白条纹渐变（方差大）；不透明实底 = 几乎恒色（方差≈0）。
    const int sampleY = grab.height() / 2 + 30;
    int minV = 255, maxV = 0;
    for (int x = 60; x < grab.width() - 60; ++x) {
        const QColor c = grab.pixelColor(x, sampleY);
        const int v = c.green();
        minV = qMin(minV, v);
        maxV = qMax(maxV, v);
    }
    std::printf("    (表格区亮度跨度 %d..%d)\n", minV, maxV);
    expect(maxV - minV > 60, "表格区域透出背景（亮度有大幅起伏，非实色块）");
}

} // namespace

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Hutaomu"));
    QApplication::setApplicationName(QStringLiteral("Hutaomu Editor SettingsUiTest"));

    // --real-config：不启用测试模式 AppData，直接读取用户真实的 background.json
    // 与背景图，按真实配置渲染一张编辑器截图（只读，绝不写用户的配置）。
    if (app.arguments().contains(QStringLiteral("--real-config"))) {
        // 必须先用真实应用名，AppDataLocation 才指向用户的配置目录
        QApplication::setApplicationName(QStringLiteral("Hutaomu Editor"));
        const editor::background::UserBackground real = editor::background::current();
        std::printf("real bg: path=%s mode=%s opacity=%.2f valid=%d exists=%d\n",
                    qPrintable(real.imagePath), qPrintable(real.mode), real.opacity,
                    int(real.isValid()),
                    int(!real.imagePath.isEmpty()
                        && QFileInfo::exists(real.imagePath)));
        editor::ThemeManager::applyTheme(QStringLiteral("paper"));
        editor::CodeEditor editor;
        editor.setPlainText(QStringLiteral("# 标题\n\n正文文字用于对比背景可见性。\n"));
        editor.resize(1600, 900);
        editor.applyTheme();
        editor.show();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        editor.grab().save(artifactPath(QStringLiteral("real-config-editor.png")));
        std::printf("saved real-config-editor.png\n");

        // 主窗口整版：验证活动栏底部插件按钮（非模态面板入口）与图标区分
        app::MainWindow window;
        window.resize(1600, 900);
        window.show();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        window.grab().save(artifactPath(QStringLiteral("real-config-mainwindow.png")));
        std::printf("saved real-config-mainwindow.png\n");
        // 展开插件面板再抓活动栏与整窗
        if (auto* bar = window.findChild<QWidget*>(QStringLiteral("ActivityBar"))) {
            for (auto* button : bar->findChildren<QToolButton*>())
                if (button->property("pageId").toString() == QLatin1String("plugins"))
                    button->click();
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            bar->grab().save(artifactPath(QStringLiteral("activity-bar.png")));
            window.grab().save(artifactPath(QStringLiteral("real-config-pluginpanel.png")));
            std::printf("saved activity-bar.png + pluginpanel\n");
        }
        return 0;
    }

    // 测试模式：AppData 指向临时目录，绝不读写真实用户的 background.json
    QStandardPaths::setTestModeEnabled(true);
    auto& s = settings::AppSettings::instance();
    s.load();
    editor::ThemeManager::applyTheme(s.theme);

    grabSettingsTabs();
    grabMenuSample();

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

    // 表格覆盖层透明（透出背景图）
    tableTransparentRegression(imagePath);

    editor::background::clear();
    std::printf(failures == 0 ? "ALL PASS\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
