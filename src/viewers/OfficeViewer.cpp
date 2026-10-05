// Hutaomu Editor - Office viewers implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "OfficeViewer.h"

#include <QFileInfo>

#include <algorithm>
#include <QLabel>
#include <QSizePolicy>
#include <QSizeF>
#include <QPushButton>
#include <QPainter>
#include <QPalette>
#include <QImage>
#include <QListWidget>
#include <QLineEdit>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QTabWidget>
#include <QColor>
#include <QFont>
#include <QBrush>
#include <QDate>
#include <QDateTime>
#include <QVector>
#include <QHash>
#include <cmath>
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

    // 纸张页面感：灰底画布 + 白色纸面（外边距）+ 文档内边距，底部状态条
    auto* canvas = new QWidget(this);
    canvas->setObjectName(QStringLiteral("docxPageCanvas"));
    canvas->setAutoFillBackground(true);
    {
        // 灰底从控件调色板派生（不依赖主题模块，明暗主题下都自然）
        QPalette canvasPalette = canvas->palette();
        canvasPalette.setColor(QPalette::Window,
                               canvasPalette.color(QPalette::Window).darker(112));
        canvas->setPalette(canvasPalette);
    }
    auto* canvasLayout = new QVBoxLayout(canvas);
    canvasLayout->setContentsMargins(24, 20, 24, 12);

    m_editor = new QTextEdit(canvas);
    m_editor->setObjectName(QStringLiteral("docxEditor"));
    m_editor->setAcceptRichText(false);
    m_editor->setFrameShape(QFrame::NoFrame);
    {
        QPalette editorPalette = m_editor->palette();
        editorPalette.setColor(QPalette::Base, Qt::white);
        m_editor->setPalette(editorPalette);
    }
    QFont docFont(QStringLiteral("Microsoft YaHei UI"), 11);
    docFont.setStyleHint(QFont::SansSerif);
    m_editor->setFont(docFont);
    m_editor->document()->setDocumentMargin(44); // 纸张内边距
    canvasLayout->addWidget(m_editor, 1);
    layout->addWidget(canvas, 1);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setObjectName(QStringLiteral("docxStats"));
    m_statsLabel->setContentsMargins(12, 4, 12, 6);
    layout->addWidget(m_statsLabel);

    connect(m_editor->document(), &QTextDocument::contentsChanged, this, [this] {
        if (!m_loading && !m_modified) {
            m_modified = true;
            emit modifiedChanged(true);
        }
    });

    loadDocx();
}


