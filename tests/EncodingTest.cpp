// Hutaomu Editor - Encoding detection regression tests.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include <QByteArray>
#include <QCoreApplication>
#include <QStringConverter>
#include <QStringEncoder>
#include <QTextCodec>
#include <cstdio>
#include <cstdlib>

#include "core/Encoding.h"

namespace {

int failures = 0;

void expect(bool condition, const char* what)
{
    if (condition) {
        std::printf("  PASS  %s\n", what);
    } else {
        std::printf("  FAIL  %s\n", what);
        ++failures;
    }
}

void expectEncoding(const core::Detected& d, core::Encoding want, int wantBom, const char* what)
{
    if (d.encoding == want && d.bomLength == wantBom) {
        std::printf("  PASS  %s\n", what);
    } else {
        std::printf("  FAIL  %s (got encoding=%d bom=%d, want encoding=%d bom=%d)\n",
                    what, int(d.encoding), d.bomLength, int(want), wantBom);
        ++failures;
    }
}

QByteArray utf32Be(const QString& text)
{
    QStringEncoder encoder(QStringConverter::Utf32BE, QStringConverter::Flag::Stateless);
    return encoder.encode(text);
}

QByteArray utf16Le(const QString& text)
{
    QStringEncoder encoder(QStringConverter::Utf16LE, QStringConverter::Flag::Stateless);
    return encoder.encode(text);
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString chinese = QStringLiteral("# Hutaomu Editor 测试\n\n这是一个 **测试文件**，用于验证编码。");

    // --- UTF-8 without BOM (regression: was misdetected as UTF-32 BE) ---
    {
        const QByteArray bytes = "# plain ascii and 中文 mixed\nsecond line";
        const core::Detected d = core::detectEncoding(bytes);
        expect(d.encoding == core::Encoding::Utf8 && d.bomLength == 0,
               "UTF-8 without BOM detected as Utf8");

        bool ok = false;
        const QString text = core::decodeBytes(bytes, d.encoding, &ok);
        expect(ok && text.contains(QStringLiteral("中文")), "UTF-8 round trip keeps Chinese");
    }

    // --- UTF-8 with BOM ---
    {
        QByteArray bytes("\xEF\xBB\xBF", 3);
        bytes += QByteArrayLiteral("# BOM file");
        const core::Detected d = core::detectEncoding(bytes);
        expect(d.encoding == core::Encoding::Utf8Bom && d.bomLength == 3,
               "UTF-8 BOM detected with length 3");
        expect(!core::decodeBytes(bytes, d.encoding).startsWith(QChar(0xFEFF)),
               "UTF-8 BOM stripped after decode");
    }

    // --- UTF-16 LE with BOM ---
    {
        QByteArray bytes("\xFF\xFE", 2);
        bytes += utf16Le(chinese);
        const core::Detected d = core::detectEncoding(bytes);
        expect(d.encoding == core::Encoding::Utf16Le && d.bomLength == 2,
               "UTF-16 LE BOM detected with length 2");
        expect(core::decodeBytes(bytes.mid(2), d.encoding) == chinese,
               "UTF-16 LE round trip");
    }

    // --- UTF-32 BE with BOM (the exact bytes of the regression) ---
    {
        QByteArray bytes("\x00\x00\xFE\xFF", 4);
        bytes += utf32Be(chinese);
        const core::Detected d = core::detectEncoding(bytes);
        expect(d.encoding == core::Encoding::Utf32Be && d.bomLength == 4,
               "UTF-32 BE BOM detected with length 4");
        expect(core::decodeBytes(bytes.mid(4), d.encoding) == chinese,
               "UTF-32 BE round trip");
    }

    // --- GB18030 (legacy Chinese, no BOM) ---
    {
        QTextCodec* gb = QTextCodec::codecForName("GB18030");
        expect(gb != nullptr, "GB18030 codec available");
        if (gb) {
            const QByteArray bytes = gb->fromUnicode(chinese);
            const core::Detected d = core::detectEncoding(bytes);
            expectEncoding(d, core::Encoding::Gb18030, 0,
                           "GB18030 text detected as Gb18030");
            expect(core::decodeBytes(bytes, d.encoding) == chinese,
                   "GB18030 round trip keeps Chinese");
        }
    }

    // --- Latin-1 last resort (invalid in both UTF-8 and GB18030) ---
    {
        const QByteArray bytes("\xFF", 1); // 0xFF is invalid everywhere in GB18030
        const core::Detected d = core::detectEncoding(bytes);
        expect(d.encoding == core::Encoding::Latin1, "invalid bytes fall back to Latin-1");
    }

    // --- Encoding display names (status bar) ---
    expect(core::encodingDisplayName(core::Encoding::Utf8) == QStringLiteral("UTF-8"),
           "display name for UTF-8");
    expect(core::encodingDisplayName(core::Encoding::Gb18030) == QStringLiteral("GB18030"),
           "display name for GB18030");

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll encoding tests passed.\n");
    return 0;
}
