// Hutaomu Editor - Document: file I/O plus on-disk metadata.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "Document.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace core {

Document::Document(QObject* parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &Document::onFileChanged);
}

bool Document::loadFrom(const QString& filePath, QString* outText, QString* errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    const QByteArray bytes = file.readAll();
    file.close();

    const Detected detected = detectEncoding(bytes);
    m_encoding = detected.encoding;
    m_lastWrittenContent = bytes;

    QString text = decodeBytes(bytes.mid(detected.bomLength), detected.encoding);
    m_eol = detectEol(text);
    // QTextDocument stores '\n' only; the on-disk convention is reapplied on save.
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    if (outText)
        *outText = text;
    m_filePath = QFileInfo(filePath).absoluteFilePath();
    watchFile();
    return true;
}

bool Document::save(const QString& editorText, const QString& filePath, QString* errorMessage)
{
    if (!writeFile(editorText, filePath, errorMessage))
        return false;

    m_filePath = QFileInfo(filePath).absoluteFilePath();
    watchFile();
    return true;
}

bool Document::writeFile(const QString& editorText, const QString& filePath, QString* errorMessage)
{
    // Atomic write via QSaveFile: a crash mid-save leaves the old file intact.
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    QString text = editorText;
    text.replace(QLatin1Char('\n'), eolString(m_eol));

    bool ok = true;
    const QByteArray bytes = encodeText(text, m_encoding, &ok);
    if (!ok && errorMessage) {
        // Unencodable characters are substituted rather than losing the file.
        *errorMessage = QObject::tr("部分字符无法以 %1 编码，已被替换。")
                            .arg(encodingDisplayName(m_encoding));
    }

    if (file.write(bytes) != bytes.size() || !file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    m_lastWrittenContent = bytes;
    return true;
}

void Document::watchFile()
{
    if (!m_filePath.isEmpty() && !m_watcher.files().contains(m_filePath))
        m_watcher.addPath(m_filePath);
}

void Document::onFileChanged(const QString& path)
{
    if (path != m_filePath)
        return;

    if (!QFileInfo::exists(path)) {
        emit fileDeleted();
        return;
    }

    // Writers that replace the file (QSaveFile, git checkout) remove it
    // from the watch list; re-arm before anything else.
    m_watcher.addPath(path);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    if (file.readAll() == m_lastWrittenContent)
        return; // Our own commit landing on disk, not an external edit.

    emit diskChanged();
}

} // namespace core
