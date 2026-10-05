// Hutaomu Editor - Language description shared by detection, highlighting,
// comment toggling, and the status bar.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

extern "C" {
typedef struct TSLanguage TSLanguage;
}

namespace syntax {

struct LanguageSpec {
    QString id;                     // "cpp"
    QString displayName;            // "C++"
    QStringList extensions;         // "cpp", "h", ...
    QStringList shebangKeywords;    // matched against the first line, e.g. "python"
    QByteArray lineComment;         // empty when the language has none
    QByteArray blockCommentStart;
    QByteArray blockCommentEnd;
    QStringList highlightQueries;   // :/grammars/... resources, concatenated
    TSLanguage* (*parser)() = nullptr;

    bool hasHighlighting() const { return !highlightQueries.isEmpty() && parser; }
    bool hasLineComment() const { return !lineComment.isEmpty(); }
    bool hasBlockComment() const { return !blockCommentStart.isEmpty() && !blockCommentEnd.isEmpty(); }
};

} // namespace syntax
