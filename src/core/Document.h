// Hutaomu Editor - Document: file I/O plus on-disk metadata.
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QByteArray>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>

#include "core/Encoding.h"
#include "core/LineEndings.h"

namespace syntax {
class LanguageSpec;
}

namespace core {

// Owns everything about a file that outlives the visible text: where it
// lives, how it is encoded, and its line-ending convention. The edited
// text itself lives in the editor widget; this class deliberately never
// stores a second copy. Reload-vs-conflict policy is decided by the UI,
// which knows the buffer state.
class Document : public QObject {
    Q_OBJECT
public:
    explicit Document(QObject* parent = nullptr);

    // Reads the file, updates encoding/EOL metadata, and returns the
    // text with all line endings normalized to '\n' for QTextDocument.
    bool loadFrom(const QString& filePath, QString* outText, QString* errorMessage);

    // Writes `editorText` ('\n'-normalized) using the document's EOL and
    // encoding. Writing an untitled document fails; the UI must route
    // that through Save As first.
    bool save(const QString& editorText, const QString& filePath, QString* errorMessage);

    const QString& filePath() const { return m_filePath; }
    bool isUntitled() const { return m_filePath.isEmpty(); }
    Encoding encoding() const { return m_encoding; }
    void setEncoding(Encoding encoding) { m_encoding = encoding; }
    const syntax::LanguageSpec* language() const { return m_language; }
    void setLanguage(const syntax::LanguageSpec* spec) { m_language = spec; }
    Eol eol() const { return m_eol; }
    void setEol(Eol eol) { m_eol = eol; }

signals:
    // The file changed on disk in a way that is NOT our own write.
    void diskChanged();
    // The file was removed from disk. The watch is silently lost after
    // this until the document is saved or reloaded again.
    void fileDeleted();

private:
    bool writeFile(const QString& editorText, const QString& filePath, QString* errorMessage);
    void watchFile();
    void onFileChanged(const QString& path);

    QString m_filePath;
    Encoding m_encoding = Encoding::Utf8;
    Eol m_eol = Eol::Lf;
    const syntax::LanguageSpec* m_language = nullptr;
    QFileSystemWatcher m_watcher;
    // Content fingerprint of the last successful load or save, used to
    // recognize our own QSaveFile commits in watcher events.
    QByteArray m_lastWrittenContent;
};

} // namespace core
