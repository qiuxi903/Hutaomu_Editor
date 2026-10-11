// Hutaomu Editor - 编辑器视口背景画刷（无状态共享服务）实现。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "EditorBackground.h"

#include "themes/ThemeManager.h"
#include "themes/UserBackground.h"

#include <QFileInfo>
#include <QImage>
#include <QMap>
#include <QPainter>
#include <QPixmap>

namespace editor::editorbackground {

namespace {

QMap<QString, QBrush>& brushCache()
{
    static QMap<QString, QBrush> cache;
    return cache;
}

} // namespace

QBrush viewportBrush(const QSize& viewportSize, qreal devicePixelRatio)
{
    const ThemeDefinition theme = ThemeManager::currentTheme();
    const QColor baseColor = theme.color(QStringLiteral("editorBg"));

    // 背景来源：用户自定义（设置页「背景…」）优先，主题自带次之
    const editor::background::UserBackground userBg = editor::background::current();
    QString path;
    QString mode;
    qreal opacity = 0.0;
    if (userBg.isValid() && QFileInfo::exists(userBg.imagePath)) {
        path = userBg.imagePath;
        mode = userBg.mode;
        // 密集纹理在 >50% 时文字会被花纹淹没；旧版本滑条语义标反可能存下 1.0
        opacity = qMin<qreal>(userBg.opacity, 0.5);
    } else if (theme.background.isValid()) {
        path = ThemeManager::assetPath(theme.background.image);
        mode = theme.background.mode;
        opacity = qBound(qreal(0.0), theme.background.opacity, qreal(1.0));
    }
    if (path.isEmpty())
        return QBrush(baseColor);

    // 视口过小（标签页创建瞬间）不合成：纹理会被画刷平铺成碎花纹。
    // 键里不包含这种裁剪，等真正布局好自然会拿到满尺寸纹理。
    if (viewportSize.isEmpty() || viewportSize.width() < 160
        || viewportSize.height() < 80)
        return QBrush(baseColor);

    // 键 = 全部影响结果的输入。图片身份用 mtime+size（「背景…」换图是同路径覆盖）
    const QFileInfo info(path);
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7x%8|%9")
                            .arg(ThemeManager::theme(),
                                 path,
                                 QString::number(info.lastModified().toMSecsSinceEpoch()),
                                 QString::number(info.size()),
                                 mode,
                                 QString::number(opacity),
                                 QString::number(viewportSize.width()),
                                 QString::number(viewportSize.height()),
                                 QString::number(devicePixelRatio));

    auto& cache = brushCache();
    if (auto it = cache.constFind(key); it != cache.constEnd())
        return it.value();

    QImage source(path);
    if (source.isNull())
        return QBrush(baseColor); // 加载失败：纯色兜底（不缓存，文件恢复后可重试）

    const QSize logicalSize = source.size() / source.devicePixelRatio();
    QString effective = mode;
    if (effective.isEmpty() || effective == QLatin1String("auto")) {
        effective = (logicalSize.width() >= viewportSize.width()
                     && logicalSize.height() >= viewportSize.height())
                        ? QStringLiteral("cover")
                        : QStringLiteral("tile");
    }

    QImage canvas(viewportSize * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
    canvas.setDevicePixelRatio(devicePixelRatio);
    QPainter painter(&canvas);
    painter.fillRect(QRect(QPoint(0, 0), viewportSize), baseColor);
    painter.setOpacity(opacity);
    if (effective == QLatin1String("stretch")) {
        painter.drawImage(QRect(QPoint(0, 0), viewportSize), source);
    } else if (effective == QLatin1String("cover")) {
        const qreal scale = qMax(qreal(viewportSize.width()) / logicalSize.width(),
                                 qreal(viewportSize.height()) / logicalSize.height());
        const QSize scaled(qRound(logicalSize.width() * scale),
                           qRound(logicalSize.height() * scale));
        painter.drawImage(QRect((viewportSize.width() - scaled.width()) / 2,
                                (viewportSize.height() - scaled.height()) / 2,
                                scaled.width(), scaled.height()),
                          source);
    } else if (effective == QLatin1String("center")) {
        painter.drawImage(QPoint((viewportSize.width() - logicalSize.width()) / 2,
                                 (viewportSize.height() - logicalSize.height()) / 2),
                          source);
    } else { // tile
        for (int y = 0; y < viewportSize.height(); y += logicalSize.height())
            for (int x = 0; x < viewportSize.width(); x += logicalSize.width())
                painter.drawImage(QPoint(x, y), source);
    }
    painter.end();

    const QBrush brush(QPixmap::fromImage(canvas));
    if (cache.size() > 8)
        cache.clear(); // 尺寸/主题反复变化的简单防膨胀
    cache.insert(key, brush);
    return brush;
}

} // namespace editor::editorbackground