// ---- docx → HTML：忠实只读渲染 ----
namespace {

// <w:p …>…</w:p> 的内容 → HTML 片段（标题/对齐/列表/字符格式）
QString docxParagraphToHtml(const QString& paraXml)
{
    // 段落属性
    QString pPr;
    const auto pPrMatch = QRegularExpression(
                              QStringLiteral("<w:pPr>(.*?)</w:pPr>"),
                              QRegularExpression::DotMatchesEverythingOption)
                              .match(paraXml);
    if (pPrMatch.hasMatch())
        pPr = pPrMatch.captured(1);

    const QString style = ooxml::unescapeXml(
        QRegularExpression(QStringLiteral("<w:pStyle w:val=\"([^\"]*)\"/>")).match(pPr)
            .captured(1));
    const QString align = ooxml::unescapeXml(
        QRegularExpression(QStringLiteral("<w:jc w:val=\"([^\"]*)\"/>")).match(pPr)
            .captured(1));
    const bool isListItem = pPr.contains(QLatin1String("<w:numPr>"));

    // 标题级别（Heading1..6 / Title；兼容中文样式名"标题 1"）
    int heading = 0;
    static const QRegularExpression headingRe(
        QStringLiteral("(?:Heading|标题)\\s*([1-6])"),
        QRegularExpression::CaseInsensitiveOption);
    const auto headingMatch = headingRe.match(style);
    if (headingMatch.hasMatch())
        heading = headingMatch.captured(1).toInt();
    else if (style.compare(QLatin1String("Title"), Qt::CaseInsensitive) == 0
             || style == QLatin1String("标题"))
        heading = 1;

    // run 内容
    QString runs;
    static const QRegularExpression runRe(QStringLiteral("<w:r(?:\\s[^>]*)?>(.*?)</w:r>"),
                                           QRegularExpression::DotMatchesEverythingOption);
    auto runIt = runRe.globalMatch(paraXml);
    while (runIt.hasNext()) {
        const QString run = runIt.next().captured(1);
        const auto rPr = QRegularExpression(QStringLiteral("<w:rPr>(.*?)</w:rPr>"),
                                            QRegularExpression::DotMatchesEverythingOption)
                             .match(run);
        const QString rPrXml = rPr.hasMatch() ? rPr.captured(1) : QString();

        QString styles;
        if (rPrXml.contains(QLatin1String("<w:b/>")))
            styles += QLatin1String("font-weight:700;");
        if (rPrXml.contains(QLatin1String("<w:i/>")))
            styles += QLatin1String("font-style:italic;");
        if (rPrXml.contains(QLatin1String("<w:strike/>")))
            styles += QLatin1String("text-decoration:line-through;");
        if (rPrXml.contains(QLatin1String("<w:u ")))
            styles += QLatin1String("text-decoration:underline;");
        const auto sz = QRegularExpression(QStringLiteral("<w:sz w:val=\"(\\d+)\"/>"))
                            .match(rPrXml);
        if (sz.hasMatch())
            styles += QStringLiteral("font-size:%1pt;").arg(
                sz.captured(1).toDouble() / 2.0);
        const auto color = QRegularExpression(
                               QStringLiteral("<w:color w:val=\"([0-9A-Fa-f]{6})\"/>"))
                               .match(rPrXml);
        if (color.hasMatch())
            styles += QStringLiteral("color:#%1;").arg(color.captured(1));
        if (rPrXml.contains(QStringLiteral("w:val=\"superscript\"")))
            styles += QLatin1String("vertical-align:super;font-size:0.8em;");
        if (rPrXml.contains(QStringLiteral("w:val=\"subscript\"")))
            styles += QLatin1String("vertical-align:sub;font-size:0.8em;");

        // 文本：w:t（保留空格）、w:tab → 空格、w:br → 换行
        QString text;
        static const QRegularExpression tRe(QStringLiteral("<w:t(?:\\s[^>]*)?>(.*?)</w:t>"),
                                             QRegularExpression::DotMatchesEverythingOption);
        auto tIt = tRe.globalMatch(run);
        while (tIt.hasNext())
            text += ooxml::unescapeXml(tIt.next().captured(1));
        if (run.contains(QLatin1String("<w:tab/>")))
            text += QLatin1String("&nbsp;&nbsp;&nbsp;&nbsp;");
        if (run.contains(QLatin1String("<w:br/>")))
            text += QLatin1String("<br/>");
        if (text.isEmpty())
            continue;
        runs += styles.isEmpty()
                    ? text
                    : QStringLiteral("<span style=\"%1\">%2</span>").arg(styles, text);
    }

    // 组装段落
    QString tag = QStringLiteral("p");
    QString tagStyle;
    if (heading > 0) {
        tag = QStringLiteral("h%1").arg(heading);
    }
    if (align == QLatin1String("center"))
        tagStyle += QLatin1String("text-align:center;");
    else if (align == QLatin1String("right"))
        tagStyle += QLatin1String("text-align:right;");
    else if (align == QLatin1String("both"))
        tagStyle += QLatin1String("text-align:justify;");
    if (isListItem)
        tagStyle += QLatin1String("margin-left:2.5em;");

    if (runs.isEmpty() && heading == 0)
        return QStringLiteral("<p style=\"margin:4px 0;\">&nbsp;</p>");

    QString openTag = QLatin1Char('<') + tag;
    if (!tagStyle.isEmpty())
        openTag += QStringLiteral(" style=\"%1\"").arg(tagStyle);
    openTag += QLatin1Char('>');

    QString body = runs;
    if (isListItem && body.startsWith(QLatin1Char('<')) == false)
        body = QStringLiteral("\u2022 ") + body;
    return openTag + body + QStringLiteral("</%1>").arg(tag);
}

// <w:tbl>…</w:tbl> → HTML 表格（递归渲染单元格里的段落）
QString docxTableToHtml(const QString& tableXml)
{
    QString html = QStringLiteral("<table border=\"0\" cellspacing=\"0\" "
                                  "cellpadding=\"4\" "
                                  "style=\"border-collapse:collapse;\">");
    static const QRegularExpression rowRe(QStringLiteral("<w:tr(?:\\s[^>]*)?>(.*?)</w:tr>"),
                                           QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression cellRe(QStringLiteral("<w:tc>(.*?)</w:tc>"),
                                            QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression spanRe(QStringLiteral("<w:gridSpan w:val=\"(\\d+)\"/>"));
    static const QRegularExpression paraRe(
        QStringLiteral("<w:p(?:\\s[^>]*)?>(.*?)</w:p>"),
        QRegularExpression::DotMatchesEverythingOption);

    auto rowIt = rowRe.globalMatch(tableXml);
    while (rowIt.hasNext()) {
        html += QLatin1String("<tr>");
        auto cellIt = cellRe.globalMatch(rowIt.next().captured(1));
        while (cellIt.hasNext()) {
            const QString cell = cellIt.next().captured(1);
            const auto span = spanRe.match(cell);
            QString attrs;
            if (span.hasMatch() && span.captured(1).toInt() > 1)
                attrs += QStringLiteral(" colspan=\"%1\"").arg(span.captured(1));
            const auto width = QRegularExpression(
                                   QStringLiteral("<w:tcW w:w=\"(\\d+)\""))
                                   .match(cell);
            if (width.hasMatch()) {
                const int px = qBound(40, width.captured(1).toInt() / 15, 1200);
                attrs += QStringLiteral(" width=\"%1\"").arg(px);
            }
            html += QLatin1String("<td style=\"border:1px solid #b9c0bd;padding:4px 8px;\"")
                    + attrs + QLatin1Char('>');
            auto paraIt = paraRe.globalMatch(cell);
            while (paraIt.hasNext())
                html += docxParagraphToHtml(paraIt.next().captured(1));
            html += QLatin1String("</td>");
        }
        html += QLatin1String("</tr>");
    }
    return html + QLatin1String("</table><p></p>");
}

} // namespace

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

    m_loading = true;

    const QString xml = QString::fromUtf8(document);
    // 按顺序走 body 的顶层块：段落与表格交错渲染
    QString html = QStringLiteral(
        // 注意：不要在 CSS 里写死 font-size —— 查看器缩放改的是文档默认字体，
        // CSS 指定的大小会把它压住导致缩放无效。
        "<html><head><style>"
        "body { font-family:'Microsoft YaHei UI','Segoe UI',sans-serif;"
        " color:#24292e; line-height:1.6; }"
        "h1 { font-size:2em; margin:12px 0 6px; }"
        "h2 { font-size:1.6em; margin:10px 0 6px; }"
        "h3 { font-size:1.3em; margin:8px 0 5px; }"
        "h4,h5,h6 { font-size:1.1em; margin:6px 0 4px; }"
        "p { margin:5px 0; }"
        "</style></head><body>");

    qsizetype pos = xml.indexOf(QStringLiteral("<w:body>"));
    const qsizetype bodyEnd = xml.lastIndexOf(QStringLiteral("</w:body>"));
    if (pos < 0)
        pos = 0;
    const qsizetype limit = bodyEnd > 0 ? bodyEnd : xml.size();

    while (pos < limit) {
        const auto pMatch = QRegularExpression(QStringLiteral("<w:p(?:\\s[^>]*)?>"))
                                .match(xml, pos);
        const auto tblMatch = QRegularExpression(QStringLiteral("<w:tbl>")).match(xml, pos);
        // 选更靠前的块
        const bool hasP = pMatch.hasMatch() && pMatch.capturedStart() < limit;
        const bool hasT = tblMatch.hasMatch() && tblMatch.capturedStart() < limit;
        if (!hasP && !hasT)
            break;
        if (hasP && (!hasT || pMatch.capturedStart() <= tblMatch.capturedStart())) {
            const qsizetype close = xml.indexOf(QStringLiteral("</w:p>"),
                                                pMatch.capturedEnd());
            if (close < 0)
                break;
            html += docxParagraphToHtml(
                xml.mid(pMatch.capturedEnd(), close - pMatch.capturedEnd()));
            pos = close + 6;
        } else {
            // 表格：深度计数找匹配的 </w:tbl>（兼容嵌套表格）
            int depth = 1;
            qsizetype cursor = tblMatch.capturedEnd();
            qsizetype close = -1;
            while (cursor < limit) {
                const auto open = QRegularExpression(QStringLiteral("<w:tbl>"))
                                      .match(xml, cursor);
                const auto closeM = xml.indexOf(QStringLiteral("</w:tbl>"), cursor);
                if (closeM < 0)
                    break;
                if (open.hasMatch() && open.capturedStart() < closeM) {
                    ++depth;
                    cursor = open.capturedEnd();
                } else {
                    --depth;
                    cursor = closeM + 8;
                    if (depth == 0) {
                        close = closeM;
                        break;
                    }
                }
            }
            if (close < 0)
                break;
            html += docxTableToHtml(
                xml.mid(tblMatch.capturedStart(), close + 8 - tblMatch.capturedStart()));
            pos = close + 8;
        }
    }
    html += QLatin1String("</body></html>");

    m_editor->setReadOnly(true);
    m_editor->setHtml(html);
    m_editor->document()->setModified(false);
    m_loading = false;
    updatePageStats();
    return true;
}

// 底部状态：字数 / 段落数
void DocxViewer::updatePageStats()
{
    if (!m_statsLabel || !m_editor)
        return;
    const QString text = m_editor->toPlainText();
    int characters = 0;
    for (const QChar ch : text) {
        if (!ch.isSpace())
            ++characters;
    }
    int paragraphs = 0;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        if (!line.trimmed().isEmpty())
            ++paragraphs;
    }
    m_statsLabel->setText(tr("共 %1 字 · %2 段").arg(characters).arg(paragraphs));
}

// docx 缩放：调整文档默认字体（CSS 里没有写死字号，故有效）
void DocxViewer::zoomIn()
{
    m_editor->zoomIn(1);
}

void DocxViewer::zoomOut()
{
    m_editor->zoomOut(1);
}

