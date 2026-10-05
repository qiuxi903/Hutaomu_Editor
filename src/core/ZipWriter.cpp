// Hutaomu Editor - Minimal ZIP writer implementation.
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "ZipWriter.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>

#include <zlib.h>

namespace core {

namespace {

void appendU16(QByteArray& out, quint16 value)
{
    out.append(char(value & 0xff));
    out.append(char((value >> 8) & 0xff));
}

void appendU32(QByteArray& out, quint32 value)
{
    out.append(char(value & 0xff));
    out.append(char((value >> 8) & 0xff));
    out.append(char((value >> 16) & 0xff));
    out.append(char((value >> 24) & 0xff));
}

// raw deflate（负 windowBits = 裸 zlib 流，与 ZIP 的 method 8 一致）
QByteArray deflateRaw(const QByteArray& data)
{
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8,
                     Z_DEFAULT_STRATEGY)
        != Z_OK)
        return {};

    QByteArray out;
    out.resize(int(deflateBound(&stream, uLong(data.size()))) + 16);
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.constData()));
    stream.avail_in = uInt(data.size());
    stream.next_out = reinterpret_cast<Bytef*>(out.data());
    stream.avail_out = uInt(out.size());

    const int rc = deflate(&stream, Z_FINISH);
    if (rc != Z_STREAM_END) {
        deflateEnd(&stream);
        return {};
    }
    out.resize(int(stream.total_out));
    deflateEnd(&stream);
    return out;
}

} // namespace

ZipWriter::ZipWriter(const QString& filePath)
    : m_filePath(filePath)
{
}

ZipWriter::~ZipWriter()
{
    close();
}

void ZipWriter::setError(const QString& message)
{
    if (m_error.isEmpty())
        m_error = message;
}

bool ZipWriter::addFile(const QString& archivePath, const QByteArray& data)
{
    if (m_closed) {
        setError(QStringLiteral("ZIP 已关闭，无法继续写入。"));
        return false;
    }
    QString name = archivePath;
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (name.isEmpty() || name.startsWith(QLatin1Char('/'))
        || name.contains(QStringLiteral(".."))) {
        setError(QStringLiteral("非法的条目路径：%1").arg(archivePath));
        return false;
    }

    Entry entry;
    entry.name = name.toUtf8();
    entry.uncompressedSize = quint32(data.size());
    entry.crc = quint32(::crc32(0, reinterpret_cast<const Bytef*>(data.constData()),
                               uInt(data.size())));

    QByteArray payload = deflateRaw(data);
    // 压不动就存原始数据（小文件/已压缩内容）
    if (payload.isEmpty() || payload.size() >= data.size()) {
        payload = data;
        entry.method = 0;
    } else {
        entry.method = 8;
    }
    entry.compressedSize = quint32(payload.size());

    const QDateTime now = QDateTime::currentDateTime();
    const QTime time = now.time();
    const QDate date = now.date();
    entry.dosTime = quint16((time.hour() << 11) | (time.minute() << 5)
                            | (time.second() / 2));
    entry.dosDate = quint16(((date.year() - 1980) << 9) | (date.month() << 5)
                            | date.day());
    entry.localHeaderOffset = quint32(m_buffer.size());

    // local file header
    appendU32(m_buffer, 0x04034b50);
    appendU16(m_buffer, 20);          // version needed
    appendU16(m_buffer, 0);           // flags
    appendU16(m_buffer, entry.method);
    appendU16(m_buffer, entry.dosTime);
    appendU16(m_buffer, entry.dosDate);
    appendU32(m_buffer, entry.crc);
    appendU32(m_buffer, entry.compressedSize);
    appendU32(m_buffer, entry.uncompressedSize);
    appendU16(m_buffer, quint16(entry.name.size()));
    appendU16(m_buffer, 0);           // extra len
    m_buffer.append(entry.name);
    m_buffer.append(payload);

    m_entries.append(entry);
    return true;
}

bool ZipWriter::close()
{
    if (m_closed)
        return m_error.isEmpty();
    m_closed = true;
    if (!m_error.isEmpty())
        return false;

    const quint32 centralOffset = quint32(m_buffer.size());
    for (const Entry& entry : m_entries) {
        appendU32(m_buffer, 0x02014b50);          // central directory header
        appendU16(m_buffer, 20);                  // version made by
        appendU16(m_buffer, 20);                  // version needed
        appendU16(m_buffer, 0);                   // flags
        appendU16(m_buffer, entry.method);
        appendU16(m_buffer, entry.dosTime);
        appendU16(m_buffer, entry.dosDate);
        appendU32(m_buffer, entry.crc);
        appendU32(m_buffer, entry.compressedSize);
        appendU32(m_buffer, entry.uncompressedSize);
        appendU16(m_buffer, quint16(entry.name.size()));
        appendU16(m_buffer, 0);                   // extra len
        appendU16(m_buffer, 0);                   // comment len
        appendU16(m_buffer, 0);                   // disk number
        appendU16(m_buffer, 0);                   // internal attrs
        appendU32(m_buffer, 0);                   // external attrs
        appendU32(m_buffer, entry.localHeaderOffset);
        m_buffer.append(entry.name);
    }
    const quint32 centralSize = quint32(m_buffer.size()) - centralOffset;
    appendU32(m_buffer, 0x06054b50);              // EOCD
    appendU16(m_buffer, 0);
    appendU16(m_buffer, 0);
    appendU16(m_buffer, quint16(m_entries.size()));
    appendU16(m_buffer, quint16(m_entries.size()));
    appendU32(m_buffer, centralSize);
    appendU32(m_buffer, centralOffset);
    appendU16(m_buffer, 0);                       // comment len

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(QStringLiteral("无法写入文件：%1").arg(m_filePath));
        return false;
    }
    if (file.write(m_buffer) != m_buffer.size()) {
        setError(QStringLiteral("写入不完整：%1").arg(m_filePath));
        return false;
    }
    return true;
}

} // namespace core
