// Hutaomu Editor - PDF viewer implementation (pdfium rendering).
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "PdfViewer.h"

#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "fpdfview.h"

namespace viewers {

namespace {
// pdfium 全局初始化（进程级一次）
struct PdfiumInit {
    PdfiumInit()
    {
        FPDF_LIBRARY_CONFIG config;
        memset(&config, 0, sizeof(config));
        config.version = 2;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig(&config);
    }
    ~PdfiumInit() { FPDF_DestroyLibrary(); }
};

PdfiumInit& pdfiumInit()
{
    static PdfiumInit init;
    return init;
}
} // namespace

PdfViewer::PdfViewer(const QString& filePath, QWidget* parent)
    : DocumentViewer(parent)
    , m_filePath(filePath)
{
    setObjectName(QStringLiteral("pdfViewer"));
    pdfiumInit(); // 确保库已初始化

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 顶部工具条：页码导航
    auto* toolbar = new QWidget(this);
    auto* toolLayout = new QHBoxLayout(toolbar);
    toolLayout->setContentsMargins(8, 4, 8, 4);
    auto* prev = new QPushButton(tr("上一页"), toolbar);
    prev->setObjectName(QStringLiteral("pdfPrevButton"));
    connect(prev, &QPushButton::clicked, this, &PdfViewer::previousPage);
    auto* next = new QPushButton(tr("下一页"), toolbar);
    next->setObjectName(QStringLiteral("pdfNextButton"));
    connect(next, &QPushButton::clicked, this, &PdfViewer::nextPage);
    m_pageLabel = new QLabel(toolbar);
    m_pageLabel->setObjectName(QStringLiteral("pdfPageLabel"));
    toolLayout->addWidget(prev);
    toolLayout->addWidget(m_pageLabel, 1);
    toolLayout->addWidget(next);
    layout->addWidget(toolbar);

    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("pdfScroll"));
    m_scroll->setWidgetResizable(false);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_canvas = new QLabel;
    m_canvas->setObjectName(QStringLiteral("pdfCanvas"));
    m_canvas->setAlignment(Qt::AlignCenter);
    m_scroll->setWidget(m_canvas);
    layout->addWidget(m_scroll, 1);

    loadPdf();
}

PdfViewer::~PdfViewer()
{
    if (m_document) {
        FPDF_CloseDocument(static_cast<FPDF_DOCUMENT>(m_document));
        m_document = nullptr;
    }
    m_documentBytes.clear(); // 文档已关闭，字节可安全释放
}

bool PdfViewer::loadPdf()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    // 必须保存为成员：FPDF_LoadMemDocument 只持有指针，不复制数据。
    // 局部 QByteArray 析构后 pdfium 会读悬空内存（首屏可能侥幸正常，
    // 翻页时崩溃在 FPDF_GetTrailerEnds 等内部解析）。
    m_documentBytes = file.readAll();
    m_document = FPDF_LoadMemDocument(m_documentBytes.constData(),
                                      int(m_documentBytes.size()), nullptr);
    if (!m_document) {
        // 加密/损坏：明确提示（不再崩溃）
        m_pageCount = 0;
        m_canvas->setText(tr("无法打开此 PDF（可能已加密或损坏）。\n%1")
                              .arg(QFileInfo(m_filePath).fileName()));
        m_canvas->setAlignment(Qt::AlignCenter);
        m_canvas->adjustSize();
        updateNavLabel();
        return false;
    }
    m_pageCount = FPDF_GetPageCount(static_cast<FPDF_DOCUMENT>(m_document));
    goToPage(0);
    return true;
}

void PdfViewer::updateNavLabel()
{
    m_pageLabel->setText(tr("第 %1 / %2 页").arg(m_currentPage + 1).arg(m_pageCount));
}

void PdfViewer::goToPage(int page)
{
    if (!m_document || m_pageCount <= 0)
        return;
    m_currentPage = qBound(0, page, m_pageCount - 1);
    renderCurrentPage();
    updateNavLabel();
}

void PdfViewer::renderCurrentPage()
{
    if (!m_document)
        return;
    FPDF_PAGE page = FPDF_LoadPage(static_cast<FPDF_DOCUMENT>(m_document),
                                   m_currentPage);
    if (!page)
        return;
    const qreal pageWidth = FPDF_GetPageWidthF(page);   // pt
    const qreal pageHeight = FPDF_GetPageHeightF(page);
    // pt -> px：1pt = 4/3 px @96dpi，再乘用户缩放
    const qreal scale = (4.0 / 3.0) * m_zoom;
    const int pixelW = qMax(1, qRound(pageWidth * scale));
    const int pixelH = qMax(1, qRound(pageHeight * scale));
    // 渲染尺寸上限（防超大缩放耗尽内存）
    if (pixelW * pixelH > 64'000'000) {
        FPDF_ClosePage(page);
        return;
    }

    QImage image(pixelW, pixelH, QImage::Format_ARGB32);
    image.fill(Qt::white);
    FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(pixelW, pixelH,
                                             FPDFBitmap_BGRA,
                                             image.bits(), pixelW * 4);
    if (bitmap) {
        FPDFBitmap_FillRect(bitmap, 0, 0, pixelW, pixelH, 0xFFFFFFFF);
        FPDF_RenderPageBitmap(bitmap, page, 0, 0, pixelW, pixelH, 0,
                              FPDF_ANNOT);
        FPDFBitmap_Destroy(bitmap);
    }
    FPDF_ClosePage(page);

    m_canvas->setPixmap(QPixmap::fromImage(image));
    m_canvas->setFixedSize(pixelW, pixelH);
    m_canvas->adjustSize();
}

void PdfViewer::nextPage()
{
    goToPage(m_currentPage + 1);
}

void PdfViewer::previousPage()
{
    goToPage(m_currentPage - 1);
}

void PdfViewer::zoomIn()
{
    m_zoom = qMin(8.0, m_zoom * 1.25);
    renderCurrentPage();
}

void PdfViewer::zoomOut()
{
    m_zoom = qMax(0.1, m_zoom / 1.25);
    renderCurrentPage();
}

void PdfViewer::resetZoom()
{
    m_zoom = 1.0;
    renderCurrentPage();
}

} // namespace viewers