void DocxViewer::resetZoom()
{
    QFont font(QStringLiteral("Microsoft YaHei UI"), 11);
    font.setStyleHint(QFont::SansSerif);
    m_editor->setFont(font);
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
    layout->setSpacing(4);

    // 名称框 + fx 编辑栏（WPS/Excel 形态：点单元格看内容，在栏里改也行）
    auto* barRow = new QHBoxLayout;
    barRow->setContentsMargins(6, 6, 6, 0);
    barRow->setSpacing(6);
    m_nameBox = new QLineEdit(this);
    m_nameBox->setObjectName(QStringLiteral("xlsxNameBox"));
    m_nameBox->setFixedWidth(96);
    m_nameBox->setReadOnly(true);
    m_nameBox->setAlignment(Qt::AlignCenter);
    auto* fxLabel = new QLabel(QStringLiteral("fx"), this);
    fxLabel->setObjectName(QStringLiteral("xlsxFxLabel"));
    m_formulaBox = new QLineEdit(this);
    m_formulaBox->setObjectName(QStringLiteral("xlsxFormulaBox"));
    m_formulaBox->setPlaceholderText(tr("选中单元格查看内容；在此输入后按回车写入"));
    barRow->addWidget(m_nameBox);
    barRow->addWidget(fxLabel);
    m_statsLabel = new QLabel(this);
    m_statsLabel->setObjectName(QStringLiteral("xlsxStats"));
    m_statsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    barRow->addWidget(m_nameBox);
    barRow->addWidget(fxLabel);
    barRow->addWidget(m_formulaBox, 1);
    barRow->addWidget(m_statsLabel);
    layout->addLayout(barRow);

    // 每个工作表一页；标签放底部（表格应用惯例）；m_table 指向当前活动表
    m_sheets = new QTabWidget(this);
    m_sheets->setObjectName(QStringLiteral("xlsxSheets"));
    m_sheets->setDocumentMode(true);
    m_sheets->setTabPosition(QTabWidget::South);
    layout->addWidget(m_sheets, 1);

    connect(m_formulaBox, &QLineEdit::returnPressed,
            this, &XlsxViewer::formulaBoxReturned);

    if (m_isCsv) {
        m_table = makeSheetTable();
        m_sheets->addTab(m_table, QStringLiteral("CSV"));
        loadCsv();
    } else {
        loadXlsx();
    }
    if (m_sheets->count() > 0) {
        // 多表文件：save() 只会写单表，为避免覆盖丢其它表，自动转为只读
        const bool readOnly = m_sheets->count() > 1;
        for (int i = 0; i < m_sheets->count(); ++i) {
            auto* table = qobject_cast<QTableWidget*>(m_sheets->widget(i));
            if (!table)
                continue;
            table->setEditTriggers(readOnly
                                       ? QAbstractItemView::NoEditTriggers
                                       : QAbstractItemView::DoubleClicked
                                             | QAbstractItemView::EditKeyPressed);
            if (readOnly)
                table->setToolTip(tr("多工作表文件以只读方式打开（保存会丢失其它表）"));
        }
        m_table = qobject_cast<QTableWidget*>(m_sheets->widget(0));
        m_sheets->setCurrentIndex(0);
        connect(m_sheets, &QTabWidget::currentChanged, this, [this](int index) {
            if (auto* table = qobject_cast<QTableWidget*>(m_sheets->widget(index)))
                m_table = table; // 保存始终针对当前活动表
        });
    }
}


// ---- xlsx 呈现层（A 路线）：sharedStrings / 多表 / 合并 / 列宽行高 / 样式 / 数字格式 ----

namespace {

// 列字母（A、AA）→ 1 基列号
int xlsxColToIndex(const QString& letters)
{
    int col = 0;
    for (const QChar ch : letters)
        col = col * 26 + (ch.toUpper().toLatin1() - 'A' + 1);
    return col;
}

// 提取 XML 标签里某个属性的值
QString xlsxAttr(const QString& tag, const QString& name)
{
    const QRegularExpression re(QStringLiteral("%1=\"([^\"]*)\"").arg(name));
    const auto match = re.match(tag);
    return match.hasMatch() ? match.captured(1) : QString();
}

// 合并 <si> 内所有 <t> 的文本（兼容富文本 run）
QString parseSharedString(const QString& siXml)
{
    QString out;
    const QRegularExpression tRe(QStringLiteral("<t(?:\\s[^>]*)?>(.*?)</t>"),
                                 QRegularExpression::DotMatchesEverythingOption);
    auto it = tRe.globalMatch(siXml);
    while (it.hasNext())
        out += ooxml::unescapeXml(it.next().captured(1));
    if (out.isEmpty() && !siXml.contains(QLatin1String("<t")))
        out = ooxml::unescapeXml(siXml);
    return out;
}

// Excel 日期序列号 → "yyyy-MM-dd[ hh:mm]"（1900 系统，用 1899-12-30 纪元规避闰年 bug）
QString excelSerialToDateTime(double serial)
{
    const QDateTime epoch(QDate(1899, 12, 30), QTime(0, 0));
    const QDateTime dateTime = epoch.addSecs(qint64(serial * 86400.0 + 0.5));
    const bool hasTime = std::modf(serial, nullptr) > 1e-9;
    return dateTime.toString(hasTime ? QStringLiteral("yyyy-MM-dd hh:mm")
                                     : QStringLiteral("yyyy-MM-dd"));
}

// 数字格式渲染：numFmtId（内置）+ 自定义格式码
QString formatXlsxNumber(double value, int numFmtId, const QString& formatCode)
{
    QString code = formatCode;
    if (code.isEmpty()) {
        switch (numFmtId) {
        case 0: code = QStringLiteral("General"); break;
        case 1: code = QStringLiteral("0"); break;
        case 2: code = QStringLiteral("0.00"); break;
        case 3: code = QStringLiteral("#,##0"); break;
        case 4: code = QStringLiteral("#,##0.00"); break;
        case 9: code = QStringLiteral("0%"); break;
        case 10: code = QStringLiteral("0.00%"); break;
        case 11: code = QStringLiteral("0.00E+00"); break;
        case 14: case 15: case 16: case 17:
            code = QStringLiteral("yyyy-MM-dd"); break;
        case 22: code = QStringLiteral("yyyy-MM-dd hh:mm"); break;
        case 49: return QString::number(value, 'f', -1);
        default: code = QStringLiteral("General"); break;
        }
    }

    // 日期/时间（含 d/y 或 h+s 视为日期时间）
    const bool looksDate = code.contains(QLatin1Char('y')) || code.contains(QLatin1Char('Y'))
                           || (code.contains(QLatin1Char('d'), Qt::CaseInsensitive)
                               && !code.contains(QLatin1Char('"')))
                           || (code.contains(QLatin1Char('h'), Qt::CaseInsensitive)
                               && code.contains(QLatin1Char('s'), Qt::CaseInsensitive));
    if (looksDate && numFmtId != 49)
        return excelSerialToDateTime(value);

    if (code.contains(QLatin1Char('%'))) {
        const int decimals = qMax(0, int(code.lastIndexOf(QLatin1Char('%'))
                                        - code.lastIndexOf(QLatin1Char('.')) - 1));
        return QString::number(value * 100.0, 'f', decimals) + QLatin1Char('%');
    }
    if (code == QLatin1String("General") || code.isEmpty())
        return QString::number(value, 'g', 10);

    // 0.00 / #,##0 类
    const int dotAt = int(code.lastIndexOf(QLatin1Char('.')));
    int decimals = 0;
    if (dotAt >= 0) {
        for (int i = dotAt + 1; i < code.size(); ++i) {
            if (code.at(i) == QLatin1Char('0') || code.at(i) == QLatin1Char('#'))
                ++decimals;
        }
    }
    QString text = QString::number(value, 'f', decimals);
    if (code.contains(QLatin1Char(',')) && dotAt != 0) {
        // 千分位（简单实现：整数部分每 3 位加逗号）
        const int signAt = text.startsWith(QLatin1Char('-')) ? 1 : 0;
        const int intEnd = dotAt >= 0 ? text.indexOf(QLatin1Char('.')) : text.size();
        for (int i = intEnd - 3; i > signAt; i -= 3)
            text.insert(i, QLatin1Char(','));
    }
    return text;
}

struct XlsxCellStyle {
    bool bold = false;
    double fontSize = 0;      // 0 = 用默认
    QColor color;             // 字体颜色（无效 = 默认）
    QColor background;        // 纯色填充（无效 = 无）
    int numFmtId = 0;
    QString numFmtCode;       // 自定义格式码（numFmtId >= 164 时）
    Qt::Alignment alignment;
    bool wrap = false;
};

} // namespace

// 多表结构与单元格（测试可达）
int XlsxViewer::sheetCount() const
{
    return m_sheets ? m_sheets->count() : 0;
}

QString XlsxViewer::sheetName(int index) const
{
    return m_sheets && index >= 0 && index < m_sheets->count()
               ? m_sheets->tabText(index)
               : QString();
}

QTableWidgetItem* XlsxViewer::cellAt(int sheet, int row, int column) const
{
    if (!m_sheets || sheet < 0 || sheet >= m_sheets->count())
        return nullptr;
    auto* table = qobject_cast<QTableWidget*>(m_sheets->widget(sheet));
    return table ? table->item(row, column) : nullptr;
}

QTableWidget* XlsxViewer::makeSheetTable()
{
    auto* table = new QTableWidget(this);
    table->setObjectName(QStringLiteral("xlsxTable"));
    table->verticalHeader()->setDefaultSectionSize(22);
    table->setAlternatingRowColors(false);
    connect(table, &QTableWidget::cellChanged,
            this, &XlsxViewer::cellChanged);
    connect(table, &QTableWidget::currentCellChanged,
            this, &XlsxViewer::currentCellChanged);
    connect(table, &QTableWidget::itemSelectionChanged,
            this, &XlsxViewer::refreshSelectionStats);
    return table;
}

