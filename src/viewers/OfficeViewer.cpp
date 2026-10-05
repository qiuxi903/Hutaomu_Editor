// Hutaomu Editor - Office viewers implementation.
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "OfficeViewer.h"

#include <QFileInfo>

#include <algorithm>
#include <QLabel>
#include <QListWidget>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QTextEdit>
#include <QVBoxLayout>

#include <zlib.h>

#include "viewers/ZipReader.h"

namespace viewers {

namespace {

// ---- 最小 ZIP 写出器（store 方式）+ CRC32 ----

quint32 crc32Of(const QByteArray& data)
{
    return quint32(::crc32(0u,
                           reinterpret_cast<const Bytef*>(data.constData()),
                           uInt(data.size())));
}

QByteArray u16le(quint16 v)
{
    QByteArray b;
    b += char(v & 0xFF);
    b += char((v >> 8) & 0xFF);
    return b;
}

QByteArray u32le(quint32 v)
{
    QByteArray b;
    b += char(v & 0xFF);
    b += char((v >> 8) & 0xFF);
    b += char((v >> 16) & 0xFF);
    b += char((v >> 24) & 0xFF);
    return b;
}

struct ZipEntry {
    QByteArray name;
    QByteArray content;
};

// 写出标准 ZIP（全部 store 不压缩——Word/Excel/WPS 均可打开）
QByteArray buildZip(const QList<ZipEntry>& entries)
{
    QByteArray zip;
    QList<QByteArray> offsets;
    for (const ZipEntry& e : entries) {
        offsets.append(u32le(quint32(zip.size())));
        zip += QByteArrayLiteral("PK\x03\x04");
        zip += u16le(20);          // version needed
        zip += u16le(0);           // flags
        zip += u16le(0);           // method = store
        zip += u16le(0);           // mod time
        zip += u16le(0x21);        // mod date (1980-01-01)
        zip += u32le(crc32Of(e.content));
        zip += u32le(quint32(e.content.size()));
        zip += u32le(quint32(e.content.size()));
        zip += u16le(quint16(e.name.size()));
        zip += u16le(0);           // extra len
        zip += e.name;
        zip += e.content;
    }
    const quint32 cdStart = quint32(zip.size());
    for (int i = 0; i < entries.size(); ++i) {
        const ZipEntry& e = entries.at(i);
        zip += QByteArrayLiteral("PK\x01\x02");
        zip += u16le(20);          // version made by
        zip += u16le(20);          // version needed
        zip += u16le(0);           // flags
        zip += u16le(0);           // method
        zip += u16le(0);
        zip += u16le(0x21);
        zip += u32le(crc32Of(e.content));
        zip += u32le(quint32(e.content.size()));
        zip += u32le(quint32(e.content.size()));
        zip += u16le(quint16(e.name.size()));
        zip += u16le(0);           // extra
        zip += u16le(0);           // comment
        zip += u16le(0);           // disk start
        zip += u16le(0);           // internal attrs
        zip += u32le(0);           // external attrs
        zip += offsets.at(i);      // local header offset
        zip += e.name;
    }
    const quint32 cdSize = quint32(zip.size()) - cdStart;
    // EOCD
    zip += QByteArrayLiteral("PK\x05\x06");
    zip += u16le(0);
    zip += u16le(0);
    zip += u16le(quint16(entries.size()));
    zip += u16le(quint16(entries.size()));
    zip += u32le(cdSize);
    zip += u32le(cdStart);
    zip += u16le(0);
    return zip;
}

QString escapeXml(QString text)
{
    text.replace(QStringLiteral("&"), QStringLiteral("&amp;"));
    text.replace(QStringLiteral("<"), QStringLiteral("&lt;"));
    text.replace(QStringLiteral(">"), QStringLiteral("&gt;"));
    text.replace(QStringLiteral("\""), QStringLiteral("&quot;"));
    return text;
}

} // namespace

namespace ooxml {

int slideNumber(const QString& name)
{
    // ppt/slides/slide12.xml -> 12（解析失败返回 0）
    static const QRegularExpression numRe(QStringLiteral("slide(\\d+)\\.xml$"));
    const auto m = numRe.match(name);
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

QString unescapeXml(QString text)
{
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return text;
}

QStringList extractRuns(const QString& xml, const QString& tag)
{
    QStringList runs;
    const QRegularExpression openRe(QStringLiteral("<%1(?:\\s[^>]*)?>").arg(tag));
    const QRegularExpression closeRe(QStringLiteral("</%1\\s*>").arg(tag));
    qsizetype pos = 0;
    while (true) {
        const auto openMatch = openRe.match(xml, pos);
        if (!openMatch.hasMatch())
            break;
        const qsizetype open = openMatch.capturedStart();
        const auto closeMatch = closeRe.match(xml, open);
        if (!closeMatch.hasMatch())
            break;
        const qsizetype close = closeMatch.capturedStart();
        runs.append(unescapeXml(xml.mid(openMatch.capturedEnd(),
                                         close - openMatch.capturedEnd())));
        pos = closeMatch.capturedEnd();
    }
    return runs;
}

} // namespace ooxml

// ==================== DocxViewer ====================

DocxViewer::DocxViewer(const QString& filePath, QWidget* parent)
    : DocumentViewer(parent)
    , m_filePath(filePath)
{
    setObjectName(QStringLiteral("docxViewer"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_editor = new QTextEdit(this);
    m_editor->setObjectName(QStringLiteral("docxEditor"));
    m_editor->setAcceptRichText(false);
    layout->addWidget(m_editor);

    connect(m_editor->document(), &QTextDocument::contentsChanged, this, [this] {
        if (!m_modified) {
            m_modified = true;
            emit modifiedChanged(true);
        }
    });

    loadDocx();
}

bool DocxViewer::loadDocx()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    m_originalZip = file.readAll();

    ZipReader zip(m_originalZip);
    if (!zip.isValid())
        return false;
    const QByteArray document = zip.entry(QStringLiteral("word/document.xml"));
    if (document.isEmpty())
        return false;

    const QString xml = QString::fromUtf8(document);
    // 段落按 <w:p ...> 或 <w:p> 切分（我们保存的最小包是无属性的
    // <w:p>，外部软件生成的一般是 <w:p w:rsid...>），段内拼接 <w:t> 文本
    static const QRegularExpression paraOpen(QStringLiteral("<w:p(?:\\s[^>]*)?>"));
    QStringList paragraphs;
    qsizetype pos = 0;
    while (true) {
        const auto paraMatch = paraOpen.match(xml, pos);
        if (!paraMatch.hasMatch())
            break;
        const qsizetype close = xml.indexOf(QStringLiteral("</w:p>"),
                                            paraMatch.capturedEnd());
        const qsizetype end = close < 0 ? xml.size() : close;
        paragraphs.append(xml.mid(paraMatch.capturedEnd(),
                                  end - paraMatch.capturedEnd()));
        pos = close < 0 ? xml.size() : close + 5;
    }
    QString text;
    for (const QString& para : paragraphs) {
        const QStringList runs = ooxml::extractRuns(para, QStringLiteral("w:t"));
        text += runs.join(QString());
        text += QLatin1Char('\n');
    }
    m_editor->setPlainText(text.trimmed());
    m_editor->document()->setModified(false);
    return true;
}

bool DocxViewer::isModified() const
{
    return m_modified;
}

// 保存：重建最小 OOXML 包（段落文本完整保真；样式/图片等复杂内容 v1
// 不保留，保存前有状态栏提示）。
bool DocxViewer::save()
{
    const QStringList paragraphs =
        m_editor->toPlainText().split(QLatin1Char('\n'));
    QString body;
    for (const QString& para : paragraphs) {
        body += QStringLiteral("<w:p><w:r><w:t xml:space=\"preserve\">%1"
                               "</w:t></w:r></w:p>")
                    .arg(escapeXml(para));
    }
    const QByteArray documentXml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/"
        "wordprocessingml/2006/main\"><w:body>%1"
        "<w:sectPr/></w:body></w:document>")
                                       .arg(body)
                                       .toUtf8();

    const QByteArray contentTypes =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/"
        "content-types\"><Default Extension=\"rels\" ContentType="
        "\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/word/document.xml\" ContentType="
        "\"application/vnd.openxmlformats-officedocument."
        "wordprocessingml.document.main+xml\"/></Types>";
    const QByteArray rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/"
        "2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://"
        "schemas.openxmlformats.org/officeDocument/2006/relationships/"
        "officeDocument\" Target=\"word/document.xml\"/></Relationships>";

    const QByteArray zip = buildZip({
        { QByteArrayLiteral("[Content_Types].xml"), contentTypes },
        { QByteArrayLiteral("_rels/.rels"), rels },
        { QByteArrayLiteral("word/document.xml"), documentXml },
    });

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const bool ok = file.write(zip) == zip.size();
    if (ok) {
        m_modified = false;
        emit modifiedChanged(false);
    }
    return ok;
}

// ==================== XlsxViewer ====================

XlsxViewer::XlsxViewer(const QString& filePath, QWidget* parent)
    : DocumentViewer(parent)
    , m_filePath(filePath)
    , m_isCsv(filePath.endsWith(QStringLiteral(".csv"),
                                Qt::CaseInsensitive))
{
    setObjectName(QStringLiteral("xlsxViewer"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("xlsxTable"));
    layout->addWidget(m_table);

    connect(m_table, &QTableWidget::cellChanged,
            this, &XlsxViewer::cellChanged);

    if (m_isCsv)
        loadCsv();
    else
        loadXlsx();
}

bool XlsxViewer::loadXlsx()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    m_originalZip = file.readAll();

    ZipReader zip(m_originalZip);
    if (!zip.isValid())
        return false;
    const QByteArray sheet = zip.entry(QStringLiteral("xl/worksheets/sheet1.xml"));
    if (sheet.isEmpty())
        return false;
    const QString xml = QString::fromUtf8(sheet);

    // 行 = <row>；单元格 = <c r="A1" t="..."><v>值</v></c>
    const QRegularExpression rowRe(QStringLiteral("<row(?:\\s[^>]*)?>"));
    const QRegularExpression cellRe(
        QStringLiteral("<c(?:\\s[^>]*?)?r=\"([A-Z]+)(\\d+)\"[^>]*>(.*?)</c>"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression valueRe(
        QStringLiteral("<v(?:\\s[^>]*)?>([^<]*)</v>"));

    int maxRow = 0;
    int maxCol = 0;
    struct Cell { int row; int col; QString text; };
    QList<Cell> cells;

    qsizetype pos = 0;
    while (true) {
        const qsizetype rowStart = xml.indexOf(rowRe, pos);
        if (rowStart < 0)
            break;
        const qsizetype rowEnd = xml.indexOf(QStringLiteral("</row>"), rowStart);
        if (rowEnd < 0)
            break;
        const QString rowXml = xml.mid(rowStart, rowEnd - rowStart);

        qsizetype cpos = 0;
        while (true) {
            const auto cell = cellRe.match(rowXml, cpos);
            if (!cell.hasMatch())
                break;
            // 列字母 -> 数字
            const QString letters = cell.captured(1);
            int col = 0;
            for (QChar ch : letters)
                col = col * 26 + (ch.toLatin1() - 'A' + 1);
            const int row = cell.captured(2).toInt();
            const auto value = valueRe.match(cell.captured(3));
            cells.append({ row, col,
                           ooxml::unescapeXml(
                               value.hasMatch() ? value.captured(1)
                                                : QString()) });
            maxRow = qMax(maxRow, row);
            maxCol = qMax(maxCol, col);
            cpos = cell.capturedEnd();
        }
        pos = rowEnd;
    }

    m_loading = true;
    m_table->clear();
    m_table->setRowCount(maxRow);
    m_table->setColumnCount(maxCol);
    for (const Cell& c : cells) {
        auto* item = new QTableWidgetItem(c.text);
        m_table->setItem(c.row - 1, c.col - 1, item);
    }
    m_table->resizeColumnsToContents();
    m_loading = false;
    return true;
}

bool XlsxViewer::loadCsv()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    QList<QStringList> rows;
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        rows.append(line.split(QLatin1Char(',')));
    }
    m_loading = true;
    int maxCols = 0;
    for (const QStringList& r : rows)
        maxCols = qMax(maxCols, r.size());
    m_table->setRowCount(rows.size());
    m_table->setColumnCount(qMax(1, maxCols));
    for (int r = 0; r < rows.size(); ++r) {
        for (int c = 0; c < rows.at(r).size(); ++c)
            m_table->setItem(r, c, new QTableWidgetItem(rows.at(r).at(c)));
    }
    m_loading = false;
    return true;
}

bool XlsxViewer::saveCsv()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < m_table->columnCount(); ++c) {
            const QTableWidgetItem* item = m_table->item(r, c);
            cells.append(item ? item->text() : QString());
        }
        stream << cells.join(QLatin1Char(',')) << "\n";
    }
    return true;
}

