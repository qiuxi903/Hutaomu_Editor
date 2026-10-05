// Hutaomu Editor - Language registry: known languages, detection, lookup.
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "LanguageRegistry.h"

#include <QFileInfo>

#include <QHash>

extern "C" {
const TSLanguage* tree_sitter_c(void);
const TSLanguage* tree_sitter_cpp(void);
const TSLanguage* tree_sitter_c_sharp(void);
const TSLanguage* tree_sitter_java(void);
const TSLanguage* tree_sitter_javascript(void);
const TSLanguage* tree_sitter_typescript(void);
const TSLanguage* tree_sitter_python(void);
const TSLanguage* tree_sitter_rust(void);
const TSLanguage* tree_sitter_go(void);
const TSLanguage* tree_sitter_json(void);
const TSLanguage* tree_sitter_html(void);
const TSLanguage* tree_sitter_css(void);
const TSLanguage* tree_sitter_bash(void);
const TSLanguage* tree_sitter_yaml(void);
const TSLanguage* tree_sitter_toml(void);
const TSLanguage* tree_sitter_php(void);
const TSLanguage* tree_sitter_xml(void);
}

// tree-sitter API 声明为返回 TSLanguage*，这里统一签名。
namespace {
TSLanguage* langC() { return const_cast<TSLanguage*>(tree_sitter_c()); }
TSLanguage* langCpp() { return const_cast<TSLanguage*>(tree_sitter_cpp()); }
TSLanguage* langCSharp() { return const_cast<TSLanguage*>(tree_sitter_c_sharp()); }
TSLanguage* langJava() { return const_cast<TSLanguage*>(tree_sitter_java()); }
TSLanguage* langJavaScript() { return const_cast<TSLanguage*>(tree_sitter_javascript()); }
TSLanguage* langTypeScript() { return const_cast<TSLanguage*>(tree_sitter_typescript()); }
TSLanguage* langPython() { return const_cast<TSLanguage*>(tree_sitter_python()); }
TSLanguage* langRust() { return const_cast<TSLanguage*>(tree_sitter_rust()); }
TSLanguage* langGo() { return const_cast<TSLanguage*>(tree_sitter_go()); }
TSLanguage* langJson() { return const_cast<TSLanguage*>(tree_sitter_json()); }
TSLanguage* langHtml() { return const_cast<TSLanguage*>(tree_sitter_html()); }
TSLanguage* langCss() { return const_cast<TSLanguage*>(tree_sitter_css()); }
TSLanguage* langBash() { return const_cast<TSLanguage*>(tree_sitter_bash()); }
TSLanguage* langYaml() { return const_cast<TSLanguage*>(tree_sitter_yaml()); }
TSLanguage* langToml() { return const_cast<TSLanguage*>(tree_sitter_toml()); }
TSLanguage* langPhp() { return const_cast<TSLanguage*>(tree_sitter_php()); }
TSLanguage* langXml() { return const_cast<TSLanguage*>(tree_sitter_xml()); }
} // namespace

