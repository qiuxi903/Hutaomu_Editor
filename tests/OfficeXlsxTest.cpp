// Hutaomu Editor - XlsxViewer (A-route rendering) regression tests.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
//
// 覆盖：sharedStrings（t="s" 显示真实文本）、多工作表（表名/数量）、
// 合并单元格（setSpan + 锚点取值）、列宽/行高、字体加粗、数字格式
// （0.00/百分比/日期序列号）、内联字符串与布尔值。
#include <QApplication>
#include <QDate>
#include <QTableWidget>
#include <QTemporaryDir>
#include <cstdio>

#include "core/ZipWriter.h"
#include "viewers/OfficeViewer.h"

namespace {

int failures = 0;

void expect(bool condition, const char* what)
{
    if (condition)
        std::printf("  PASS  %s\n", what);
    else {
        std::printf("  FAIL  %s\n", what);
        ++failures;
    }
}

QByteArray zipOf(const QList<QPair<QString, QByteArray>>& entries)
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("fixture.xlsx"));
    core::ZipWriter writer(path);
    for (const auto& entry : entries)
        writer.addFile(entry.first, entry.second);
    writer.close();
    QFile file(path);
    file.open(QIODevice::ReadOnly);
    return file.readAll();
}

QByteArray xmlOf(const char* text)
{
    return QByteArray(text) + QByteArrayLiteral("\n");
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    setvbuf(stdout, nullptr, _IONBF, 0);

    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("fixture.xlsx"));

    // ---- 构造夹具：两个工作表、共享字符串、合并、样式、列宽、数字格式 ----
    {
        core::ZipWriter writer(path);
        writer.addFile(QStringLiteral("[Content_Types].xml"),
                       xmlOf(R"(<?xml version="1.0"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"/> )"));
        writer.addFile(QStringLiteral("xl/workbook.xml"),
                       xmlOf(R"(<?xml version="1.0"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="数据" sheetId="1" r:id="rId1"/><sheet name="Sheet2" sheetId="2" r:id="rId2"/></sheets></workbook>)"));
        writer.addFile(QStringLiteral("xl/_rels/workbook.xml.rels"),
                       xmlOf(R"(<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="x" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="x" Target="worksheets/sheet2.xml"/></Relationships>)"));
        writer.addFile(QStringLiteral("xl/sharedStrings.xml"),
                       xmlOf(R"(<?xml version="1.0"?><sst count="3" uniqueCount="3"><si><t>你好，世界</t></si><si><t>合并区标题</t></si><si><t>富文本</t></si></sst>)"));
        writer.addFile(QStringLiteral("xl/styles.xml"),
                       xmlOf(R"(<?xml version="1.0"?><styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><numFmts count="1"><numFmt numFmtId="164" formatCode="0.0%"/></numFmts><fonts count="2"><font><sz val="11"/><name val="Calibri"/></font><font><b/><sz val="14"/><color rgb="FFFF0000"/></font></fonts><fills count="2"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="solid"><fgColor rgb="FFFFFF00"/></patternFill></fill></fills><cellXfs count="5"><xf numFmtId="0" fontId="0" fillId="0"/><xf numFmtId="2" fontId="0" fillId="0" applyNumberFormat="1"/><xf numFmtId="0" fontId="1" fillId="1" applyFont="1" applyFill="1"/><xf numFmtId="164" fontId="0" fillId="0" applyNumberFormat="1"/><xf numFmtId="14" fontId="0" fillId="0" applyNumberFormat="1"/></cellXfs></styleSheet>)"));
        writer.addFile(QStringLiteral("xl/worksheets/sheet1.xml"),
                       xmlOf(R"(<?xml version="1.0"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><cols><col min="1" max="1" width="25.5" customWidth="1"/></cols><sheetData><row r="1" ht="30" customHeight="1"><c r="A1" t="s" s="2"><v>0</v></c><c r="B1" t="s"><v>1</v></c><c r="C1" t="b"><v>1</v></c></row><row r="2"><c r="A2" s="1"><v>3.14159</v></c><c r="B2" s="3"><v>0.1234</v></c><c r="C2" t="inlineStr"><is><t>内联</t></is></c></row><row r="3"><c r="A3" s="4"><v>45000</v></c><c r="B3" t="str"><v>公式缓存</v></c></row></sheetData><mergeCells count="1"><mergeCell ref="B1:C1"/></mergeCells></worksheet>)"));
        writer.addFile(QStringLiteral("xl/worksheets/sheet2.xml"),
                       xmlOf(R"(<?xml version="1.0"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData><row r="1"><c r="A1" t="s"><v>2</v></c></row></sheetData></worksheet>)"));
        writer.close();
    }

    viewers::XlsxViewer viewer(path);
    viewer.resize(900, 500);
    viewer.show();
    QCoreApplication::processEvents();

    std::printf("== 多表结构 ==\n");
    {
        expect(viewer.sheetCount() == 2, "two sheets discovered");
        expect(viewer.sheetName(0) == QStringLiteral("数据"),
               "sheet name from workbook.xml");
        expect(viewer.sheetName(1) == QStringLiteral("Sheet2"), "second sheet name");
    }

    std::printf("== sharedStrings 与类型 ==\n");
    {
        const QTableWidgetItem* a1 = viewer.cellAt(0, 0, 0);
        expect(a1 != nullptr && a1->text() == QStringLiteral("你好，世界"),
               "t=\"s\" resolves through sharedStrings");
        expect(a1 != nullptr && a1->font().bold(), "style font bold applied");
        expect(a1 != nullptr && a1->font().pointSizeF() > 13.9,
               "style font size applied");
        expect(a1 != nullptr && a1->foreground().color() == QColor(255, 0, 0),
               "font color applied");
        expect(a1 != nullptr && a1->background().color() == QColor(255, 255, 0),
               "solid fill applied");

        const QTableWidgetItem* b2 = viewer.cellAt(0, 1, 1);
        expect(b2 != nullptr && b2->text() == QStringLiteral("12.3%"),
               "custom number format 0.0% renders (12.3%)");
        const QTableWidgetItem* a2 = viewer.cellAt(0, 1, 0);
        expect(a2 != nullptr && a2->text() == QStringLiteral("3.14"),
               "builtin numFmt 2 (0.00) renders");
        const QTableWidgetItem* c1 = viewer.cellAt(0, 0, 2);
        expect(c1 != nullptr && c1->text() == QStringLiteral("TRUE"),
               "boolean cell renders TRUE/FALSE");
        const QTableWidgetItem* c2 = viewer.cellAt(0, 1, 2);
        expect(c2 != nullptr && c2->text() == QStringLiteral("内联"),
               "inlineStr cell renders");
        const QTableWidgetItem* b3 = viewer.cellAt(0, 2, 1);
        expect(b3 != nullptr && b3->text() == QStringLiteral("公式缓存"),
               "t=\"str\" (formula cache) renders");
        const QTableWidgetItem* a3 = viewer.cellAt(0, 2, 0);
        const QString expectedDate
            = QDate(1899, 12, 30).addDays(45000).toString(QStringLiteral("yyyy-MM-dd"));
        expect(a3 != nullptr && a3->text() == expectedDate,
               "date serial renders as yyyy-MM-dd");
    }

    std::printf("== 合并 / 列宽 / 行高 ==\n");
    {
        // 第一张表有 3 列（第二张只有 1 列），按尺寸选中，避免同名 objectName 混淆
        QTableWidget* table = nullptr;
        const auto tables = viewer.findChildren<QTableWidget*>();
        for (auto* candidate : tables) {
            if (candidate->columnCount() >= 3)
                table = candidate;
        }
        expect(table != nullptr, "sheet 1 table reachable");
        if (table) {
            const QTableWidgetItem* anchor = table->item(0, 1);
            expect(anchor != nullptr, "merged anchor cell exists");
            if (anchor) {
                const QRect b1 = table->visualItemRect(anchor);
                expect(b1.width() > table->columnWidth(1),
                       "mergeCell B1:C1 spans two columns");
                expect(anchor->text() == QStringLiteral("合并区标题"),
                       "merged anchor keeps the value");
            }
            // 列宽 25.5 字符 → qRound(25.5*7)+5 = 184px；行高 30pt → 40px
            expect(table->columnWidth(0) == 184, "custom column width applied");
            expect(table->rowHeight(0) == 40, "custom row height applied");
        }
    }

    std::printf("== 第二张表 ==\n");
    {
        const QTableWidgetItem* a1 = viewer.cellAt(1, 0, 0);
        expect(a1 != nullptr && a1->text() == QStringLiteral("富文本"),
               "second sheet cell resolves sharedStrings too");
    }

    // ---- 坏包：不崩溃、返回 false ----
    std::printf("== 容错 ==\n");
    {
        QTemporaryDir bad;
        const QString badPath = bad.filePath(QStringLiteral("bad.xlsx"));
        core::ZipWriter w(badPath);
        w.addFile(QStringLiteral("xl/workbook.xml"), QByteArrayLiteral("<workbook/>"));
        w.close();
        viewers::XlsxViewer badViewer(badPath);
        expect(badViewer.sheetCount() == 0, "workbook without sheets loads empty");
    }

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll xlsx rendering tests passed.\n");
    return 0;
}
