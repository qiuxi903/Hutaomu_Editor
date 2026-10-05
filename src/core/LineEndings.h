// Hutaomu Editor - Line ending conventions.
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QString>

namespace core {

enum class Eol {
    Lf,   // Unix
    Crlf, // Windows
    Cr,   // Classic Mac (rare, kept for fidelity)
};

inline QString eolString(Eol eol)
{
    switch (eol) {
    case Eol::Crlf: return QStringLiteral("\r\n");
    case Eol::Cr:   return QStringLiteral("\r");
    case Eol::Lf:   break;
    }
    return QStringLiteral("\n");
}

inline QString eolDisplayName(Eol eol)
{
    switch (eol) {
    case Eol::Crlf: return QStringLiteral("CRLF");
    case Eol::Cr:   return QStringLiteral("CR");
    case Eol::Lf:   break;
    }
    return QStringLiteral("LF");
}

// QTextDocument normalizes all line endings to '\n' internally, so the
// on-disk convention is tracked here and re-applied when saving.
inline Eol detectEol(const QString& text)
{
    const qsizetype crlf = text.indexOf(QStringLiteral("\r\n"));
    if (crlf >= 0)
        return Eol::Crlf;
    if (text.contains(QLatin1Char('\r')))
        return Eol::Cr;
    return Eol::Lf;
}

} // namespace core
