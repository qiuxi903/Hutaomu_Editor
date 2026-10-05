// Hutaomu Editor - Minimal ZIP reader (for OOXML docx/xlsx/pptx).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

namespace viewers {

// 只读 ZIP 提取器：解析 central directory，按需解压单个条目
// （deflate = zlib raw / store = 原样）。供 OOXML 查看器读取
// word/document.xml、xl/worksheets/*.xml 等。
class ZipReader {
public:
    explicit ZipReader(const QByteArray& data);

    bool isValid() const { return m_valid; }
    QStringList entries() const;
    bool hasEntry(const QString& name) const;
    QByteArray entry(const QString& name) const; // 解压后的内容

private:
    struct EntryInfo {
        quint32 offset = 0; // local header 在文件中的偏移
        quint32 compressedSize = 0;
        quint32 uncompressedSize = 0;
        quint16 method = 0; // 0=store 8=deflate
    };

    QByteArray m_data;
    QHash<QString, EntryInfo> m_entries;
    bool m_valid = false;

    QByteArray decompress(const EntryInfo& info) const;
};

} // namespace viewers