// (0,0) -> "A1"；供名称框使用
QString XlsxViewer::cellReference(int row, int column) const
{
    QString letters;
    int v = column + 1;
    while (v > 0) {
        letters.prepend(QChar(char('A') + (v - 1) % 26));
        v = (v - 1) / 26;
    }
    return letters + QString::number(row + 1);
}

void XlsxViewer::currentCellChanged(int row, int column, int previousRow,
                                    int previousColumn)
{
    Q_UNUSED(previousRow);
    Q_UNUSED(previousColumn);
    auto* table = qobject_cast<QTableWidget*>(sender());
    if (!table || table != m_table)
        return;
    updateFormulaBar(table, row, column);
}

// 名称框/编辑栏刷新：公式单元格显示 =公式；有范围选择时名称框显示 A2:A3
void XlsxViewer::updateFormulaBar(QTableWidget* table, int row, int column)
{
    // 名称框：多选时显示选中项的包围盒（A2:C3），单选显示单元格引用
    const auto selected = table->selectionModel()->selectedIndexes();
    if (selected.size() > 1) {
        int top = selected.first().row();
        int bottom = top;
        int left = selected.first().column();
        int right = left;
        for (const QModelIndex& index : selected) {
            top = qMin(top, index.row());
            bottom = qMax(bottom, index.row());
            left = qMin(left, index.column());
            right = qMax(right, index.column());
        }
        m_nameBox->setText(cellReference(top, left) + QLatin1Char(':')
                           + cellReference(bottom, right));
    } else {
        m_nameBox->setText(cellReference(row, column));
    }
    const QTableWidgetItem* item = table->item(row, column);
    if (!item) {
        m_formulaBox->setText(QString());
        return;
    }
    // 公式单元格：编辑栏显示 =公式（单元格本体仍是缓存计算值）
    const QString formula = item->data(Qt::UserRole).toString();
    m_formulaBox->setText(formula.isEmpty() ? item->text() : formula);
}

// 选区统计（WPS 底部那条的简化版）：对选中区域的数字求和/平均/计数
void XlsxViewer::refreshSelectionStatsImpl()
{
    auto* table = m_table;
    if (!table || !m_statsLabel)
        return;
    // 选区变化同时刷新名称框/编辑栏（名称框显示 A2:A3 这类范围引用）
    updateFormulaBar(table, table->currentRow(), table->currentColumn());
    const QList<QTableWidgetItem*> selected = table->selectedItems();
    double sum = 0.0;
    int numbers = 0;
    for (const QTableWidgetItem* item : selected) {
        if (!item)
            continue;
        bool ok = false;
        const double value = item->text().toDouble(&ok);
        if (ok) {
            sum += value;
            ++numbers;
        }
    }
    if (numbers == 0) {
        m_statsLabel->clear();
        return;
    }
    m_statsLabel->setText(tr("求和 %1 · 平均 %2 · 计数 %3")
                              .arg(QString::number(sum, 'g', 10))
                              .arg(QString::number(sum / numbers, 'g', 10))
                              .arg(numbers));
}

