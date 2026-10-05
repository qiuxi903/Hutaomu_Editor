// Hutaomu Editor - Image viewer (zoom/pan/rotate + save).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QImage>
#include <QPoint>

#include "viewers/DocumentViewer.h"

class QLabel;
class QScrollArea;

namespace viewers {

// 图片查看器：滚轮缩放（Ctrl+滚轮）、拖拽平移、旋转、另存。
// 可编辑项（旋转/翻转/缩放保存）会真实写回文件。
class ImageViewer : public DocumentViewer {
    Q_OBJECT
public:
    explicit ImageViewer(const QString& filePath, QWidget* parent = nullptr);

    QString filePath() const override { return m_filePath; }
    bool isModified() const override { return m_modified; }
    bool save() override;
    bool saveAs(const QString& path);

    void zoomIn() override;
    void zoomOut() override;
    void resetZoom() override;
    bool supportsZoom() const override { return true; }

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool event(QEvent* event) override;

private:
    void applyTransform();
    void setModified(bool on);

    QString m_filePath;
    QImage m_image;       // 原始图像（变换前的基准）
    QImage m_display;     // 应用旋转/翻转后的图像
    qreal m_zoom = 1.0;
    int m_rotation = 0;   // 90 度步进累计
    bool m_flipH = false;
    bool m_modified = false;
    QPoint m_panStart;
    bool m_panning = false;
    QScrollArea* m_scroll = nullptr;
    QLabel* m_canvas = nullptr;
};

} // namespace viewers