bool XlsxViewer::isModified() const
{
    return m_modified;
}

bool XlsxViewer::save()
{
    if (m_isCsv)
        return saveCsv();

    // xlsx：重建最小 OOXML 包（当前表格内容；v1 单表）
    QString rows;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QString cells;
        for (int c = 0; c < m_table->columnCount(); ++c) {
            const QTableWidgetItem* item = m_table->item(r, c);
            if (!item || item->text().isEmpty())
                continue;
            const QString letters = [c]() {
                QString s;
                int v = c + 1;
                while (v > 0) {
                    s.prepend(QChar(char('A' + (v - 1) % 26)));
                    v = (v - 1) / 26;
                }
                return s;
            }();
            cells += QStringLiteral("<c r=\"%1%2\"><v>%3</v></c>")
                         .arg(letters)
                         .arg(r + 1)
                         .arg(escapeXml(item->text()));
        }
        rows += QStringLiteral("<row r=\"%1\">%2</row>").arg(r + 1).arg(cells);
    }
    const QByteArray sheetXml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/"
        "2006/main\"><sheetData>%1</sheetData></worksheet>")
                                    .arg(rows)
                                    .toUtf8();
    const QByteArray contentTypes =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/"
        "content-types\"><Default Extension=\"rels\" ContentType="
        "\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/"
        "vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType="
        "\"application/vnd.openxmlformats-officedocument.spreadsheetml."
        "worksheet+xml\"/></Types>";
    const QByteArray rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/"
        "2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://"
        "schemas.openxmlformats.org/officeDocument/2006/relationships/"
        "officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>";
    const QByteArray workbook =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/"
        "2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/"
        "officeDocument/2006/relationships\"><sheets><sheet name=\"Sheet1\""
        " sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>";
    const QByteArray wbRels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/"
        "2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://"
        "schemas.openxmlformats.org/officeDocument/2006/relationships/"
        "worksheet\" Target=\"worksheets/sheet1.xml\"/></Relationships>";

    const QByteArray zip = buildZip({
        { QByteArrayLiteral("[Content_Types].xml"), contentTypes },
        { QByteArrayLiteral("_rels/.rels"), rels },
        { QByteArrayLiteral("xl/workbook.xml"), workbook },
        { QByteArrayLiteral("xl/_rels/workbook.xml.rels"), wbRels },
        { QByteArrayLiteral("xl/worksheets/sheet1.xml"), sheetXml },
    });

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const bool ok = file.write(zip) == zip.size();
    if (ok) {
        m_modified = false;
        emit modifiedChanged(false);
    }
    return ok;
}

