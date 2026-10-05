// Hutaomu Editor - Office viewers implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "OfficeViewer.h"

#include <QFileInfo>

#include <algorithm>
#include <QLabel>
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
    barRow->addWidget(m_formulaBox, 1);
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
    m_nameBox->setText(cellReference(row, column));
    const QTableWidgetItem* item = table->item(row, column);
    m_formulaBox->setText(item ? item->text() : QString());
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
        struct Cell { int row; int col; QString text; int styleId; };
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

                cells.append({ row, col, text, styleId });
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