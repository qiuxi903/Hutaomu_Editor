// Hutaomu Editor - Minimal ZIP reader implementation.
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "ZipReader.h"

#include <zlib.h>

namespace viewers {

namespace {
constexpr quint32 kCentralSignature = 0x02014b50;
constexpr quint32 kLocalSignature = 0x04034b50;

quint16 readU16(const QByteArray& d, int pos)
{
    if (pos + 2 > d.size())
        return 0;
    return quint16(quint8(d.at(pos))) | quint16(quint8(d.at(pos + 1))) << 8;
}

quint32 readU32(const QByteArray& d, int pos)
{
    if (pos + 4 > d.size())
        return 0;
    return quint32(quint8(d.at(pos)))
           | quint32(quint8(d.at(pos + 1))) << 8
           | quint32(quint8(d.at(pos + 2))) << 16
           | quint32(quint8(d.at(pos + 3))) << 24;
}
} // namespace

ZipReader::ZipReader(const QByteArray& data)
    : m_data(data)
{
    // 总大小上限：OOXML 包超过 512MB 视为异常（防损坏/恶意文件）
    if (m_data.size() > 512 * 1024 * 1024)
        return;
    if (m_data.size() < 22)
        return;
    // EOCD 至少 22 字节；注释区最长 65535
    int eocd = -1;
    for (int i = m_data.size() - 22; i >= qMax(0, m_data.size() - 22 - 65535); --i) {
        if (readU32(m_data, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0)
        return;
    const int cdCount = readU16(m_data, eocd + 10);
    int cdOffset = int(readU32(m_data, eocd + 16));

    for (int i = 0; i < cdCount; ++i) {
        if (cdOffset + 46 > m_data.size()
            || readU32(m_data, cdOffset) != kCentralSignature)
            break;
        const quint16 method = readU16(m_data, cdOffset + 10);
        const quint32 csize = readU32(m_data, cdOffset + 20);
        const quint32 usize = readU32(m_data, cdOffset + 24);
        const quint16 nameLen = readU16(m_data, cdOffset + 28);
        const quint16 extraLen = readU16(m_data, cdOffset + 30);
        const quint16 commentLen = readU16(m_data, cdOffset + 32);
        const quint32 localOffset = readU32(m_data, cdOffset + 42);

        if (cdOffset + 46 + nameLen > m_data.size())
            break;
        const QString name = QString::fromUtf8(
            m_data.mid(cdOffset + 46, nameLen));

        if (!name.endsWith(QLatin1Char('/'))) { // 跳过目录条目
            EntryInfo info;
            info.offset = localOffset;
            info.compressedSize = csize;
            info.uncompressedSize = usize;
            info.method = method;
            m_entries.insert(name, info);
        }
        cdOffset += 46 + nameLen + extraLen + commentLen;
    }
    m_valid = !m_entries.isEmpty();
}

QStringList ZipReader::entries() const
{
    return m_entries.keys();
}

bool ZipReader::hasEntry(const QString& name) const
{
    return m_entries.contains(name);
}

QByteArray ZipReader::decompress(const EntryInfo& info) const
{
    // local file header：读文件名长度跳过额外字段
    const int lh = int(info.offset);
    if (lh + 30 > m_data.size() || readU32(m_data, lh) != kLocalSignature)
        return {};
    const quint16 nameLen = readU16(m_data, lh + 26);
    const quint16 extraLen = readU16(m_data, lh + 28);
    const int dataStart = lh + 30 + nameLen + extraLen;
    if (dataStart + int(info.compressedSize) > m_data.size())
        return {};
    const QByteArray compressed = m_data.mid(dataStart, int(info.compressedSize));

    if (info.method == 0) {
        // store：直接返回（大小已在解析时校验）
        return compressed; // store
    }
    if (info.method != 8)
        return {}; // 未知压缩方法

    // 解压大小上限：单条目 256MB（防 zip bomb / 损坏 size 字段）
    const qsizetype expected = qsizetype(info.uncompressedSize);
    if (expected > 256 * 1024 * 1024)
        return {};

    // raw deflate
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, -15) != Z_OK)
        return {};
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.constData()));
    zs.avail_in = uInt(compressed.size());
    QByteArray out;
    out.resize(expected > 0 ? int(expected) : qMax(65536, compressed.size() * 4));
    int ret = Z_OK;
    while (true) {
        if (zs.total_out >= out.size()) {
            if (out.size() >= 256 * 1024 * 1024) {
                inflateEnd(&zs);
                return {}; // 超限
            }
            out.resize(qMin(qsizetype(256 * 1024 * 1024), out.size() * 2));
        }
        zs.next_out = reinterpret_cast<Bytef*>(out.data() + zs.total_out);
        zs.avail_out = uInt(out.size() - zs.total_out);
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret == Z_STREAM_END || ret != Z_OK)
            break;
    }
    inflateEnd(&zs);
    if (ret != Z_STREAM_END)
        return {};
    out.resize(zs.total_out);
    return out;
}

QByteArray ZipReader::entry(const QString& name) const
{
    const auto it = m_entries.constFind(name);
    if (it == m_entries.constEnd())
        return {};
    return decompress(it.value());
}

} // namespace viewers