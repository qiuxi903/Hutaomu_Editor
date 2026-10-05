// Hutaomu Editor - Minimal ZIP writer (store + raw deflate via zlib).
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace core {

// 只写不读的 ZIP 写入器：用于导出主题包（.htmtpi）与插件包（.htmed）。
// 与 viewers::ZipReader 对应（读取侧），两边都用 zlib。
// 条目按添加顺序写入；close() 写中央目录与 EOCD。
class ZipWriter {
public:
    explicit ZipWriter(const QString& filePath);
    ~ZipWriter();

    ZipWriter(const ZipWriter&) = delete;
    ZipWriter& operator=(const ZipWriter&) = delete;

    // archivePath 用 '/' 分隔；目录条目可省略（读取侧会自动建目录）。
    bool addFile(const QString& archivePath, const QByteArray& data);

    bool close();          // 幂等；析构时若未 close 会自动尝试
    bool failed() const { return !m_error.isEmpty(); }
    QString errorString() const { return m_error; }

private:
    struct Entry {
        QByteArray name;
        quint32 crc = 0;
        quint32 compressedSize = 0;
        quint32 uncompressedSize = 0;
        quint16 method = 0;
        quint32 localHeaderOffset = 0;
        quint16 dosTime = 0;
        quint16 dosDate = 0;
    };

    void setError(const QString& message);
    QString m_filePath;
    QByteArray m_buffer;   // 整个 zip 先攒在内存：主题/插件包都是小文件
    QList<Entry> m_entries;
    QString m_error;
    bool m_closed = false;
};

} // namespace core
