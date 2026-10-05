// Hutaomu Editor - Theme-aware SVG icon loading.
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "IconLoader.h"

#include "ThemeManager.h"

#include <QCache>
#include <QFile>
#include <QGuiApplication>
#include <QSvgRenderer>
#include <QImage>
#include <QPainter>

namespace editor {

namespace {

// 主题包覆盖优先，其次内置资源。
QString iconSourcePath(const QString& name)
{
    const QString override = ThemeManager::assetPath(
        QStringLiteral("assets/icons/%1.svg").arg(name));
    if (!override.isEmpty())
        return override;
    return QStringLiteral(":/icons/%1.svg").arg(name);
}

QCache<QString, QPixmap>& pixmapCache()
{
    static QCache<QString, QPixmap> cache;
    cache.setMaxCost(256);
    return cache;
}

} // namespace

QPixmap IconLoader::loadFile(const QString& path, const QColor& fill,
                              const QColor& panel, int size)
{
    if (path.isEmpty())
        return QPixmap();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QPixmap();

    QString svg = QString::fromUtf8(file.readAll());
    svg.replace(QStringLiteral("FILLCOLOR"), fill.name());
    svg.replace(QStringLiteral("PANELCOLOR"), panel.name());

    const qreal dpr = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
    QImage image(qRound(size * dpr), qRound(size * dpr),
                 QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QSvgRenderer renderer(svg.toUtf8());
    QPainter painter(&image);
    renderer.render(&painter, QRectF(0, 0, size, size));
    painter.end();
    return QPixmap::fromImage(image);
}

QPixmap IconLoader::load(const QString& name, const QColor& fill,
                         const QColor& panel, int size)
{
    const QString source = iconSourcePath(name);
    // 主题 id 入键：不同主题可为同名图标提供不同资源，切换后不能复用旧图。
    const QString key = QStringLiteral("%1|%2|%3|%4|%5")
                            .arg(ThemeManager::theme(), name, fill.name(),
                                 panel.name())
                            .arg(size);

    QCache<QString, QPixmap>& cache = pixmapCache();
    if (QPixmap* hit = cache.object(key))
        return *hit;

    QFile file(source);
    if (!file.open(QIODevice::ReadOnly))
        return QPixmap();

    QString svg = QString::fromUtf8(file.readAll());
    svg.replace(QStringLiteral("FILLCOLOR"), fill.name());
    svg.replace(QStringLiteral("PANELCOLOR"), panel.name());

    const qreal dpr = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
    QImage image(qRound(size * dpr), qRound(size * dpr), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);

    QSvgRenderer renderer(svg.toUtf8());
    QPainter painter(&image);
    renderer.render(&painter, QRectF(0, 0, size, size));
    painter.end();

    QPixmap pixmap = QPixmap::fromImage(image);
    cache.insert(key, new QPixmap(pixmap));
    return pixmap;
}

void IconLoader::clearCache()
{
    pixmapCache().clear();
}

} // namespace editor
