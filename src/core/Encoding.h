// Hutaomu Editor - Text encoding detection and conversion.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QByteArray>
#include <QString>

namespace core {

enum class Encoding {
    Utf8,       // No BOM, valid UTF-8 (includes plain ASCII)
    Utf8Bom,
    Utf16Le,
    Utf16Be,
    Utf32Le,
    Utf32Be,
    Gb18030,    // Fallback for legacy Chinese text
    Latin1,     // Last resort; never fails
};

// Highest-fidelity name shown in the status bar.
QString encodingDisplayName(Encoding encoding);

struct Detected {
    Encoding encoding;
    int bomLength;
};

Detected detectEncoding(const QByteArray& bytes);

// Decode/encode. `ok` is set to false when the round trip cannot be
// performed faithfully (invalid input or unconvertible characters).
QString decodeBytes(const QByteArray& bytes, Encoding encoding, bool* ok = nullptr);
QByteArray encodeText(const QString& text, Encoding encoding, bool* ok = nullptr);

} // namespace core
