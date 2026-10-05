// Hutaomu Editor - Office (OOXML) viewers: docx/xlsx/pptx.
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QString>

#include "viewers/DocumentViewer.h"

class QTextEdit;
class QTableWidget;
class QListWidget;
class QStackedWidget;

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

private:
    bool loadDocx();

    QString m_filePath;
    QByteArray m_originalZip; // 原���文件字节（保存时替换 document.xml 用）
    QTextEdit* m_editor = nullptr;
    bool m_modified = false;
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

private:
    bool loadXlsx();
    bool loadCsv();
    bool saveCsv();

    QString m_filePath;
    QByteArray m_originalZip;
    QTableWidget* m_table = nullptr;
    QStringList m_sheetNames;
    int m_sheetIndex = 0;
    bool m_isCsv = false;
    bool m_loading = false;
    bool m_modified = false;
};

// ---- PowerPoint (.pptx)：幻灯片文本浏览+编辑 ----
class PptxViewer : public DocumentViewer {
    Q_OBJECT
public:
    explicit PptxViewer(const QString& filePath, QWidget* parent = nullptr);

    QString filePath() const override { return m_filePath; }
    bool isModified() const override;
    bool save() override;

private:
    bool loadPptx();

    QString m_filePath;
    QByteArray m_originalZip;
    QListWidget* m_slideList = nullptr;
    QStackedWidget* m_slides = nullptr;
    bool m_modified = false;
};

} // namespace viewers