namespace syntax {

LanguageRegistry& LanguageRegistry::instance()
{
    static LanguageRegistry s_registry;
    return s_registry;
}

LanguageRegistry::LanguageRegistry()
{
    QList<LanguageSpec> specs;
    specs.append({ "c", "C", { "c", "h" }, {},
                   "//", "/*", "*/",
                   { ":/grammars/tree-sitter-c/queries/highlights.scm" }, &langC });
    specs.append({ "cpp", "C++", { "cpp", "cxx", "cc", "hpp", "hh", "c++", "ipp" }, {},
                   "//", "/*", "*/",
                   { ":/grammars/tree-sitter-c/queries/highlights.scm",
                     ":/grammars/tree-sitter-cpp/queries/highlights.scm" }, &langCpp });
    specs.append({ "c-sharp", "C#", { "cs" }, {},
                   "//", "/*", "*/",
                   { ":/grammars/tree-sitter-c-sharp/queries/highlights.scm" }, &langCSharp });
    specs.append({ "java", "Java", { "java" }, {},
                   "//", "/*", "*/",
                   { ":/grammars/tree-sitter-java/queries/highlights.scm" }, &langJava });
    specs.append({ "javascript", "JavaScript", { "js", "mjs", "cjs" }, { "node" },
                   "//", "/*", "*/",
                   { ":/grammars/tree-sitter-javascript/queries/highlights.scm" }, &langJavaScript });
    specs.append({ "typescript", "TypeScript", { "ts" }, {},
                   "//", "/*", "*/",
                   { ":/grammars/tree-sitter-javascript/queries/highlights.scm",
                     ":/grammars/tree-sitter-typescript/queries/highlights.scm" }, &langTypeScript });
    specs.append({ "python", "Python", { "py", "pyw", "pyi" }, { "python", "python3" },
                   "#", "", "",
                   { ":/grammars/tree-sitter-python/queries/highlights.scm" }, &langPython });
    specs.append({ "rust", "Rust", { "rs" }, { "rust" },
                   "//", "/*", "*/",
                   { ":/grammars/tree-sitter-rust/queries/highlights.scm" }, &langRust });
    specs.append({ "go", "Go", { "go" }, { "go" },
                   "//", "/*", "*/",
                   { ":/grammars/tree-sitter-go/queries/highlights.scm" }, &langGo });
    specs.append({ "json", "JSON", { "json" }, {},
                   "", "", "",
                   { ":/grammars/tree-sitter-json/queries/highlights.scm" }, &langJson });
    specs.append({ "html", "HTML", { "html", "htm" }, {},
                   "", "<!--", "-->",
                   { ":/grammars/tree-sitter-html/queries/highlights.scm" }, &langHtml });
    specs.append({ "css", "CSS", { "css" }, {},
                   "", "/*", "*/",
                   { ":/grammars/tree-sitter-css/queries/highlights.scm" }, &langCss });
    specs.append({ "bash", "Shell", { "sh", "bash", "zsh" }, { "sh", "bash", "zsh" },
                   "#", "", "",
                   { ":/grammars/tree-sitter-bash/queries/highlights.scm" }, &langBash });
    specs.append({ "yaml", "YAML", { "yml", "yaml" }, {},
                   "#", "", "",
                   { ":/grammars/tree-sitter-yaml/queries/highlights.scm" }, &langYaml });
    specs.append({ "toml", "TOML", { "toml" }, {},
                   "#", "", "",
                   { ":/grammars/tree-sitter-toml/queries/highlights.scm" }, &langToml });
    // SQL 语法仓未随发行版提供 parser，暂只提供识别与注释切换。
    specs.append({ "sql", "SQL", { "sql" }, { "sql" },
                   "--", "/*", "*/",
                   {}, nullptr });
    specs.append({ "php", "PHP", { "php" }, { "php" },
                   "//", "/*", "*/",
                   { ":/grammars/tree-sitter-php/queries/highlights.scm" }, &langPhp });
    specs.append({ "xml", "XML", { "xml", "xsl", "xslt" }, {},
                   "", "<!--", "-->",
                   { ":/grammars/tree-sitter-xml/queries/xml/highlights.scm" }, &langXml });
    // 纯文本：无高亮、无注释切换，但有正式条目 —— 状态栏可显示语言名，
    // 插件也能把新扩展名（如 *.log）认领到它上面（v1 的 formats 贡献点）。
    specs.append({ "plaintext", "纯文本", { "txt", "text" }, {},
                   "", "", "",
                   {}, nullptr });
    specs.append({ "markdown", "Markdown", { "md", "markdown", "mkd" }, {},
                   "", "<!--", "-->",
                   {}, nullptr });

    for (const LanguageSpec& spec : specs) {
        auto* stored = new LanguageSpec(spec);
        m_languages.append(stored);
        m_byId.insert(stored->id, stored);
        for (const QString& ext : stored->extensions)
            m_byExtension.insert(ext, stored);
    }
}

void LanguageRegistry::setExtensionOverrides(const QHash<QString, QString>& extensionToLanguage)
{
    m_extensionOverrides = extensionToLanguage;
}

const LanguageSpec* LanguageRegistry::detect(const QString& filePath, const QString& content) const
{
    // 1. 扩展名
    const QString ext = QFileInfo(filePath).suffix().toLower();

    // 插件认领的扩展名优先（如 *.log）
    if (!ext.isEmpty()) {
        const QString override = m_extensionOverrides.value(ext);
        if (!override.isEmpty()) {
            if (const LanguageSpec* spec = findById(override))
                return spec;
        }
    }
    if (!ext.isEmpty() && m_byExtension.contains(ext))
        return m_byExtension.value(ext);

    // 2. Shebang
    if (content.startsWith(QStringLiteral("#!"))) {
        const QString firstLine = content.section(QLatin1Char('\n'), 0, 0);
        for (const LanguageSpec* spec : m_languages) {
            for (const QString& keyword : spec->shebangKeywords) {
                if (firstLine.contains(keyword))
                    return spec;
            }
        }
    }

    // 3. 内容嗅探（少量易混淆类型）
    const QString trimmed = content.left(4096);
    if (trimmed.startsWith(QStringLiteral("<?xml")))
        return findById("xml");
    if (trimmed.contains(QStringLiteral("<!DOCTYPE html"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("<html"), Qt::CaseInsensitive))
        return findById("html");
    if (trimmed.startsWith(QStringLiteral("{")) || trimmed.startsWith(QStringLiteral("["))) {
        // 粗略 JSON 判断：首行像键值对结构
        if (trimmed.contains(QStringLiteral("\":")) || trimmed.contains(QStringLiteral("\": ")))
            return findById("json");
    }

    return nullptr;
}

const LanguageSpec* LanguageRegistry::findById(const QString& id) const
{
    return m_byId.value(id, nullptr);
}

} // namespace syntax
