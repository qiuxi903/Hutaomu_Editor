// Hutaomu Editor - Office (OOXML) viewers: docx/xlsx/pptx.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QString>

#include "viewers/DocumentViewer.h"

class QTextEdit;
class QTableWidget;
class QListWidget;
class QStackedWidget;
class QLineEdit;
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

private:
    bool loadXlsx();
    bool loadCsv();
    bool saveCsv();
    QTableWidget* makeSheetTable();
    QString cellReference(int row, int column) const;   // (0,0) -> "A1"

    QString m_filePath;
    QByteArray m_originalZip;
    QTabWidget* m_sheets = nullptr;   // 每个工作表一个表格页（底部标签）
    QTableWidget* m_table = nullptr;  // 当前活动表（保存用）
    QLineEdit* m_nameBox = nullptr;    // 名称框：当前单元格引用
    QLineEdit* m_formulaBox = nullptr; // fx 编辑栏：当前单元格内容
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