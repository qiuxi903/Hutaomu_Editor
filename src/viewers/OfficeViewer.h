// Hutaomu Editor - Office (OOXML) viewers: docx/xlsx/pptx.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QColor>
#include <QLabel>
#include <QRectF>
#include <QString>
#include <QVector>

#include "viewers/DocumentViewer.h"

class QTextEdit;
class QTableWidget;
class QListWidget;
class QStackedWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidgetItem;
class QTabWidget;

namespace viewers {

// OOXML 解析共用工具：从 XML 文本提取 <w:t>/<a:t> 等文本节点
namespace ooxml {
QStringList extractRuns(const QString& xml, const QString& tag);
QString unescapeXml(QString text);
} // namespace ooxml

// ---- Word (.docx)：富文本编辑（所见段落，保存回写 docx）----
class DocxViewer : public DocumentViewer {
    Q_OBJECT
public:
    explicit DocxViewer(const QString& filePath, QWidget* parent = nullptr);

    QString filePath() const override { return m_filePath; }
    bool isModified() const override;
    bool save() override;

public slots:
    bool supportsZoom() const override { return true; }
    void zoomIn() override;
    void zoomOut() override;
    void resetZoom() override;

private:
    bool loadDocx();
    void updatePageStats();

    QString m_filePath;
    QByteArray m_originalZip; // 原始文件字节（保存时替换 document.xml 用）
    QTextEdit* m_editor = nullptr;
    QLabel* m_statsLabel = nullptr; // 底部：字数 / 段落
    bool m_modified = false;
    bool m_loading = false; // setHtml 也会触发 contentsChanged，需要区分
};

// ---- Excel (.xlsx/.csv)：表格编辑（单元格直接可改，保存回写）----
class XlsxViewer : public DocumentViewer {
    Q_OBJECT
public:
    explicit XlsxViewer(const QString& filePath, QWidget* parent = nullptr);

    QString filePath() const override { return m_filePath; }
    bool isModified() const override;
    bool save() override;

private slots:
    void cellChanged(int row, int column);
    // 名称框/编辑栏跟随当前单元格；编辑栏回车写入单元格
    void currentCellChanged(int row, int column, int previousRow, int previousColumn);
    void formulaBoxReturned();

public:
    // 测试可达：多表结构与单元格内容
    int sheetCount() const;
    QString sheetName(int index) const;
    QTableWidgetItem* cellAt(int sheet, int row, int column) const;
    QLineEdit* nameBox() const { return m_nameBox; }
    QLineEdit* formulaBox() const { return m_formulaBox; }
    QTabWidget* sheetTabs() const { return m_sheets; }
    QString statsText() const { return m_statsLabel ? m_statsLabel->text() : QString(); }
    // 测试可达：手动触发一次选区统计刷新
    void refreshSelectionStats() { refreshSelectionStatsImpl(); }

private:
    bool loadXlsx();
    bool loadCsv();
    bool saveCsv();
    QTableWidget* makeSheetTable();
    QString cellReference(int row, int column) const;   // (0,0) -> "A1"
    void updateFormulaBar(QTableWidget* table, int row, int column);
    void refreshSelectionStatsImpl();

    QString m_filePath;
    QByteArray m_originalZip;
    QTabWidget* m_sheets = nullptr;   // 每个工作表一个表格页（底部标签）
    QTableWidget* m_table = nullptr;  // 当前活动表（保存用）
    QLineEdit* m_nameBox = nullptr;    // 名称框：当前单元格引用
    QLineEdit* m_formulaBox = nullptr; // fx 编辑栏：当前单元格内容（公式时显示 =公式）
    QLabel* m_statsLabel = nullptr;    // 选区统计：求和/平均/计数
    QStringList m_sheetNames;
    int m_sheetIndex = 0;
    bool m_isCsv = false;
    bool m_loading = false;
    bool m_modified = false;
};

// ---- PowerPoint (.pptx)：幻灯片形状/文本/图片渲染 ----

// 文本 run：字号（pt）、加粗、颜色
struct PptxRun {
    QString text;
    double sizePt = 18.0;
    bool bold = false;
    QColor color;
};

// 形状：位置/尺寸（pt）、填充/描边、文本段落、图片
struct PptxShape {
    QRectF rect;                    // 左上角 + 宽高（pt）
    QColor fill;
    QColor line;
    QImage image;                   // 图片形状（已从包内解出）
    QVector<QVector<PptxRun>> paragraphs;
    bool picture = false;
};

struct PptxSlide {
    QSizeF sizePt = QSizeF(960, 540);
    QVector<PptxShape> shapes;
};

class PptxViewer : public DocumentViewer {
    Q_OBJECT
public:
    explicit PptxViewer(const QString& filePath, QWidget* parent = nullptr);

    QString filePath() const override { return m_filePath; }
    bool isModified() const override;
    bool save() override;

    // 查看器缩放（画布等比缩放）
    bool supportsZoom() const override { return true; }
    void zoomIn() override;
    void zoomOut() override;
    void resetZoom() override;
    double zoom() const { return m_zoom; }

public slots:
    void showSlide(int index);

public:
    // 测试可达
    int slideCount() const { return m_slideData.size(); }
    int shapeCount(int slide) const;
    QString slideText(int slide) const;

private slots:
    void nextSlide();
    void previousSlide();

private:
    bool loadPptx();
    void applyZoom();

    QString m_filePath;
    QByteArray m_originalZip;
    QListWidget* m_slideList = nullptr;
    QStackedWidget* m_slides = nullptr;
    QLabel* m_pageLabel = nullptr;
    QPushButton* m_prevButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QVector<PptxSlide> m_slideData;
    double m_zoom = 1.0;
    bool m_modified = false;
};

} // namespace viewers