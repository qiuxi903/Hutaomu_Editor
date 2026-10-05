// Hutaomu Editor - Image viewer implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "ImageViewer.h"

#include <QApplication>
#include <QFileInfo>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollBar>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace viewers {

ImageViewer::ImageViewer(const QString& filePath, QWidget* parent)
    : DocumentViewer(parent)
    , m_filePath(filePath)
{
    setObjectName(QStringLiteral("imageViewer"));

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(false);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_canvas = new QLabel;
    m_canvas->setAlignment(Qt::AlignCenter);
    m_scroll->setWidget(m_canvas);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scroll);

    m_image.load(filePath);
    m_display = m_image;
    applyTransform();
}

void ImageViewer::applyTransform()
{
    QTransform t;
    t.rotate(m_rotation);
    if (m_flipH)
        t.scale(-1.0, 1.0);
    m_display = m_image.transformed(t);
    QImage scaled = m_display.scaled(
        qMax(1, qRound(m_display.width() * m_zoom)),
        qMax(1, qRound(m_display.height() * m_zoom)),
        Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_canvas->setPixmap(QPixmap::fromImage(scaled));
    m_canvas->adjustSize();
}

void ImageViewer::setModified(bool on)
{
    if (m_modified != on) {
        m_modified = on;
        emit modifiedChanged(on);
    }
}

void ImageViewer::zoomIn()
{
    m_zoom = qMin(8.0, m_zoom * 1.25);
    applyTransform();
}

void ImageViewer::zoomOut()
{
    m_zoom = qMax(0.05, m_zoom / 1.25);
    applyTransform();
}

void ImageViewer::resetZoom()
{
    m_zoom = 1.0;
    m_rotation = 0;
    m_flipH = false;
    applyTransform();
}

void ImageViewer::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0)
            zoomIn();
        else
            zoomOut();
        event->accept();
        return;
    }
    DocumentViewer::wheelEvent(event);
}

void ImageViewer::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_panning = true;
        m_panStart = event->globalPosition().toPoint();
        setCursor(Qt::ClosedHandCursor);
    }
    DocumentViewer::mousePressEvent(event);
}

void ImageViewer::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        const QPoint delta = event->globalPosition().toPoint() - m_panStart;
        m_panStart = event->globalPosition().toPoint();
        QScrollBar* h = m_scroll->horizontalScrollBar();
        QScrollBar* v = m_scroll->verticalScrollBar();
        h->setValue(h->value() - delta.x());
        v->setValue(v->value() - delta.y());
    }
    DocumentViewer::mouseMoveEvent(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
    }
    DocumentViewer::mouseReleaseEvent(event);
}

bool ImageViewer::event(QEvent* event)
{
    // 旋转/翻转快捷键（Ctrl+R 旋转，Ctrl+Shift+R 反向，Ctrl+H 翻转）
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->modifiers() & Qt::ControlModifier) {
            if (key->key() == Qt::Key_R) {
                m_rotation = (m_rotation + (key->modifiers() & Qt::ShiftModifier
                                                ? -90 : 90) + 360) % 360;
                setModified(true);
                applyTransform();
                return true;
            }
            if (key->key() == Qt::Key_H) {
                m_flipH = !m_flipH;
                setModified(true);
                applyTransform();
                return true;
            }
        }
    }
    return DocumentViewer::event(event);
}

bool ImageViewer::save()
{
    return saveAs(m_filePath);
}

bool ImageViewer::saveAs(const QString& path)
{
    const QByteArray fmt = QFileInfo(path).suffix().toLower().toUtf8();
    if (!m_display.save(path, fmt.isEmpty() ? nullptr : fmt.constData()))
        return false;
    m_image = m_display;
    setModified(false);
    return true;
}

} // namespace viewers