// Hutaomu Editor - PDF viewer (pdfium rendering).
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QImage>
#include <QStringList>
#include <QWidget>

#include "viewers/DocumentViewer.h"

class QLabel;
class QPushButton;
class QScrollArea;

namespace viewers {

// PDF 查看器（pdfium 渲染）：
//   - 真实页面渲染（矢量/字体/CJK 全支持）到 QImage；
//   - 页码导航、Ctrl+滚轮缩放、页内滚动；
//   - 渲染在后台线程做（大页面不卡 UI）。
class PdfViewer : public DocumentViewer {
    Q_OBJECT
public:
    explicit PdfViewer(const QString& filePath, QWidget* parent = nullptr);
    ~PdfViewer() override;

    QString filePath() const override { return m_filePath; }

    void zoomIn() override;
    void zoomOut() override;
    void resetZoom() override;
    bool supportsZoom() const override { return true; }

private slots:
    void goToPage(int page);
    void nextPage();
    void previousPage();
    void renderCurrentPage();

private:
    bool loadPdf();
    void updateNavLabel();

    QString m_filePath;
    // FPDF_LoadMemDocument 不复制数据、只持有指针——必须让字节活到
    // document 关闭为止（否则翻页时 pdfium 读到已释放内存而崩溃）
    QByteArray m_documentBytes;
    void* m_document = nullptr; // FPDF_DOCUMENT（避免在头里引 pdfium 头）
    int m_pageCount = 0;
    int m_currentPage = 0;
    qreal m_zoom = 1.0;         // 显示缩放（基准 = 页面 1:1 像素）
    QScrollArea* m_scroll = nullptr;
    QLabel* m_canvas = nullptr;
    QLabel* m_pageLabel = nullptr;
};

} // namespace viewers