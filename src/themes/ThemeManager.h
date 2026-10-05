// Hutaomu Editor - Theme definitions and loading (JSON color palettes +
// T2 metrics rendered into a parameterized QSS template).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace editor {

// 自定义绘制控件所需的颜色 token（QSS 无法触达 QWidget::paintEvent）。
struct ThemeTokens {
    QColor accent;                       // 品牌强调色
    QColor titleBarBackground;
    QColor editorBackground;
    QColor editorForeground;
    QColor currentLineBackground;
    QColor selectionBackground;
    QColor lineNumberBackground;
    QColor lineNumberForeground;
    QColor lineNumberActiveForeground;
    QColor activityBarBackground;
    QColor sidebarBackground;
    QColor searchHighlightBackground;    // 查找匹配底色
};

// 编辑区背景图规格（T3）：图片路径相对主题根目录（如 "assets/background.png"）。
struct ThemeBackground {
    QString image;                                        // 空 = 无背景图
    QString mode = QStringLiteral("tile");                // tile / center / stretch
    qreal opacity = 1.0;                                  // 0..1，叠在 editorBg 之上
    bool isValid() const { return !image.isEmpty(); }
};

// 界面形态（T4）：主题用 layout.json 声明的有限开关集合 —— 主题不写代码，
// 宿主按这些开关显隐/样式化既有区域。空 optional = 该主题不管这项。
struct ThemeLayout {
    std::optional<bool> activityBarVisible;
    std::optional<bool> statusBarVisible;
    std::optional<bool> sidebarVisible;    // 资源管理器 + 搜索（侧栏卡片列）
    std::optional<bool> outlineVisible;
    std::optional<bool> previewVisible;
    QString sidebarStyle;                  // "" | plain | card
    QString tabsStyle;                     // "" | tab | underline | pill | hidden
    std::optional<bool> centered;          // 编辑区内容居中限宽
    std::optional<int> maxWidth;           // 居中时的栏宽（px）
    std::optional<bool> headingUnderline;  // 标题下划线（h1/h2）
    bool immersive = false;                // 专注写作预设（上面的键可覆盖其取值）

    bool isDefault() const
    {
        return !activityBarVisible && !statusBarVisible && !sidebarVisible
               && !outlineVisible && !previewVisible && sidebarStyle.isEmpty()
               && tabsStyle.isEmpty() && !centered && !maxWidth && !headingUnderline
               && !immersive;
    }
    // 人类可读的变更摘要（"隐藏活动栏、标签下划线"），用于应用前提示。
    QString summary() const;
};

// 一套完整主题：id + 显示名 + 颜色表（扁平键名，如 "editorBg"、"syntax.keyword"）
// + 度量表（T2 能力，如 "treeRowHeight"、"editorFontSize"）
// + 资源（T3：品牌 logo、活动栏图标覆盖、编辑区背景图）。
struct ThemeDefinition {
    QString id;
    QString name;
    QString description;
    bool dark = false;
    bool builtin = true;              // 内置（编译进资源）vs 用户安装
    QString rootPath;                 // 主题根目录（内置为 ":/themes"，安装版为磁盘目录）
    QString previewPath;              // 预览图路径，可为空
    QStringList capabilities;         // colors / metrics / assets / layout
    QString sourcePluginId;           // 由插件提供时 = 插件 id（不可单独删除）
    ThemeBackground background;       // T3：编辑区背景图
    ThemeLayout layout;               // T4：界面形态（layout.json）
    QHash<QString, QColor> colors;
    QHash<QString, QString> metrics;  // 原始值均为字符串，渲染进 QSS 或由 C++ 读取

    QColor color(const QString& key) const { return colors.value(key, QColor()); }
    QString metric(const QString& key) const { return metrics.value(key); }
};

class ThemeManager {
public:
    // 应用指定主题（id 不存在时回退默认主题）。
    static void applyTheme(const QString& themeId);

    static QString theme();                  // 当前主题 id
    static bool currentIsDark();
    static const ThemeTokens& tokens();

    static QList<ThemeDefinition> availableThemes(); // 多来源合并（内置 → 已安装）
    static ThemeDefinition themeDefinition(const QString& themeId);
    static ThemeDefinition currentTheme();           // 当前生效主题的完整定义
    static const ThemeLayout& currentLayout();       // 当前主题声明的界面形态

    // T3 资源解析：把包内相对路径（"assets/icons/brand.svg"）解析成实际路径。
    // 只接受主题根目录内的相对路径；文件不存在返回空串（调用方回退内置资源）。
    // 传入 themeId 可查询任意主题，空串表示当前主题。
    static QString assetPath(const QString& relativePath, const QString& themeId = QString());
    static QColor syntaxColor(const QString& role);  // "keyword"/"string"...
    static QColor color(const QString& key);         // 任意主题色键

    // T2 度量：默认值 + 密度预设 + 主题覆盖后的最终值。
    static QString metric(const QString& key);
    static int metricInt(const QString& key, int fallback = 0);
    static qreal metricReal(const QString& key, qreal fallback = 0.0);

    // 主题包安装 / 卸载 / 导出（.htmtpi 为 ZIP；安装也接受目录形态）。
    static QString userThemeDirectory();     // <AppData>/Hutaomu/themes
    static bool installThemePackage(const QString& packagePath, QString* themeId,
                                    QString* error);
    static bool uninstallTheme(const QString& themeId, QString* error);
    // 把任意来源（内置资源 / 已安装 / 插件提供）的主题导出为 .htmtpi，
    // 便于分享或备份；扁平 JSON 主题会被导出为标准的目录形态。
    static bool exportThemePackage(const QString& themeId, const QString& targetFile,
                                   QString* error);

    // 渲染后的样式表（含 @m.* 度量替换），供测试与预览使用。
    static QString renderedStyleSheet(const QString& themeId);

private:
    ThemeManager() = default;

    static QList<ThemeDefinition> discover(); // 多来源合并后的完整主题列表
    static bool loadDefinition(const QString& path, bool builtin, ThemeDefinition* out);
    static void applyDefinition(const ThemeDefinition& theme);
    static void applyPalette(const ThemeDefinition& theme);
    static QString renderStyleSheet(const ThemeDefinition& theme,
                                    const QHash<QString, QString>& metrics);

    static QString s_themeId;
    static ThemeDefinition s_theme;
    static ThemeTokens s_tokens;
    static QHash<QString, QString> s_metrics;
};

} // namespace editor