// 编辑栏回车：写入当前单元格（触发既有的修改标记/保存逻辑）
void XlsxViewer::formulaBoxReturned()
{
    if (!m_table || m_loading)
        return;
    const int row = m_table->currentRow();
    const int column = m_table->currentColumn();
    if (row < 0 || column < 0)
        return;
    const QString text = m_formulaBox->text();
    if (auto* item = m_table->item(row, column)) {
        if (item->text() == text)
            return;
        item->setText(text);
    } else {
        m_table->setItem(row, column, new QTableWidgetItem(text));
    }
    m_table->setCurrentCell(row, column);
    m_table->setFocus();
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

    // ---- 共享字符串表：t="s" 的单元格索引到这里取真实文本 ----
    QStringList sharedStrings;
    const QByteArray sst = zip.entry(QStringLiteral("xl/sharedStrings.xml"));
    if (!sst.isEmpty()) {
        const QString xml = QString::fromUtf8(sst);
        const QRegularExpression siRe(QStringLiteral("<si(?:\\s[^>]*)?>(.*?)</si>"),
                                       QRegularExpression::DotMatchesEverythingOption);
        auto it = siRe.globalMatch(xml);
        while (it.hasNext())
            sharedStrings.append(parseSharedString(it.next().captured(1)));
    }

    // ---- 样式表：字体 / 填充 / 对齐 / 数字格式 ----
    QVector<XlsxCellStyle> styles;
    QHash<int, QString> customFormats;   // numFmtId -> 格式码
    const QByteArray stylesXml = zip.entry(QStringLiteral("xl/styles.xml"));
    if (!stylesXml.isEmpty()) {
        const QString xml = QString::fromUtf8(stylesXml);

        const QRegularExpression numFmtRe(
            QStringLiteral("<numFmt\\s[^>]*numFmtId=\"(\\d+)\"[^>]*formatCode=\"([^\"]*)\""),
            QRegularExpression::DotMatchesEverythingOption);
        auto it = numFmtRe.globalMatch(xml);
        while (it.hasNext()) {
            const auto match = it.next();
            customFormats.insert(match.captured(1).toInt(), match.captured(2));
        }

        QVector<bool> bolds;
        QVector<double> sizes;
        QVector<QColor> colors;
        const QRegularExpression fontRe(QStringLiteral("<font>(.*?)</font>"),
                                        QRegularExpression::DotMatchesEverythingOption);
        auto fontIt = fontRe.globalMatch(xml);
        while (fontIt.hasNext()) {
            const QString fontXml = fontIt.next().captured(1);
            bolds.append(fontXml.contains(QLatin1String("<b/>")));
            const auto sz = QRegularExpression(QStringLiteral("<sz val=\"([\\d.]+)\"/>"))
                                .match(fontXml);
            sizes.append(sz.hasMatch() ? sz.captured(1).toDouble() : 0.0);
            const auto color = QRegularExpression(
                                   QStringLiteral("<color rgb=\"([0-9A-Fa-f]{8})\"/>"))
                                   .match(fontXml);
            colors.append(color.hasMatch()
                              ? QColor(QLatin1Char('#')
                                       + color.captured(1).right(6))
                              : QColor());
        }

        QVector<QColor> fills;
        const QRegularExpression fillRe(
            QStringLiteral("<fill>(.*?)</fill>"),
            QRegularExpression::DotMatchesEverythingOption);
        auto fillIt = fillRe.globalMatch(xml);
        while (fillIt.hasNext()) {
            const QString fillXml = fillIt.next().captured(1);
            QColor color;
            if (fillXml.contains(QLatin1String("patternType=\"solid\""))) {
                const auto fg = QRegularExpression(
                                    QStringLiteral("<fgColor rgb=\"([0-9A-Fa-f]{8})\"/>"))
                                    .match(fillXml);
                if (fg.hasMatch())
                    color = QColor(QLatin1Char('#') + fg.captured(1).right(6));
            }
            fills.append(color);
        }

        // 注意 <cellXfs> 可能带属性（<cellXfs count="4">），按前缀查找
        const int cellXfsAt = xml.indexOf(QStringLiteral("<cellXfs"));
        if (cellXfsAt >= 0) {
            const int cellXfsEnd = xml.indexOf(QStringLiteral("</cellXfs>"), cellXfsAt);
            const QString block = xml.mid(cellXfsAt, cellXfsEnd - cellXfsAt);
            const QRegularExpression xfRe(QStringLiteral("<xf\\s([^>]*?)/?>"),
                                           QRegularExpression::DotMatchesEverythingOption);
            auto xit = xfRe.globalMatch(block);
            while (xit.hasNext()) {
                const QString attrs = xit.next().captured(1);
                XlsxCellStyle style;
                const int fontId = xlsxAttr(attrs, QStringLiteral("fontId")).toInt();
                const int fillId = xlsxAttr(attrs, QStringLiteral("fillId")).toInt();
                style.numFmtId = xlsxAttr(attrs, QStringLiteral("numFmtId")).toInt();
                if (fontId >= 0 && fontId < bolds.size()) {
                    style.bold = bolds.at(fontId);
                    style.fontSize = sizes.at(fontId);
                    style.color = colors.at(fontId);
                }
                if (fillId >= 0 && fillId < fills.size())
                    style.background = fills.at(fillId);
                if (customFormats.contains(style.numFmtId))
                    style.numFmtCode = customFormats.value(style.numFmtId);
                const auto alignment = QRegularExpression(
                                           QStringLiteral("<alignment\\s([^>]*)/>"))
                                           .match(attrs);
                if (alignment.hasMatch()) {
                    const QString a = alignment.captured(1);
                    const QString h = xlsxAttr(a, QStringLiteral("horizontal"));
                    if (h == QLatin1String("center"))
                        style.alignment |= Qt::AlignHCenter;
                    else if (h == QLatin1String("right"))
                        style.alignment |= Qt::AlignRight;
                    else if (h == QLatin1String("left"))
                        style.alignment |= Qt::AlignLeft;
                    const QString v = xlsxAttr(a, QStringLiteral("vertical"));
                    if (v == QLatin1String("center"))
                        style.alignment |= Qt::AlignVCenter;
                    style.wrap = a.contains(QLatin1String("wrapText=\"1\""));
                }
                styles.append(style);
            }
        }
    }

    // ---- 工作簿：表名 + rId ----
    struct SheetRef { QString name; QString rid; };
    QList<SheetRef> sheetRefs;
    const QByteArray wb = zip.entry(QStringLiteral("xl/workbook.xml"));
    if (wb.isEmpty())
        return false;
    {
        const QString xml = QString::fromUtf8(wb);
        const QRegularExpression sheetRe(
            QStringLiteral("<sheet\\s[^>]*name=\"([^\"]*)\"[^>]*r:id=\"([^\"]+)\""));
        auto it = sheetRe.globalMatch(xml);
        while (it.hasNext()) {
            const auto m = it.next();
            sheetRefs.append({ ooxml::unescapeXml(m.captured(1)), m.captured(2) });
        }
    }

    // ---- 关系：rId -> worksheets/sheetN.xml ----
    QHash<QString, QString> relTargets;
    const QByteArray rels = zip.entry(QStringLiteral("xl/_rels/workbook.xml.rels"));
    if (!rels.isEmpty()) {
        const QString xml = QString::fromUtf8(rels);
        const QRegularExpression relRe(
            QStringLiteral("<Relationship\\s[^>]*Id=\"([^\"]+)\"[^>]*Target=\"([^\"]+)\""));
        auto it = relRe.globalMatch(xml);
        while (it.hasNext()) {
            const auto m = it.next();
            relTargets.insert(m.captured(1), m.captured(2));
        }
    }

    m_loading = true;
    m_sheets->clear();

    for (const SheetRef& ref : sheetRefs) {
        QString target = relTargets.value(ref.rid);
        if (target.isEmpty())
            continue;
        target.remove(QLatin1String("/xl/"));
        const QByteArray sheetXml = zip.entry(QStringLiteral("xl/") + target);
        if (sheetXml.isEmpty())
            continue;
        const QString xml = QString::fromUtf8(sheetXml);

        auto* table = makeSheetTable();

        // 行 / 单元格（r="A1"、t 类型、s 样式）
        struct Cell { int row; int col; QString text; int styleId; QString formula; };
        QList<Cell> cells;
        QList<QPair<QPair<int, int>, QPair<int, int>>> merges; // (r1,c1)-(r2,c2)
        QHash<int, qreal> rowHeights;
        QHash<int, qreal> colWidths;
        int maxRow = 0;
        int maxCol = 0;

        const QRegularExpression rowRe(QStringLiteral("<row\\s([^>]*)>(.*?)</row>"),
                                        QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpression cellRe(
            QStringLiteral("<c((?:\\s[^>]*?)?)(?:/>|>(.*?)</c>)"),
            QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpression valueRe(QStringLiteral("<v(?:\\s[^>]*)?>([^<]*)</v>"));
        const QRegularExpression mergeRe(
            QStringLiteral("<mergeCell ref=\"([A-Z]+)(\\d+):([A-Z]+)(\\d+)\"/>"));

        auto rowIt = rowRe.globalMatch(xml);
        while (rowIt.hasNext()) {
            const auto rowMatch = rowIt.next();
            const QString rowAttrs = rowMatch.captured(1);
            const QString rowXml = rowMatch.captured(2);
            const int rowNumber = xlsxAttr(rowAttrs, QStringLiteral("r")).toInt();
            const auto ht = QRegularExpression(QStringLiteral("ht=\"([\\d.]+)\""))
                                .match(rowAttrs);
            if (ht.hasMatch())
                rowHeights.insert(rowNumber - 1, ht.captured(1).toDouble());

            auto cellIt = cellRe.globalMatch(rowXml);
            while (cellIt.hasNext()) {
                const auto cell = cellIt.next();
                const QString attrs = cell.captured(1);
                const QString inner = cell.captured(2);
                const auto ref = QRegularExpression(QStringLiteral("r=\"([A-Z]+)(\\d+)\""))
                                     .match(attrs);
                if (!ref.hasMatch())
                    continue;
                const int col = xlsxColToIndex(ref.captured(1));
                const int row = ref.captured(2).toInt();

                const QString type = xlsxAttr(attrs, QStringLiteral("t"));
                const int styleId = xlsxAttr(attrs, QStringLiteral("s")).toInt();

                // 公式（<f>…</f>）：存进 UserRole，编辑栏显示 =公式
                QString formula;
                const auto formulaMatch
                    = QRegularExpression(QStringLiteral("<f(?:\s[^>]*)?>(.*?)</f>"),
                                          QRegularExpression::DotMatchesEverythingOption)
                          .match(inner);
                if (formulaMatch.hasMatch())
                    formula = QLatin1Char('=')
                              + ooxml::unescapeXml(formulaMatch.captured(1));

                QString text;
                if (type == QLatin1String("s")) {
                    bool ok = false;
                    const int index = valueRe.match(inner).captured(1).toInt(&ok);
                    text = ok && index >= 0 && index < sharedStrings.size()
                               ? sharedStrings.at(index)
                               : QString();
                } else if (type == QLatin1String("inlineStr")) {
                    text = parseSharedString(inner);
                } else if (type == QLatin1String("str") || type == QLatin1String("e")) {
                    text = ooxml::unescapeXml(valueRe.match(inner).captured(1));
                } else if (type == QLatin1String("b")) {
                    text = valueRe.match(inner).captured(1) == QLatin1String("1")
                               ? QStringLiteral("TRUE")
                               : QStringLiteral("FALSE");
                } else {
                    // 数字：按样式里的数字格式渲染
                    const QString raw = valueRe.match(inner).captured(1);
                    if (!raw.isEmpty()) {
                        bool ok = false;
                        const double number = raw.toDouble(&ok);
                        if (ok && styleId >= 0 && styleId < styles.size()) {
                            const XlsxCellStyle& style = styles.at(styleId);
                            text = formatXlsxNumber(number, style.numFmtId,
                                                    style.numFmtCode);
                        } else {
                            text = QString::number(number, 'g', 10);
                        }
                    }
                }

                cells.append({ row, col, text, styleId, formula });
                maxRow = qMax(maxRow, row);
                maxCol = qMax(maxCol, col);
            }
        }

        // 合并区域
        auto mergeIt = mergeRe.globalMatch(xml);
        while (mergeIt.hasNext()) {
            const auto m = mergeIt.next();
            const int r1 = m.captured(2).toInt();
            const int c1 = xlsxColToIndex(m.captured(1));
            const int r2 = m.captured(4).toInt();
            const int c2 = xlsxColToIndex(m.captured(3));
            merges.append({ { r1 - 1, c1 - 1 }, { r2 - 1, c2 - 1 } });
            maxRow = qMax(maxRow, r2);
            maxCol = qMax(maxCol, c2);
        }

        // 列宽（cols：min..max，width 单位≈字符）
        const QRegularExpression colRe(QStringLiteral("<col\\s([^>]*)/>"));
        auto colIt = colRe.globalMatch(xml);
        while (colIt.hasNext()) {
            const QString attrs = colIt.next().captured(1);
            if (!attrs.contains(QLatin1String("customWidth")))
                continue;
            const int min = xlsxAttr(attrs, QStringLiteral("min")).toInt();
            const int max = qMax(min, xlsxAttr(attrs, QStringLiteral("max")).toInt());
            const qreal width = xlsxAttr(attrs, QStringLiteral("width")).toDouble();
            for (int c = min; c <= max; ++c)
                colWidths.insert(c - 1, width);
        }

        table->setRowCount(qMax(1, maxRow));
        table->setColumnCount(qMax(1, maxCol));

        for (const Cell& cell : cells) {
            auto* item = new QTableWidgetItem(cell.text);
            if (!cell.formula.isEmpty())
                item->setData(Qt::UserRole, cell.formula);
            if (cell.styleId >= 0 && cell.styleId < styles.size()) {
                const XlsxCellStyle& style = styles.at(cell.styleId);
                QFont font = item->font();
                font.setBold(style.bold);
                if (style.fontSize > 0)
                    font.setPointSizeF(style.fontSize);
                item->setFont(font);
                if (style.color.isValid())
                    item->setForeground(style.color);
                if (style.background.isValid())
                    item->setBackground(style.background);
                if (style.alignment != Qt::Alignment())
                    item->setTextAlignment(style.alignment);
            }
            table->setItem(cell.row - 1, cell.col - 1, item);
        }

        for (const auto& merge : merges) {
            const int r1 = merge.first.first;
            const int c1 = merge.first.second;
            const int r2 = merge.second.first;
            const int c2 = merge.second.second;
            table->setSpan(r1, c1, r2 - r1 + 1, c2 - c1 + 1);
            // 合并区里非左上角的空单元格移除，避免编辑出怪值
            for (int r = r1; r <= r2; ++r)
                for (int c = c1; c <= c2; ++c)
                    if ((r != r1 || c != c1) && !table->item(r, c))
                        table->setItem(r, c, new QTableWidgetItem(QString()));
        }

        for (auto it = colWidths.constBegin(); it != colWidths.constEnd(); ++it) {
            if (it.key() < table->columnCount())
                table->setColumnWidth(it.key(), qRound(it.value() * 7.0) + 5);
        }
        for (auto it = rowHeights.constBegin(); it != rowHeights.constEnd(); ++it) {
            if (it.key() < table->rowCount())
                table->setRowHeight(it.key(), qRound(it.value() * 96.0 / 72.0));
        }

        // 大片可滚动空白区（表格应用的惯例：用到的范围之外再留一大截）
        if (table->columnCount() < 30)
            table->setColumnCount(30);
        else
            table->setColumnCount(table->columnCount() + 8);
        if (table->rowCount() < 100)
            table->setRowCount(100);
        else
            table->setRowCount(table->rowCount() + 40);
        // 列标题 A..Z、AA…
        QStringList headerLabels;
        for (int c = 0; c < table->columnCount(); ++c) {
            QString letters;
            int v = c + 1;
            while (v > 0) {
                letters.prepend(QChar(char('A') + (v - 1) % 26));
                v = (v - 1) / 26;
            }
            headerLabels.append(letters);
        }
        table->setHorizontalHeaderLabels(headerLabels);

        m_sheets->addTab(table, ref.name);
        m_sheetNames.append(ref.name);
    }

    if (m_sheets->count() == 0)
        return false;
    m_table = qobject_cast<QTableWidget*>(m_sheets->widget(0));
    m_sheetIndex = 0;
    m_loading = false;
    if (m_statsLabel)
        m_statsLabel->clear();
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
    // 与 xlsx 相同的外壳：扩展网格 + 列标题
    if (m_table->columnCount() < 30)
        m_table->setColumnCount(30);
    if (m_table->rowCount() < 100)
        m_table->setRowCount(100);
    QStringList headerLabels;
    for (int c = 0; c < m_table->columnCount(); ++c) {
        QString letters;
        int v = c + 1;
        while (v > 0) {
            letters.prepend(QChar(char('A') + (v - 1) % 26));
            v = (v - 1) / 26;
        }
        headerLabels.append(letters);
    }
    m_table->setHorizontalHeaderLabels(headerLabels);
    m_nameBox->setText(QStringLiteral("A1"));
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


namespace {

// 属性提取（不用正则，避免转义问题）：name="value"
QString pptAttr(const QString& tag, const QString& name)
{
    const QString pattern = name + QStringLiteral("=") + QLatin1Char('"');
    const int at = tag.indexOf(pattern);
    if (at < 0)
        return QString();
    const int valueStart = at + pattern.size();
    const int valueEnd = tag.indexOf(QLatin1Char('"'), valueStart);
    return valueEnd < 0 ? QString() : tag.mid(valueStart, valueEnd - valueStart);
}

// EMU → pt（1pt = 12700 EMU）
double emuToPt(double emu)
{
    return emu / 12700.0;
}

// <a:solidFill><a:srgbClr val="RRGGBB"/> 的颜色；没有则返回无效色
QColor pptSolidFill(const QString& xml)
{
    const int fillAt = xml.indexOf(QStringLiteral("solidFill"));
    if (fillAt < 0)
        return QColor();
    const int clrAt = xml.indexOf(QStringLiteral("srgbClr val="), fillAt);
    if (clrAt < 0)
        return QColor();
    const int valueStart = clrAt + int(qstrlen("srgbClr val=")) + 1;
    const QString hex = xml.mid(valueStart, 6);
    return QColor(QLatin1Char('#') + hex);
}

// 形状范围：<a:off x y/><a:ext cx cy/>
QRectF pptShapeRect(const QString& xml)
{
    const int offAt = xml.indexOf(QStringLiteral("<a:off "));
    const int extAt = xml.indexOf(QStringLiteral("<a:ext "));
    if (offAt < 0 || extAt < 0)
        return QRectF();
    const QString offTag = xml.mid(offAt, xml.indexOf(QLatin1Char('>'), offAt) - offAt);
    const QString extTag = xml.mid(extAt, xml.indexOf(QLatin1Char('>'), extAt) - extAt);
    return QRectF(emuToPt(pptAttr(offTag, QStringLiteral("x")).toDouble()),
                  emuToPt(pptAttr(offTag, QStringLiteral("y")).toDouble()),
                  emuToPt(pptAttr(extTag, QStringLiteral("cx")).toDouble()),
                  emuToPt(pptAttr(extTag, QStringLiteral("cy")).toDouble()));
}

// 一个 <a:p> 段落里的文本 run（字号/加粗/颜色）
QVector<PptxRun> pptParagraphRuns(const QString& paraXml, double defaultSizePt)
{
    QVector<PptxRun> runs;
    int pos = 0;
    while (true) {
        const int rAt = paraXml.indexOf(QStringLiteral("<a:r>"), pos);
        const int rWithAttrs = paraXml.indexOf(QStringLiteral("<a:r "), pos);
        int startAt = rAt;
        if (startAt < 0 || (rWithAttrs >= 0 && rWithAttrs < startAt))
            startAt = rWithAttrs;
        if (startAt < 0)
            break;
        const int rEnd = paraXml.indexOf(QStringLiteral("</a:r>"), startAt);
        if (rEnd < 0)
            break;
        const QString runXml = paraXml.mid(startAt, rEnd - startAt);

        const int tAt = runXml.indexOf(QStringLiteral("<a:t>"));
        const int tEnd = runXml.indexOf(QStringLiteral("</a:t>"), tAt);
        if (tAt >= 0 && tEnd > tAt) {
            PptxRun run;
            run.text = ooxml::unescapeXml(
                runXml.mid(tAt + int(qstrlen("<a:t>")), tEnd - tAt - int(qstrlen("<a:t>"))));
            const int rPrAt = runXml.indexOf(QStringLiteral("<a:rPr"));
            if (rPrAt >= 0) {
                const QString rPr = runXml.mid(rPrAt, runXml.indexOf(QLatin1Char('>'), rPrAt)
                                                          - rPrAt);
                const QString sz = pptAttr(rPr, QStringLiteral("sz"));
                if (!sz.isEmpty())
                    run.sizePt = sz.toDouble() / 100.0; // sz 单位是 1/100 pt
                const QString bold = pptAttr(rPr, QStringLiteral("b"));
                run.bold = bold == QLatin1String("1") || bold == QLatin1String("true");
                run.color = pptSolidFill(runXml.mid(rPrAt, rEnd - rPrAt));
            }
            const int endRPr = runXml.indexOf(QStringLiteral("</a:rPr>"));
            if (!run.color.isValid() && endRPr >= 0)
                run.color = pptSolidFill(runXml.mid(endRPr));
            if (!run.color.isValid() && rPrAt < 0)
                run.color = pptSolidFill(runXml);
            if (run.sizePt <= 0)
                run.sizePt = defaultSizePt;
            runs.append(run);
        }
        pos = rEnd + int(qstrlen("</a:r>"));
    }
    return runs;
}

// 形状的文本段落
QVector<QVector<PptxRun>> pptShapeParagraphs(const QString& xml, double defaultSizePt)
{
    QVector<QVector<PptxRun>> paragraphs;
    int pos = 0;
    while (true) {
        const int pAt = xml.indexOf(QStringLiteral("<a:p>"), pos);
        const int pWithAttrs = xml.indexOf(QStringLiteral("<a:p "), pos);
        int startAt = pAt;
        if (startAt < 0 || (pWithAttrs >= 0 && pWithAttrs < startAt))
            startAt = pWithAttrs;
        if (startAt < 0)
            break;
        const int pEnd = xml.indexOf(QStringLiteral("</a:p>"), startAt);
        if (pEnd < 0)
            break;
        const QVector<PptxRun> runs
            = pptParagraphRuns(xml.mid(startAt, pEnd - startAt), defaultSizePt);
        if (!runs.isEmpty())
            paragraphs.append(runs);
        pos = pEnd + int(qstrlen("</a:p>"));
    }
    return paragraphs;
}

// 幻灯片画布：白底 + 形状填充/边框 + 图片 + 文本 run（等比缩放、居中留白）
class PptxSlideCanvas : public QWidget {
public:
    PptxSlideCanvas(const PptxSlide* slide, const double* zoom, QWidget* parent)
        : QWidget(parent)
        , m_slide(slide)
        , m_zoom(zoom)
    {
        setMinimumSize(320, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().window());
        if (!m_slide)
            return;
        const QSizeF slideSize = m_slide->sizePt;
        const double fit = qMin(width() / slideSize.width(),
                                height() / slideSize.height());
        const double scale = fit * (*m_zoom);
        const double w = slideSize.width() * scale;
        const double h = slideSize.height() * scale;
        const double ox = (width() - w) / 2.0;
        const double oy = (height() - h) / 2.0;

        // 白纸 + 细边框（幻灯片边界）
        const QRectF pageRect(ox, oy, w, h);
        painter.fillRect(pageRect, Qt::white);
        painter.setPen(QPen(QColor(0xb0, 0xb8, 0xb4), 1));
        painter.drawRect(pageRect);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        for (const PptxShape& shape : m_slide->shapes) {
            const QRectF target(ox + shape.rect.x() * scale, oy + shape.rect.y() * scale,
                                shape.rect.width() * scale,
                                shape.rect.height() * scale);
            if (shape.image.isNull() == false) {
                painter.drawImage(target, shape.image);
                continue;
            }
            if (shape.fill.isValid())
                painter.fillRect(target, shape.fill);
            if (shape.line.isValid())
                painter.setPen(shape.line);
            else
                painter.setPen(Qt::NoPen);
            if (shape.line.isValid())
                painter.drawRect(target);

            // 文本：段落逐段、run 逐段顺排（超出宽度换行）
            double y = target.top() + 4.0;
            for (const QVector<PptxRun>& paragraph : shape.paragraphs) {
                double x = target.left() + 4.0;
                double lineHeight = 0;
                for (const PptxRun& run : paragraph) {
                    QFont font = painter.font();
                    font.setPointSizeF(qMax(6.0, run.sizePt * scale));
                    font.setBold(run.bold);
                    painter.setFont(font);
                    painter.setPen(run.color.isValid() ? run.color : QColor(0x24, 0x29, 0x2e));
                    const QFontMetricsF metrics(font);
                    lineHeight = qMax(lineHeight, metrics.height());
                    QString word;
                    for (const QChar ch : run.text) {
                        const bool breakable = ch.isSpace() || ch.unicode() > 0x2000;
                        const double advance
                            = metrics.horizontalAdvance(word + ch);
                        if (breakable && x + advance > target.right() - 4.0
                            && !word.trimmed().isEmpty()) {
                            painter.drawText(QPointF(x, y + metrics.ascent()), word);
                            x = target.left() + 4.0;
                            y += lineHeight;
                            word.clear();
                            if (ch.isSpace())
                                continue;
                        }
                        word.append(ch);
                    }
                    if (!word.isEmpty()) {
                        painter.drawText(QPointF(x, y + metrics.ascent()), word);
                        x += metrics.horizontalAdvance(word);
                    }
                }
                if (lineHeight <= 0)
                    lineHeight = painter.fontMetrics().height();
                y += lineHeight + 2.0;
            }
        }
    }

private:
    const PptxSlide* m_slide = nullptr;
    const double* m_zoom = nullptr;
};

// 解析一张幻灯片
PptxSlide parsePptxSlide(const QString& xml, const QHash<QString, QString>& rels,
                         const QHash<QString, QImage>& media, const QSizeF& slideSize)
{
    PptxSlide slide;
    slide.sizePt = slideSize;

    int autoPlaced = 0; // 无几何信息的形状按顺序排布
    int pos = 0;
    while (true) {
        const int spAt = xml.indexOf(QStringLiteral("<p:sp>"), pos);
        const int picAt = xml.indexOf(QStringLiteral("<p:pic>"), pos);
        if (spAt < 0 && picAt < 0)
            break;
        const bool isPicture = picAt >= 0 && (spAt < 0 || picAt < spAt);
        const QString openTag = isPicture ? QStringLiteral("<p:pic>")
                                          : QStringLiteral("<p:sp>");
        const QString closeTag = isPicture ? QStringLiteral("</p:pic>")
                                           : QStringLiteral("</p:sp>");
        const int startAt = isPicture ? picAt : spAt;
        const int endAt = xml.indexOf(closeTag, startAt);
        if (endAt < 0)
            break;
        const QString shapeXml = xml.mid(startAt, endAt - startAt);

        PptxShape shape;
        shape.picture = isPicture;
        shape.rect = pptShapeRect(shapeXml);
        shape.fill = pptSolidFill(shapeXml.mid(0, shapeXml.indexOf(
                                                  QStringLiteral("<p:txBody>")) >= 0
                                                  ? shapeXml.indexOf(QStringLiteral("<p:txBody>"))
                                                  : shapeXml.size()));
        shape.line = QColor();
        if (isPicture) {
            const QString embed = pptAttr(shapeXml, QStringLiteral("r:embed"));
            const QString target = rels.value(embed);
            if (!target.isEmpty() && media.contains(target))
                shape.image = media.value(target);
        } else {
            shape.paragraphs = pptShapeParagraphs(shapeXml, 18.0);
        }
        // 占位符常常不写 xfrm（位置继承自幻灯片版式）。没有几何信息时给一个
        // 合理的默认位置，宁可位置近似也不要把内容整块丢掉。
        if (!shape.rect.isValid() && !shape.picture && !shape.paragraphs.isEmpty()) {
            const double slideW = slideSize.width();
            const double slideH = slideSize.height();
            if (autoPlaced == 0)
                shape.rect = QRectF(slideW * 0.08, slideH * 0.08, slideW * 0.84,
                                    slideH * 0.18);
            else
                shape.rect = QRectF(slideW * 0.08,
                                    slideH * (0.30 + 0.14 * (autoPlaced - 1)),
                                    slideW * 0.84, slideH * 0.12);
            ++autoPlaced;
        }
        if (shape.rect.isValid() && (shape.rect.width() > 1 || shape.rect.height() > 1))
            slide.shapes.append(shape);
        pos = endAt + closeTag.size();
    }
    return slide;
}

} // namespace

PptxViewer::PptxViewer(const QString& filePath, QWidget* parent)
    : DocumentViewer(parent)
    , m_filePath(filePath)
{
    setObjectName(QStringLiteral("pptxViewer"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 工具栏：上一页 / 页码 / 下一页 / 缩放
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("pptxToolbar"));
    auto* barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(8, 6, 8, 6);
    barLayout->setSpacing(6);
    m_prevButton = new QPushButton(QStringLiteral("◀"), bar);
    m_nextButton = new QPushButton(QStringLiteral("▶"), bar);
    m_pageLabel = new QLabel(bar);
    m_pageLabel->setObjectName(QStringLiteral("pptxPageLabel"));
    auto* zoomInButton = new QPushButton(QStringLiteral("放大"), bar);
    auto* zoomOutButton = new QPushButton(QStringLiteral("缩小"), bar);
    auto* zoomResetButton = new QPushButton(QStringLiteral("适应窗口"), bar);
    barLayout->addWidget(m_prevButton);
    barLayout->addWidget(m_nextButton);
    barLayout->addWidget(m_pageLabel);
    barLayout->addStretch(1);
    barLayout->addWidget(zoomOutButton);
    barLayout->addWidget(zoomInButton);
    barLayout->addWidget(zoomResetButton);
    layout->addWidget(bar);
    connect(m_prevButton, &QPushButton::clicked, this, &PptxViewer::previousSlide);
    connect(m_nextButton, &QPushButton::clicked, this, &PptxViewer::nextSlide);
    connect(zoomInButton, &QPushButton::clicked, this, &PptxViewer::zoomIn);
    connect(zoomOutButton, &QPushButton::clicked, this, &PptxViewer::zoomOut);
    connect(zoomResetButton, &QPushButton::clicked, this, &PptxViewer::resetZoom);

    auto* body = new QWidget(this);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    m_slideList = new QListWidget(body);
    m_slideList->setObjectName(QStringLiteral("pptxSlideList"));
    m_slideList->setFixedWidth(150);
    bodyLayout->addWidget(m_slideList);
    m_slides = new QStackedWidget(body);
    m_slides->setObjectName(QStringLiteral("pptxSlides"));
    bodyLayout->addWidget(m_slides, 1);
    layout->addWidget(body, 1);

    connect(m_slideList, &QListWidget::currentRowChanged,
            m_slides, &QStackedWidget::setCurrentIndex);
    connect(m_slideList, &QListWidget::currentRowChanged,
            this, &PptxViewer::showSlide);

    loadPptx();
}

void PptxViewer::showSlide(int index)
{
    if (!m_pageLabel)
        return;
    m_pageLabel->setText(tr("第 %1 / %2 页").arg(index + 1).arg(m_slideData.size()));
    if (m_prevButton)
        m_prevButton->setEnabled(index > 0);
    if (m_nextButton)
        m_nextButton->setEnabled(index + 1 < m_slideData.size());
}

void PptxViewer::nextSlide()
{
    const int next = m_slideList->currentRow() + 1;
    if (next < m_slideList->count())
        m_slideList->setCurrentRow(next);
}

void PptxViewer::previousSlide()
{
    const int previous = m_slideList->currentRow() - 1;
    if (previous >= 0)
        m_slideList->setCurrentRow(previous);
}

void PptxViewer::zoomIn()
{
    m_zoom = qMin(4.0, m_zoom * 1.15);
    applyZoom();
}

void PptxViewer::zoomOut()
{
    m_zoom = qMax(0.25, m_zoom / 1.15);
    applyZoom();
}

void PptxViewer::resetZoom()
{
    m_zoom = 1.0;
    applyZoom();
}

void PptxViewer::applyZoom()
{
    for (int i = 0; i < m_slides->count(); ++i) {
        if (QWidget* canvas = m_slides->widget(i))
            canvas->update();
    }
}

int PptxViewer::shapeCount(int slide) const
{
    return slide >= 0 && slide < m_slideData.size() ? m_slideData.at(slide).shapes.size() : 0;
}

QString PptxViewer::slideText(int slide) const
{
    if (slide < 0 || slide >= m_slideData.size())
        return QString();
    QString text;
    for (const PptxShape& shape : m_slideData.at(slide).shapes) {
        for (const QVector<PptxRun>& paragraph : shape.paragraphs) {
            for (const PptxRun& run : paragraph)
                text += run.text;
            text += QLatin1Char('\n');
        }
    }
    return text;
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

    // 幻灯片尺寸（presentation.xml 的 <p:sldSz>，默认 16:9）
    QSizeF slideSize(960, 540);
    const QByteArray presentation = zip.entry(QStringLiteral("ppt/presentation.xml"));
    if (!presentation.isEmpty()) {
        const QString xml = QString::fromUtf8(presentation);
        const QString szTag = xml.mid(xml.indexOf(QStringLiteral("<p:sldSz")));
        const QString tag = szTag.left(szTag.indexOf(QLatin1Char('>')));
        const double cx = pptAttr(tag, QStringLiteral("cx")).toDouble();
        const double cy = pptAttr(tag, QStringLiteral("cy")).toDouble();
        if (cx > 0 && cy > 0)
            slideSize = QSizeF(emuToPt(cx), emuToPt(cy));
    }

    // 幻灯片列表（数字序）
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

    // 媒体图片（ppt/media/*）按需解码
    QHash<QString, QImage> media;
    const auto loadMedia = [&](const QString& target) {
        if (media.contains(target))
            return;
        const QByteArray bytes = zip.entry(target);
        QImage image;
        if (!bytes.isEmpty())
            image.loadFromData(bytes);
        media.insert(target, image);
    };

    for (int i = 0; i < slideNames.size(); ++i) {
        const QString slideName = slideNames.at(i);
        const QString slideXml = QString::fromUtf8(zip.entry(slideName));

        // 幻灯片关系：rId → 目标路径（相对 ppt/slides → ppt/media/...）
        QHash<QString, QString> rels;
        const QString relsName = QStringLiteral("ppt/slides/_rels/")
                                 + slideName.mid(int(qstrlen("ppt/slides/")))
                                 + QStringLiteral(".rels");
        const QByteArray relsBytes = zip.entry(relsName);
        if (!relsBytes.isEmpty()) {
            const QString relsXml = QString::fromUtf8(relsBytes);
            int pos = 0;
            while (true) {
                const int relAt = relsXml.indexOf(QStringLiteral("<Relationship "), pos);
                if (relAt < 0)
                    break;
                const int relEnd = relsXml.indexOf(QLatin1Char('>'), relAt);
                const QString tag = relsXml.mid(relAt, relEnd - relAt);
                const QString id = pptAttr(tag, QStringLiteral("Id"));
                QString target = pptAttr(tag, QStringLiteral("Target"));
                if (target.startsWith(QStringLiteral("../")))
                    target = QStringLiteral("ppt/") + target.mid(3);
                if (!id.isEmpty() && !target.isEmpty()) {
                    rels.insert(id, target);
                    loadMedia(target);
                }
                pos = relEnd < 0 ? relsXml.size() : relEnd + 1;
            }
        }

        m_slideData.append(parsePptxSlide(slideXml, rels, media, slideSize));
        auto* canvas = new PptxSlideCanvas(&m_slideData.last(), &m_zoom, m_slides);
        canvas->setObjectName(QStringLiteral("pptxSlideCanvas"));
        m_slides->addWidget(canvas);
        m_slideList->addItem(tr("幻灯片 %1").arg(i + 1));
    }

    if (m_slideList->count() > 0)
        m_slideList->setCurrentRow(0);
    showSlide(0);
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