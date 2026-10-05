// Hutaomu Editor - Viewer factory implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "ViewerFactory.h"

#include "viewers/ImageViewer.h"
#include "viewers/MediaViewer.h"
#include "viewers/OfficeViewer.h"
#include "viewers/PdfViewer.h"

namespace viewers {

DocumentViewer* createViewer(const QString& filePath, QWidget* parent)
{
    switch (viewerKindForFile(filePath)) {
    case ViewerKind::Image:
        return new ImageViewer(filePath, parent);
    case ViewerKind::Video:
    case ViewerKind::Audio:
        return new MediaViewer(filePath, parent);
    case ViewerKind::Pdf:
        return new PdfViewer(filePath, parent);
    case ViewerKind::Docx:
        return new DocxViewer(filePath, parent);
    case ViewerKind::Xlsx:
        return new XlsxViewer(filePath, parent);
    case ViewerKind::Pptx:
        return new PptxViewer(filePath, parent);
    case ViewerKind::None:
    default:
        return nullptr;
    }
}

} // namespace viewers