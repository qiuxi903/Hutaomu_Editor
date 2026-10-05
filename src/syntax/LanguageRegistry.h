// Hutaomu Editor - Language registry: known languages, detection, lookup.
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include "syntax/LanguageSpec.h"

namespace syntax {

class LanguageRegistry {
public:
    static LanguageRegistry& instance();

    // 扩展名 → shebang → 内容嗅探（按顺序）。
    const LanguageSpec* detect(const QString& filePath, const QString& content) const;
    // 插件贡献的扩展名映射（扩展名 → 语言 id）：由 PluginManager 在加载/启停
    // 插件时推入，优先于内置检测。放在这里是为了让语法层不反向依赖插件层。
    void setExtensionOverrides(const QHash<QString, QString>& extensionToLanguage);
    const LanguageSpec* findById(const QString& id) const;
    const QList<const LanguageSpec*>& languages() const { return m_languages; }

private:
    LanguageRegistry();

    QList<const LanguageSpec*> m_languages;
    QHash<QString, const LanguageSpec*> m_byId;
    QHash<QString, const LanguageSpec*> m_byExtension;
    QHash<QString, QString> m_extensionOverrides;
};

} // namespace syntax
