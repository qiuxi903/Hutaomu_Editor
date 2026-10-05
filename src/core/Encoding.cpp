// Hutaomu Editor - Text encoding detection and conversion.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "Encoding.h"

#include <QStringConverter>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QTextCodec> // Qt6::Core5Compat, for GB18030

namespace core {

namespace {
// BOM constants must be length-constructed QByteArrays: a "\x00..." literal
// decays to const char* and truncates at the first NUL byte.
const QByteArray kUtf8Bom("\xEF\xBB\xBF", 3);
const QByteArray kUtf16LeBom("\xFF\xFE", 2);
const QByteArray kUtf16BeBom("\xFE\xFF", 2);
const QByteArray kUtf32LeBom("\xFF\xFE\x00\x00", 4);
const QByteArray kUtf32BeBom("\x00\x00\xFE\xFF", 4);
} // namespace

QString encodingDisplayName(Encoding encoding)
{
    switch (encoding) {
    case Encoding::Utf8:    return QStringLiteral("UTF-8");
    case Encoding::Utf8Bom: return QStringLiteral("UTF-8 with BOM");
    case Encoding::Utf16Le: return QStringLiteral("UTF-16 LE");
    case Encoding::Utf16Be: return QStringLiteral("UTF-16 BE");
    case Encoding::Utf32Le: return QStringLiteral("UTF-32 LE");
    case Encoding::Utf32Be: return QStringLiteral("UTF-32 BE");
    case Encoding::Gb18030: return QStringLiteral("GB18030");
    case Encoding::Latin1:  return QStringLiteral("Latin-1");
    }
    return QStringLiteral("UTF-8");
}

Detected detectEncoding(const QByteArray& bytes)
{
    Detected result{ Encoding::Utf8, 0 };

    if (bytes.startsWith(kUtf8Bom)) {
        result = { Encoding::Utf8Bom, 3 };
    } else if (bytes.startsWith(kUtf32LeBom)) {
        result = { Encoding::Utf32Le, 4 };
    } else if (bytes.startsWith(kUtf32BeBom)) {
        result = { Encoding::Utf32Be, 4 };
    } else if (bytes.startsWith(kUtf16LeBom)) {
        result = { Encoding::Utf16Le, 2 };
    } else if (bytes.startsWith(kUtf16BeBom)) {
        result = { Encoding::Utf16Be, 2 };
    } else if (!bytes.isEmpty()) {
        // No BOM. Strict UTF-8 validation first; only fall back when it
        // fails. GB18030 accepts most byte sequences, so it would mask
        // valid UTF-8 if tried first — order matters here.
        QStringDecoder utf8(QStringConverter::Utf8);
        // DecodedData is lazy: only converting it to QString runs the decode.
        const QString probe = utf8(bytes);
        Q_UNUSED(probe);
        if (utf8.hasError()) {
            QTextCodec* gb = QTextCodec::codecForName("GB18030");
            if (gb) {
                QTextCodec::ConverterState state;
                gb->toUnicode(bytes.constData(), bytes.size(), &state);
                if (state.invalidChars == 0)
                    result = { Encoding::Gb18030, 0 };
                else
                    result = { Encoding::Latin1, 0 };
            } else {
                result = { Encoding::Latin1, 0 };
            }
        }
    }

    return result;
}

namespace {

QStringConverter::Encoding converterEncoding(Encoding encoding, bool* supported)
{
    *supported = true;
    switch (encoding) {
    case Encoding::Utf8:
    case Encoding::Utf8Bom: return QStringConverter::Utf8;
    case Encoding::Utf16Le: return QStringConverter::Utf16LE;
    case Encoding::Utf16Be: return QStringConverter::Utf16BE;
    case Encoding::Utf32Le: return QStringConverter::Utf32LE;
    case Encoding::Utf32Be: return QStringConverter::Utf32BE;
    default:                *supported = false; return QStringConverter::Utf8;
    }
}

QTextCodec* codecFor(Encoding encoding)
{
    switch (encoding) {
    case Encoding::Gb18030: return QTextCodec::codecForName("GB18030");
    case Encoding::Latin1:  return QTextCodec::codecForName("ISO-8859-1");
    default:                return nullptr;
    }
}

} // namespace

QString decodeBytes(const QByteArray& bytes, Encoding encoding, bool* ok)
{
    if (ok)
        *ok = true;

    bool supported = false;
    const QStringConverter::Encoding conv = converterEncoding(encoding, &supported);
    if (supported) {
        QStringDecoder decoder(conv);
        QString text = decoder(bytes);
        if (decoder.hasError() && ok)
            *ok = false;
        // QStringConverter keeps a UTF-8 BOM as U+FEFF; drop it.
        if (encoding == Encoding::Utf8Bom && text.startsWith(QChar(0xFEFF)))
            text.remove(0, 1);
        return text;
    }

    if (QTextCodec* codec = codecFor(encoding)) {
        QTextCodec::ConverterState state;
        QString text = codec->toUnicode(bytes.constData(), bytes.size(), &state);
        if (state.invalidChars > 0 && ok)
            *ok = false;
        return text;
    }

    if (ok)
        *ok = false;
    return QString::fromLatin1(bytes);
}

QByteArray encodeText(const QString& text, Encoding encoding, bool* ok)
{
    if (ok)
        *ok = true;

    bool supported = false;
    const QStringConverter::Encoding conv = converterEncoding(encoding, &supported);
    if (supported) {
        QStringEncoder encoder(conv);
        QByteArray bytes = encoder(text);
        if (encoder.hasError() && ok)
            *ok = false;
        if (encoding == Encoding::Utf8Bom && !bytes.startsWith(kUtf8Bom))
            bytes.prepend(kUtf8Bom);
        return bytes;
    }

    if (QTextCodec* codec = codecFor(encoding)) {
        QByteArray bytes = codec->fromUnicode(text);
        return bytes;
    }

    if (ok)
        *ok = false;
    return text.toLatin1();
}

} // namespace core
