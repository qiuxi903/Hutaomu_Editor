// Hutaomu Editor - tree-sitter integration self test.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include <QCoreApplication>
#include <QFile>
#include <cstdio>
#include <cstdlib>

extern "C" {
#include <tree_sitter/api.h>
}

#include "syntax/LanguageRegistry.h"

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

int countCaptures(const char* code, const syntax::LanguageSpec* spec)
{
    TSParser* parser = ts_parser_new();
    if (!ts_parser_set_language(parser, spec->parser())) {
        ts_parser_delete(parser);
        return -1;
    }
    TSTree* tree = ts_parser_parse_string(parser, nullptr, code, uint32_t(strlen(code)));
    if (!tree) {
        ts_parser_delete(parser);
        return -1;
    }

    QByteArray source;
    for (const QString& resource : spec->highlightQueries) {
        QFile file(resource);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            source += file.readAll();
    }

    uint32_t errOffset = 0;
    TSQueryError errType = TSQueryErrorNone;
    TSQuery* query = ts_query_new(spec->parser(), source.constData(),
                                  uint32_t(source.size()), &errOffset, &errType);
    int captures = -1;
    if (query) {
        TSQueryCursor* cursor = ts_query_cursor_new();
        ts_query_cursor_exec(cursor, query, ts_tree_root_node(tree));
        TSQueryMatch match;
        captures = 0;
        while (ts_query_cursor_next_match(cursor, &match))
            captures += int(match.capture_count);
        ts_query_cursor_delete(cursor);
        ts_query_delete(query);
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return captures;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    auto& registry = syntax::LanguageRegistry::instance();

    // --- 语言识别 ---
    expect(registry.detect("main.cpp", "") == registry.findById("cpp"), "detect .cpp as C++");
    expect(registry.detect("main.py", "") == registry.findById("python"), "detect .py as Python");
    expect(registry.detect("x.ts", "") == registry.findById("typescript"), "detect .ts as TypeScript");
    expect(registry.detect("Cargo.toml", "") == registry.findById("toml"), "detect .toml as TOML");
    expect(registry.detect("unknown.xyz", "") == nullptr, "unknown extension -> nullptr");
    expect(registry.detect("run.sh", "") == registry.findById("bash"), "detect .sh as Shell");

    // shebang 识别
    expect(registry.detect("", "#!/usr/bin/env python3\nprint(1)\n") == registry.findById("python"),
           "shebang python detected");

    // --- 解析 + 高亮查询 ---
    struct Case {
        const char* id;
        const char* code;
    };
    const Case cases[] = {
        { "c", "int main() {\n    // comment\n    return 0;\n}\n" },
        { "cpp", "#include <string>\nstruct A { int x; };\nint main() { /* c */ return 0; }\n" },
        { "javascript", "function hi(name) {\n  const s = `hi ${name}`;\n  return s;\n}\n" },
        { "typescript", "interface A { x: number }\nconst a: A = { x: 1 };\n" },
        { "python", "def hi(name: str) -> str:\n    \"\"\"doc\"\"\"\n    return f\"hi {name}\"\n" },
        { "rust", "fn main() {\n    let s: &str = \"hi\";\n    println!(\"{}\", s);\n}\n" },
        { "go", "package main\n\nfunc main() {\n\ts := \"hi\"\n\t_ = s\n}\n" },
        { "java", "public class A {\n    public static void main(String[] a) {\n        int x = 1;\n    }\n}\n" },
        { "c-sharp", "class A {\n    static void Main() {\n        int x = 1;\n    }\n}\n" },
        { "json", "{\n  \"name\": \"hutaomu\",\n  \"n\": 42,\n  \"ok\": true\n}\n" },
        { "html", "<!DOCTYPE html>\n<html><body class=\"x\">hi</body></html>\n" },
        { "css", "body {\n  color: #fff; /* c */\n}\n" },
        { "bash", "#!/bin/sh\n# comment\necho \"hi\"\n" },
        { "yaml", "root:\n  child: \"value\"\n  n: 42\n" },
        { "toml", "[section]\nname = \"hutaomu\"\nn = 42\n" },
        { "php", "<?php\nfunction hi($n) {\n  return \"hi $n\";\n}\n" },
        { "xml", "<?xml version=\"1.0\"?>\n<root a=\"1\"><child>text</child></root>\n" },
    };

    for (const Case& c : cases) {
        const syntax::LanguageSpec* spec = registry.findById(c.id);
        expect(spec != nullptr, (QString("registry has %1").arg(c.id)).toUtf8().constData());
        if (!spec)
            continue;
        const int captures = countCaptures(c.code, spec);
        expect(captures > 5,
               (QString("%1 highlight query yields captures (%2)").arg(c.id).arg(captures))
                   .toUtf8().constData());
    }

    if (failures > 0) {
        std::printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll tree-sitter tests passed.\n");
    return 0;
}