void XlsxViewer::cellChanged(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    if (!m_loading && !m_modified) {
        m_modified = true;
        emit modifiedChanged(true);
    }
}

// ==================== PptxViewer ====================

PptxViewer::PptxViewer(const QString& filePath, QWidget* parent)
    : DocumentViewer(parent)
    , m_filePath(filePath)
{
    setObjectName(QStringLiteral("pptxViewer"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_slideList = new QListWidget(this);
    m_slideList->setObjectName(QStringLiteral("pptxSlideList"));
    m_slideList->setFixedWidth(160);
    layout->addWidget(m_slideList);

    m_slides = new QStackedWidget(this);
    m_slides->setObjectName(QStringLiteral("pptxSlides"));
    layout->addWidget(m_slides, 1);

    connect(m_slideList, &QListWidget::currentRowChanged,
            m_slides, &QStackedWidget::setCurrentIndex);

    loadPptx();
}

bool PptxViewer::loadPptx()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    m_originalZip = file.readAll();

    ZipReader zip(m_originalZip);
    if (!zip.isValid())
        return false;

    // 幻灯片 = ppt/slides/slideN.xml（按编号排序——字符串排序会出
    // slide10 < slide2 的错序，必须数字比较）
    QStringList slideNames;
    for (const QString& name : zip.entries()) {
        if (name.startsWith(QStringLiteral("ppt/slides/slide"))
            && name.endsWith(QStringLiteral(".xml")))
            slideNames.append(name);
    }
    std::sort(slideNames.begin(), slideNames.end(),
              [](const QString& a, const QString& b) {
                  const int numA = ooxml::slideNumber(a);
                  const int numB = ooxml::slideNumber(b);
                  if (numA != numB)
                      return numA < numB;
                  return a < b;
              });
    if (slideNames.isEmpty())
        return false;

    for (int i = 0; i < slideNames.size(); ++i) {
        const QString xml = QString::fromUtf8(zip.entry(slideNames.at(i)));
        const QStringList texts = ooxml::extractRuns(xml, QStringLiteral("a:t"));
        auto* edit = new QTextEdit(m_slides);
        edit->setObjectName(QStringLiteral("pptxSlideEdit"));
        edit->setAcceptRichText(false);
        edit->setPlainText(texts.join(QLatin1Char('\n')));
        m_slides->addWidget(edit);
        m_slideList->addItem(QStringLiteral("幻灯片 %1").arg(i + 1));

        connect(edit->document(), &QTextDocument::contentsChanged, this, [this] {
            if (!m_modified) {
                m_modified = true;
                emit modifiedChanged(true);
            }
        });
    }
    m_slideList->setCurrentRow(0);
    return true;
}

bool PptxViewer::isModified() const
{
    return m_modified;
}

bool PptxViewer::save()
{
    // 重建最小 pptx：每页文本框内容
    QList<ZipEntry> entries;
    QByteArray contentTypes =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/"
        "content-types\"><Default Extension=\"rels\" ContentType="
        "\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>";
    QByteArray slidesXml;
    QByteArray slideRels;
    for (int i = 0; i < m_slides->count(); ++i) {
        auto* edit = qobject_cast<QTextEdit*>(m_slides->widget(i));
        if (!edit)
            continue;
        QString shapes;
        const QStringList lines = edit->toPlainText().split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            shapes += QStringLiteral(
                "<p:sp><p:txBody><a:p><a:r><a:t>%1</a:t></a:r></a:p>"
                "</p:txBody></p:sp>")
                .arg(escapeXml(line));
        }
        const QByteArray slide = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<p:sld xmlns:p=\"http://schemas.openxmlformats.org/"
            "presentationml/2006/main\" xmlns:a=\"http://schemas."
            "openxmlformats.org/drawingml/2006/main\"><p:cSld>"
            "<p:spTree>%1</p:spTree></p:cSld></p:sld>")
                                         .arg(shapes)
                                         .toUtf8();
        entries.append({ QStringLiteral("ppt/slides/slide%1.xml").arg(i + 1)
                             .toUtf8(),
                         slide });
        contentTypes += QStringLiteral(
            "<Override PartName=\"/ppt/slides/slide%1.xml\" ContentType="
            "\"application/vnd.openxmlformats-officedocument."
            "presentationml.slide+xml\"/>")
                            .arg(i + 1)
                            .toUtf8();
    }
    contentTypes += QStringLiteral("</Types>").toUtf8();

    const QByteArray rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/"
        "2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://"
        "schemas.openxmlformats.org/officeDocument/2006/relationships/"
        "officeDocument\" Target=\"ppt/presentation.xml\"/></Relationships>";
    const QByteArray presentation = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<p:presentation xmlns:p=\"http://schemas.openxmlformats.org/"
        "presentationml/2006/main\" xmlns:r=\"http://schemas."
        "openxmlformats.org/officeDocument/2006/relationships\">"
        "<p:sldIdLst>%1</p:sldIdLst></p:presentation>")
                                        .arg([&] {
                                            QString ids;
                                            for (int i = 0;
                                                 i < m_slides->count(); ++i)
                                                ids += QStringLiteral(
                                                    "<p:sldId id=\"%1\" "
                                                    "r:id=\"rId%2\"/>")
                                                    .arg(256 + i)
                                                    .arg(i + 1);
                                            return ids;
                                        }())
                                        .toUtf8();

    entries.append({ QByteArrayLiteral("[Content_Types].xml"), contentTypes });
    entries.append({ QByteArrayLiteral("_rels/.rels"), rels });
    entries.append({ QByteArrayLiteral("ppt/presentation.xml"), presentation });

    const QByteArray zip = buildZip(entries);
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const bool ok = file.write(zip) == zip.size();
    if (ok) {
        m_modified = false;
        emit modifiedChanged(false);
    }
    return ok;
}

} // namespace viewers