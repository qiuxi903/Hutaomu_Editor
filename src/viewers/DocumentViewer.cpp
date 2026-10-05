// Hutaomu Editor - Viewer framework common code.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "DocumentViewer.h"
#include <QFileInfo>

namespace viewers {

DocumentViewer::DocumentViewer(QWidget* parent)
    : QWidget(parent)
{
}

ViewerKind viewerKindForFile(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("png") || suffix == QStringLiteral("jpg")
        || suffix == QStringLiteral("jpeg") || suffix == QStringLiteral("bmp")
        || suffix == QStringLiteral("gif") || suffix == QStringLiteral("webp")
        || suffix == QStringLiteral("svg") || suffix == QStringLiteral("ico")
        || suffix == QStringLiteral("tif") || suffix == QStringLiteral("tiff"))
        return ViewerKind::Image;
    if (suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mkv")
        || suffix == QStringLiteral("avi") || suffix == QStringLiteral("mov")
        || suffix == QStringLiteral("webm") || suffix == QStringLiteral("wmv")
        || suffix == QStringLiteral("flv") || suffix == QStringLiteral("m4v"))
        return ViewerKind::Video;
    if (suffix == QStringLiteral("mp3") || suffix == QStringLiteral("wav")
        || suffix == QStringLiteral("flac") || suffix == QStringLiteral("ogg")
        || suffix == QStringLiteral("m4a") || suffix == QStringLiteral("aac")
        || suffix == QStringLiteral("wma"))
        return ViewerKind::Audio;
    if (suffix == QStringLiteral("pdf"))
        return ViewerKind::Pdf;
    if (suffix == QStringLiteral("docx") || suffix == QStringLiteral("doc"))
        return ViewerKind::Docx;
    if (suffix == QStringLiteral("xlsx") || suffix == QStringLiteral("xls")
        || suffix == QStringLiteral("csv"))
        return ViewerKind::Xlsx;
    if (suffix == QStringLiteral("pptx") || suffix == QStringLiteral("ppt"))
        return ViewerKind::Pptx;
    return ViewerKind::None;
}

QString viewerKindName(ViewerKind kind)
{
    switch (kind) {
    case ViewerKind::Image: return QStringLiteral("图片");
    case ViewerKind::Video: return QStringLiteral("视频");
    case ViewerKind::Audio: return QStringLiteral("音频");
    case ViewerKind::Pdf: return QStringLiteral("PDF");
    case ViewerKind::Docx: return QStringLiteral("Word");
    case ViewerKind::Xlsx: return QStringLiteral("Excel");
    case ViewerKind::Pptx: return QStringLiteral("PowerPoint");
    case ViewerKind::None:
    default:
        return QStringLiteral("纯文本");
    }
}

} // namespace viewers