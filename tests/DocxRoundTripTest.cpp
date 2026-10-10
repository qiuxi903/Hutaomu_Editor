// Hutaomu Editor - docx 编辑保存往返测试（round-trip：编辑 → 保存 → 重开 → 内容一致）。
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include <QApplication>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextEdit>
#include <cstdio>

#include "core/ZipWriter.h"
#include "viewers/OfficeViewer.h"
#include "viewers/ZipReader.h"

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

void pump()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
}

QByteArray makeDocx(const QString& heading, const QString& body, const QString& bold)
{
    core::ZipWriter writer(QStringLiteral("dummy"));
    Q_UNUSED(writer);
    Q_UNUSED(heading);
    Q_UNUSED(body);
    Q_UNUSED(bold);
    return {};
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    setvbuf(stdout, nullptr, _IONBF, 0);

    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("roundtrip.docx"));

    // ---- 1) 构造一份含格式的基本 docx ----
    {
        core::ZipWriter writer(path);
        writer.addFile(QStringLiteral("[Content_Types].xml"),
                       QByteArrayLiteral("<?xml version=\"1.0\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"xml\" ContentType=\"application/xml\"/></Types>"));
        writer.addFile(QStringLiteral("word/document.xml"),
                       QByteArrayLiteral("<?xml version=\"1.0\"?>"
                                         "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
                                         "<w:body>"
                                         "<w:p><w:pPr><w:pStyle w:val=\"Heading1\"/></w:pPr>"
                                         "<w:r><w:rPr><w:b/><w:sz w:val=\"48\"/></w:rPr>"
                                         "<w:t>标题一</w:t></w:r></w:p>"
                                         "<w:p><w:r><w:t>普通段落。</w:t></w:r></w:p>"
                                         "<w:p><w:r><w:rPr><w:i/><w:color w:val=\"FF0000\"/></w:rPr>"
                                         "<w:t>红色斜体</w:t></w:r></w:p>"
                                         "</w:body></w:document>"));
        writer.close();
    }

    std::printf("== 打开与渲染 ==\n");
    {
        viewers::DocxViewer viewer(path);
        viewer.resize(900, 700);
        viewer.show();
        pump();

        auto* editor = viewer.findChild<QTextEdit*>(QStringLiteral("docxEditor"));
        expect(editor != nullptr, "editor reachable");
        expect(!editor->isReadOnly(), "docx opens editable");
        expect(viewer.isModified() == false, "fresh open is not modified");
        expect(editor->toPlainText().contains(QStringLiteral("标题一")),
               "heading text loaded");
        expect(editor->toPlainText().contains(QStringLiteral("红色斜体")),
               "colored run loaded");

        std::printf("== 编辑 ==\n");
        // 在文档末尾追加一段
        QTextCursor cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertBlock();
        cursor.insertText(QStringLiteral("新增的段落。"));
        pump();
        expect(editor->toPlainText().contains(QStringLiteral("新增的段落。")),
               "new paragraph inserted");
        expect(viewer.isModified(), "edit marks modified");

        // 选中"红色斜体"并改为粗体
        cursor = editor->textCursor();
        const int pos = editor->toPlainText().indexOf(QStringLiteral("红色斜体"));
        cursor.setPosition(pos);
        cursor.setPosition(pos + 4, QTextCursor::KeepAnchor);
        QTextCharFormat boldFmt;
        boldFmt.setFontWeight(QFont::Bold);
        cursor.mergeCharFormat(boldFmt);
        editor->setTextCursor(cursor);
        pump();

        std::printf("== 保存 ==\n");
        expect(viewer.save(), "save succeeds");
        expect(!viewer.isModified(), "save clears modified flag");
    }

    std::printf("== 重开验证 ==\n");
    {
        viewers::DocxViewer viewer2(path);
        viewer2.resize(900, 700);
        viewer2.show();
        pump();

        auto* editor2 = viewer2.findChild<QTextEdit*>(QStringLiteral("docxEditor"));
        expect(editor2 != nullptr, "reopened editor reachable");
        const QString text = editor2 ? editor2->toPlainText() : QString();
        expect(text.contains(QStringLiteral("标题一")), "heading survived round-trip");
        expect(text.contains(QStringLiteral("普通段落")), "body survived round-trip");
        expect(text.contains(QStringLiteral("红色斜体")), "colored run survived round-trip");
        expect(text.contains(QStringLiteral("新增的段落")), "new paragraph survived round-trip");

        // 保存后的文件仍是合法 zip（ZipReader 能解）
        QFile file(path);
        file.open(QIODevice::ReadOnly);
        viewers::ZipReader check(file.readAll());
        expect(check.isValid(), "saved file is a valid ZIP");
        expect(check.hasEntry(QStringLiteral("word/document.xml")),
               "document.xml present in saved file");
    }

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll docx round-trip tests passed.\n");
    return 0;
}
