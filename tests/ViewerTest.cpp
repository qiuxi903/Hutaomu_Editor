// Hutaomu Editor - Document viewer tests (image/pdf/office/zip).
// SPDX-License-Identifier: LicenseRef-Proprietary
#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QKeyEvent>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <cstdio>

#include "viewers/ImageViewer.h"
#include "viewers/OfficeViewer.h"
#include "viewers/PdfViewer.h"
#include "viewers/ViewerFactory.h"
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

QByteArray makeDocxZip(const QByteArray& documentXml)
{
    // 用生产 ZipReader 的逆过程——这里直接构造一个 store-ZIP
    auto u16 = [](quint16 v) {
        return QByteArray(1, char(v & 0xFF)) + QByteArray(1, char((v >> 8) & 0xFF));
    };
    auto u32 = [](quint32 v) {
        return QByteArray(1, char(v & 0xFF)) + QByteArray(1, char((v >> 8) & 0xFF))
               + QByteArray(1, char((v >> 16) & 0xFF))
               + QByteArray(1, char((v >> 24) & 0xFF));
    };
    QByteArray zip;
    struct Entry { QByteArray name; QByteArray content; };
    const Entry entries[] = {
        { QByteArrayLiteral("word/document.xml"), documentXml },
    };
    QList<QByteArray> offsets;
    for (const Entry& e : entries) {
        offsets.append(u32(quint32(zip.size())));
        zip += QByteArrayLiteral("PK\x03\x04");
        zip += u16(20) + u16(0) + u16(0) + u16(0) + u16(0x21);
        quint32 crc = 0;
        // crc32 表驱动太重，测试里用 0 占位（ZipReader 不校验 CRC）
        zip += u32(crc);
        zip += u32(quint32(e.content.size()));
        zip += u32(quint32(e.content.size()));
        zip += u16(quint16(e.name.size())) + u16(0);
        zip += e.name;
        zip += e.content;
    }
    const quint32 cdStart = quint32(zip.size());
    for (int i = 0; i < 1; ++i) {
        const Entry& e = entries[i];
        zip += QByteArrayLiteral("PK\x01\x02");
        zip += u16(20) + u16(20) + u16(0) + u16(0) + u16(0) + u16(0x21);
        zip += u32(0); // crc
        zip += u32(quint32(e.content.size()));
        zip += u32(quint32(e.content.size()));
        zip += u16(quint16(e.name.size())) + u16(0) + u16(0) + u16(0) + u16(0);
        zip += u32(0);
        zip += offsets.at(i);
        zip += e.name;
    }
    const quint32 cdSize = quint32(zip.size()) - cdStart;
    zip += QByteArrayLiteral("PK\x05\x06");
    zip += u16(0) + u16(0) + u16(1) + u16(1);
    zip += u32(cdSize) + u32(cdStart) + u16(0);
    return zip;
}

} // namespace

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    QTemporaryDir dir;

    // --- 格式判定 ---
    expect(viewers::viewerKindForFile(QStringLiteral("a.png"))
               == viewers::ViewerKind::Image,
           "png detected as image");
    expect(viewers::viewerKindForFile(QStringLiteral("a.MP4"))
               == viewers::ViewerKind::Video,
           "mp4 detected as video (case-insensitive)");
    expect(viewers::viewerKindForFile(QStringLiteral("a.pdf"))
               == viewers::ViewerKind::Pdf,
           "pdf detected");
    expect(viewers::viewerKindForFile(QStringLiteral("a.docx"))
               == viewers::ViewerKind::Docx,
           "docx detected");
    expect(viewers::viewerKindForFile(QStringLiteral("a.xlsx"))
               == viewers::ViewerKind::Xlsx,
           "xlsx detected");
    expect(viewers::viewerKindForFile(QStringLiteral("a.pptx"))
               == viewers::ViewerKind::Pptx,
           "pptx detected");
    expect(viewers::viewerKindForFile(QStringLiteral("a.cpp"))
               == viewers::ViewerKind::None,
           "cpp stays text");

    // --- ZipReader：store 条目读取 ---
    {
        const QByteArray docXml =
            "<?xml version=\"1.0\"?><w:document><w:body><w:p><w:t>Hello"
            "</w:t><w:t>World</w:t></w:p></w:body></w:document>";
        const QByteArray zip = makeDocxZip(docXml);
        viewers::ZipReader reader(zip);
        expect(reader.isValid(), "zip reader validates central directory");
        expect(reader.hasEntry(QStringLiteral("word/document.xml")),
               "zip finds entry");
        const QByteArray content =
            reader.entry(QStringLiteral("word/document.xml"));
        expect(content == docXml, "store entry decompressed verbatim");
        expect(reader.entry(QStringLiteral("missing.xml")).isEmpty(),
               "missing entry returns empty");
    }

    // --- 图片查看器：加载 + 旋转 + 保存 ---
    {
        const QString path = dir.filePath(QStringLiteral("test.png"));
        QImage image(64, 48, QImage::Format_RGB32);
        image.fill(Qt::red);
        image.save(path);

        viewers::ImageViewer viewer(path);
        expect(viewer.filePath() == path, "image viewer loads file");
        expect(!viewer.isModified(), "image starts unmodified");

        // Ctrl+R 旋转（event() 快捷键路径）
        QKeyEvent rotatePress(QEvent::KeyPress, Qt::Key_R, Qt::ControlModifier);
        QApplication::sendEvent(&viewer, &rotatePress);
        expect(viewer.isModified(), "rotation marks viewer modified");

        const QString out = dir.filePath(QStringLiteral("rotated.png"));
        QImage rotated;
        expect(viewer.saveAs(out), "save writes rotated image");
        rotated.load(out);
        expect(rotated.width() == 48 && rotated.height() == 64,
               "saved image is rotated (w/h swapped)");
    }

    // --- DocxViewer：读取 + 编辑 + 保存 ---
    {
        const QString path = dir.filePath(QStringLiteral("test.docx"));
        const QByteArray docXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<w:document xmlns:w=\"http://schemas.openxmlformats.org/"
            "wordprocessingml/2006/main\"><w:body>"
            "<w:p><w:r><w:t>第一段</w:t></w:r></w:p>"
            "<w:p><w:r><w:t>Second &amp; line</w:t></w:r></w:p>"
            "</w:body></w:document>";
        QFile file(path);
        file.open(QIODevice::WriteOnly);
        file.write(makeDocxZip(docXml));
        file.close();

        viewers::DocxViewer viewer(path);
        auto* editor = viewer.findChild<QTextEdit*>(
            QStringLiteral("docxEditor"));
        expect(editor && editor->toPlainText().contains(QStringLiteral("第一段")),
               "docx text extracted into editor");
        expect(editor->toPlainText().contains(QStringLiteral("Second & line")),
               "xml entities unescaped");

        editor->setPlainText(QStringLiteral("第一段\n编辑过的内容"));
        expect(viewer.isModified(), "editing marks docx modified");
        expect(viewer.save(), "docx save succeeds");

        // 重新打开验证往返
        viewers::DocxViewer reloaded(path);
        auto* reloadedEditor = reloaded.findChild<QTextEdit*>(
            QStringLiteral("docxEditor"));
        expect(reloadedEditor
                   && reloadedEditor->toPlainText()
                          == QStringLiteral("第一段\n编辑过的内容"),
               "docx round-trips through save/reload");
    }

    // --- XlsxViewer（CSV 路径）：读取 + 编辑 + 保存 ---
    {
        const QString path = dir.filePath(QStringLiteral("test.csv"));
        QFile file(path);
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        file.write("name,qty\napple,3\nbanana,7\n");
        file.close();

        viewers::XlsxViewer viewer(path);
        auto* table = viewer.findChild<QTableWidget*>(
            QStringLiteral("xlsxTable"));
        expect(table && table->rowCount() == 3 && table->columnCount() == 2,
               "csv rows/columns loaded");
        expect(table->item(1, 0)->text() == QStringLiteral("apple"),
               "csv cell text correct");

        table->item(1, 1)->setText(QStringLiteral("10"));
        expect(viewer.isModified(), "cell edit marks csv modified");
        expect(viewer.save(), "csv save succeeds");

        viewers::XlsxViewer reloaded(path);
        auto* reloadedTable = reloaded.findChild<QTableWidget*>(
            QStringLiteral("xlsxTable"));
        expect(reloadedTable && reloadedTable->item(1, 1)
                                      && reloadedTable->item(1, 1)->text()
                                             == QStringLiteral("10"),
               "csv round-trips through save/reload");
    }

    // --- PptxViewer：多页文本 ---
    {
        const QString path = dir.filePath(QStringLiteral("test.pptx"));
        // 两页 slide XML 的 store-zip（slideN.xml 命名是解析入口）
        const QByteArray slide1 =
            "<?xml version=\"1.0\"?><p:sld xmlns:p=\"x\" xmlns:a=\"x\">"
            "<p:cSld><p:spTree><p:sp><p:txBody><a:p><a:r><a:t>Slide One"
            "</a:t></a:r></a:p></p:txBody></p:sp></p:spTree></p:cSld></p:sld>";
        const QByteArray slide2 =
            "<?xml version=\"1.0\"?><p:sld xmlns:p=\"x\" xmlns:a=\"x\">"
            "<p:cSld><p:spTree><p:sp><p:txBody><a:p><a:r><a:t>Slide Two"
            "</a:t></a:r></a:p></p:txBody></p:sp></p:spTree></p:cSld></p:sld>";
        // 复用 makeDocxZip 的单条目逻辑不够——直接手写两条目 zip
        auto u16 = [](quint16 v) {
            return QByteArray(1, char(v & 0xFF))
                   + QByteArray(1, char((v >> 8) & 0xFF));
        };
        auto u32 = [](quint32 v) {
            return QByteArray(1, char(v & 0xFF))
                   + QByteArray(1, char((v >> 8) & 0xFF))
                   + QByteArray(1, char((v >> 16) & 0xFF))
                   + QByteArray(1, char((v >> 24) & 0xFF));
        };
        const QByteArrayList names = {
            QByteArrayLiteral("ppt/slides/slide1.xml"),
            QByteArrayLiteral("ppt/slides/slide2.xml"),
        };
        const QByteArrayList contents = { slide1, slide2 };
        QByteArray zip;
        QList<QByteArray> offsets;
        for (int i = 0; i < 2; ++i) {
            offsets.append(u32(quint32(zip.size())));
            zip += QByteArrayLiteral("PK\x03\x04");
            zip += u16(20) + u16(0) + u16(0) + u16(0) + u16(0x21) + u32(0);
            zip += u32(quint32(contents.at(i).size()));
            zip += u32(quint32(contents.at(i).size()));
            zip += u16(quint16(names.at(i).size())) + u16(0);
            zip += names.at(i) + contents.at(i);
        }
        const quint32 cdStart = quint32(zip.size());
        for (int i = 0; i < 2; ++i) {
            zip += QByteArrayLiteral("PK\x01\x02");
            zip += u16(20) + u16(20) + u16(0) + u16(0) + u16(0) + u16(0x21);
            zip += u32(0);
            zip += u32(quint32(contents.at(i).size()));
            zip += u32(quint32(contents.at(i).size()));
            zip += u16(quint16(names.at(i).size()))
                   + u16(0) + u16(0) + u16(0) + u16(0) + u32(0);
            zip += offsets.at(i) + names.at(i);
        }
        const quint32 cdSize = quint32(zip.size()) - cdStart;
        zip += QByteArrayLiteral("PK\x05\x06");
        zip += u16(0) + u16(0) + u16(2) + u16(2) + u32(cdSize) + u32(cdStart)
               + u16(0);

        QFile file(path);
        file.open(QIODevice::WriteOnly);
        file.write(zip);
        file.close();

        viewers::PptxViewer viewer(path);
        const auto edits = viewer.findChildren<QTextEdit*>(
            QStringLiteral("pptxSlideEdit"));
        expect(edits.size() == 2,
               "pptx loads both slides into editors");
        // findChildren 顺序不保证；按内容集合断言
        QSet<QString> texts;
        for (const QTextEdit* e : edits)
            texts.insert(e->toPlainText());
        expect(texts.contains(QStringLiteral("Slide One"))
                   && texts.contains(QStringLiteral("Slide Two")),
               "pptx slide text correct");
    }

    // --- PdfViewer（pdfium 渲染）：真实渲染 + 损坏降级 ---
    {
        // 最小有效 PDF（一个内容流）
        const QString path = dir.filePath(QStringLiteral("mini.pdf"));
        QFile file(path);
        file.open(QIODevice::WriteOnly);
        // 需要完整结构（catalog/pages/page/font + xref），pdfium 才能加载
        file.write(
            "%PDF-1.4\n"
            "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
            "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n"
            "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
            "/Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>\nendobj\n"
            "4 0 obj\n<< /Length 42 >>\nstream\n"
            "BT /F1 24 Tf 100 700 Td (Hello PDF) Tj ET\n"
            "endstream\nendobj\n"
            "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n"
            "xref\n0 6\n"
            "0000000000 65535 f \n"
            "0000000009 00000 n \n"
            "0000000058 00000 n \n"
            "0000000115 00000 n \n"
            "0000000241 00000 n \n"
            "0000000336 00000 n \n"
            "trailer\n<< /Size 6 /Root 1 0 R >>\n"
            "startxref\n405\n%%EOF\n");
        file.close();

        viewers::PdfViewer viewer(path);
        expect(viewer.filePath() == path, "pdf viewer loads valid pdf");
        auto* canvas = viewer.findChild<QLabel*>(QStringLiteral("pdfCanvas"));
        expect(canvas && !canvas->pixmap().isNull(),
               "pdf page rendered to pixmap");
        if (canvas && !canvas->pixmap().isNull()) {
            const QImage img = canvas->pixmap().toImage();
            int nonWhite = 0;
            for (int y = 0; y < img.height(); y += 5)
                for (int x = 0; x < img.width(); x += 5)
                    if (img.pixel(x, y) != 0xFFFFFFFF)
                        ++nonWhite;
            expect(nonWhite > 0, "rendered page has content (not blank)");
        }
        // 翻页/缩放压力不崩
        for (int i = 0; i < 5; ++i) {
            viewer.zoomIn();
            viewer.zoomOut();
        }
        expect(true, "pdf zoom stress survives");
    }
    {
        const QString path = dir.filePath(QStringLiteral("broken.pdf"));
        QFile file(path);
        file.open(QIODevice::WriteOnly);
        file.write("not a real pdf");
        file.close();

        viewers::PdfViewer viewer(path);
        expect(viewer.filePath() == path, "pdf viewer opens damaged file");
        // pdfium 加载失败 -> 画布显示降级提示而不是崩溃
        auto* canvas = viewer.findChild<QLabel*>(QStringLiteral("pdfCanvas"));
        expect(canvas && !canvas->text().isEmpty(),
               "damaged pdf degrades to hint text");
    }

    // --- 工厂 ---
    {
        const QString path = dir.filePath(QStringLiteral("x.png"));
        QImage(8, 8, QImage::Format_RGB32).save(path);
        viewers::DocumentViewer* viewer = viewers::createViewer(path, nullptr);
        expect(viewer && qobject_cast<viewers::ImageViewer*>(viewer),
               "factory creates image viewer for png");
        delete viewer;
        expect(viewers::createViewer(QStringLiteral("x.txt"), nullptr)
                   == nullptr,
               "factory returns null for text files");
    }

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll viewer tests passed.\n");
    return 0;
}