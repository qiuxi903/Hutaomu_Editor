// Hutaomu Editor - Theme definitions and loading (JSON color palettes +
// T2 metrics rendered into a parameterized QSS template).
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "ThemeManager.h"

#include "ThemePackage.h"

#include "core/ZipWriter.h"
#include "plugins/PluginManager.h"

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPalette>
#include <QStandardPaths>
#include <QStyleFactory>

#include <algorithm>

namespace editor {

namespace {

constexpr const char* kDefaultThemeId = "paper";
constexpr const char* kTemplatePath = ":/themes/template.qss";
constexpr const char* kThemeFileName = "theme.json";

QString builtinThemeDirectory() { return QStringLiteral(":/themes"); }

// T2 度量默认值（comfortable 密度）。主题只需覆盖想改的项。
QHash<QString, QString> defaultMetrics()
{
    return {
        { QStringLiteral("fontSizeSmall"), QStringLiteral("12") },
        { QStringLiteral("fontSizeNormal"), QStringLiteral("13") },
        { QStringLiteral("menuBarItemPaddingV"), QStringLiteral("9") },
        { QStringLiteral("menuBarItemPaddingH"), QStringLiteral("10") },
        { QStringLiteral("cornerRadius"), QStringLiteral("4") },
        { QStringLiteral("cornerRadiusLarge"), QStringLiteral("6") },
        { QStringLiteral("sidebarButtonPadding"), QStringLiteral("3") },
        { QStringLiteral("welcomeButtonPaddingV"), QStringLiteral("7") },
        { QStringLiteral("welcomeButtonPaddingH"), QStringLiteral("24") },
        { QStringLiteral("treeRowHeight"), QStringLiteral("24") },
        { QStringLiteral("inputPaddingV"), QStringLiteral("5") },
        { QStringLiteral("inputPaddingH"), QStringLiteral("8") },
        { QStringLiteral("compactInputPaddingV"), QStringLiteral("3") },
        { QStringLiteral("compactInputPaddingH"), QStringLiteral("6") },
        { QStringLiteral("tabPaddingV"), QStringLiteral("6") },
        { QStringLiteral("tabPaddingH"), QStringLiteral("10") },
        { QStringLiteral("tabHeight"), QStringLiteral("34") },
        { QStringLiteral("scrollbarWidth"), QStringLiteral("12") },
        { QStringLiteral("statusBarPaddingH"), QStringLiteral("8") },
        { QStringLiteral("menuItemPaddingV"), QStringLiteral("5") },
        { QStringLiteral("menuItemPaddingLeft"), QStringLiteral("24") },
        { QStringLiteral("menuItemPaddingRight"), QStringLiteral("28") },
        { QStringLiteral("tooltipPadding"), QStringLiteral("4") },
        { QStringLiteral("buttonPaddingV"), QStringLiteral("6") },
        { QStringLiteral("buttonPaddingH"), QStringLiteral("18") },
        // 编辑器（由 C++ 读取；0/空 = 不覆盖用户设置）
        { QStringLiteral("editorFontFamily"), QString() },
        { QStringLiteral("editorFontSize"), QStringLiteral("0") },
    };
}

// uiDensity 预设：先套用整组内边距，再让主题的显式 metrics 覆盖。
QHash<QString, QHash<QString, QString>> densityPresets()
{
    const QHash<QString, QString> compact {
        { QStringLiteral("menuBarItemPaddingV"), QStringLiteral("6") },
        { QStringLiteral("menuBarItemPaddingH"), QStringLiteral("8") },
        { QStringLiteral("sidebarButtonPadding"), QStringLiteral("2") },
        { QStringLiteral("treeRowHeight"), QStringLiteral("20") },
        { QStringLiteral("tabPaddingV"), QStringLiteral("4") },
        { QStringLiteral("menuItemPaddingV"), QStringLiteral("3") },
        { QStringLiteral("statusBarPaddingH"), QStringLiteral("6") },
        { QStringLiteral("welcomeButtonPaddingV"), QStringLiteral("5") },
        { QStringLiteral("buttonPaddingV"), QStringLiteral("4") },
        { QStringLiteral("tooltipPadding"), QStringLiteral("3") },
    };
    const QHash<QString, QString> spacious {
        { QStringLiteral("menuBarItemPaddingV"), QStringLiteral("10") },
        { QStringLiteral("menuBarItemPaddingH"), QStringLiteral("12") },
        { QStringLiteral("sidebarButtonPadding"), QStringLiteral("4") },
        { QStringLiteral("treeRowHeight"), QStringLiteral("30") },
        { QStringLiteral("tabPaddingV"), QStringLiteral("9") },
        { QStringLiteral("tabPaddingH"), QStringLiteral("12") },
        { QStringLiteral("menuItemPaddingV"), QStringLiteral("7") },
        { QStringLiteral("statusBarPaddingH"), QStringLiteral("10") },
        { QStringLiteral("inputPaddingV"), QStringLiteral("7") },
        { QStringLiteral("inputPaddingH"), QStringLiteral("10") },
        { QStringLiteral("buttonPaddingV"), QStringLiteral("8") },
        { QStringLiteral("buttonPaddingH"), QStringLiteral("22") },
        { QStringLiteral("welcomeButtonPaddingV"), QStringLiteral("9") },
    };
    return {
        { QStringLiteral("compact"), compact },
        { QStringLiteral("spacious"), spacious },
    };
}

// 由插件的主题根目录反查插件 id（"<plugins>/<id>/themes" → "<id>"）。
QString pluginIdForThemeRoot(const QString& themeRoot)
{
    QString path = themeRoot;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const int themesAt = path.lastIndexOf(QStringLiteral("/themes"));
    if (themesAt < 0)
        return QString();
    const QString pluginRoot = path.left(themesAt);
    const int slash = pluginRoot.lastIndexOf(QLatin1Char('/'));
    return slash >= 0 ? pluginRoot.mid(slash + 1) : pluginRoot;
}

QHash<QString, QString> mergedMetrics(const ThemeDefinition& theme)
{
    QHash<QString, QString> merged = defaultMetrics();
    const QString density = theme.metrics.value(QStringLiteral("uiDensity"),
                                                QStringLiteral("comfortable"));
    const auto presets = densityPresets();
    if (const auto it = presets.constFind(density); it != presets.constEnd()) {
        for (auto p = it->constBegin(); p != it->constEnd(); ++p)
            merged.insert(p.key(), p.value());
    }
    for (auto it = theme.metrics.constBegin(); it != theme.metrics.constEnd(); ++it)
        merged.insert(it.key(), it.value());
    // uiDensity 本身不进 QSS
    merged.remove(QStringLiteral("uiDensity"));
    return merged;
}

// 主题包内资源路径：必须是包内相对路径（拒绝绝对路径、盘符、.. 越界）。
bool isSafeAssetPath(const QString& path)
{
    if (path.isEmpty())
        return false;
    QString normalized = path;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalized.startsWith(QLatin1Char('/')) || normalized.contains(QLatin1Char(':')))
        return false;
    const QStringList parts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        if (part == QLatin1String(".."))
            return false;
    }
    return true;
}

QString metricValueToString(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (value.isBool())
        return value.toBool() ? QStringLiteral("1") : QStringLiteral("0");
    if (value.isDouble()) {
        const double d = value.toDouble();
        if (qFuzzyCompare(d + 1.0, double(qRound(d)) + 1.0))
            return QString::number(qRound(d));
        return QString::number(d, 'g', 6);
    }
    return QString();
}

void appendInferredCapabilities(ThemeDefinition* out)
{
    if (!out->capabilities.contains(QStringLiteral("colors")))
        out->capabilities.append(QStringLiteral("colors"));
    if (!out->metrics.isEmpty() && !out->capabilities.contains(QStringLiteral("metrics")))
        out->capabilities.append(QStringLiteral("metrics"));
    if (out->background.isValid() && !out->capabilities.contains(QStringLiteral("assets")))
        out->capabilities.append(QStringLiteral("assets"));
    if (out->rootPath.startsWith(QLatin1String(":/")))
        return; // 资源内不做目录探测
    if (!out->layout.isDefault()
        && !out->capabilities.contains(QStringLiteral("layout")))
        out->capabilities.append(QStringLiteral("layout"));
    if (QFileInfo::exists(out->rootPath + QStringLiteral("/assets"))
        && !out->capabilities.contains(QStringLiteral("assets")))
        out->capabilities.append(QStringLiteral("assets"));
}

} // namespace

QString ThemeManager::s_themeId;
ThemeDefinition ThemeManager::s_theme;
ThemeTokens ThemeManager::s_tokens;
QHash<QString, QString> ThemeManager::s_metrics;

bool ThemeManager::loadDefinition(const QString& path, bool builtin, ThemeDefinition* out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    out->id = root.value(QStringLiteral("id")).toString();
    out->name = root.value(QStringLiteral("name")).toString();
    out->description = root.value(QStringLiteral("description")).toString();
    out->dark = root.value(QStringLiteral("dark")).toBool();
    out->builtin = builtin;
    if (out->id.isEmpty() || !themepkg::isValidThemeId(out->id))
        return false;

    const QJsonObject colors = root.value(QStringLiteral("colors")).toObject();
    for (auto it = colors.begin(); it != colors.end(); ++it) {
        const QColor color(it.value().toString());
        if (color.isValid())
            out->colors.insert(it.key(), color);
    }

    const QJsonObject metrics = root.value(QStringLiteral("metrics")).toObject();
    for (auto it = metrics.begin(); it != metrics.end(); ++it) {
        const QString value = metricValueToString(it.value());
        if (!value.isNull())
            out->metrics.insert(it.key(), value);
    }

    // T3：编辑区背景图（图片路径相对主题根目录）
    const QJsonObject background = root.value(QStringLiteral("background")).toObject();
    if (!background.isEmpty()) {
        out->background.image = background.value(QStringLiteral("image")).toString();
        const QString mode = background.value(QStringLiteral("mode")).toString();
        if (!mode.isEmpty())
            out->background.mode = mode;
        if (background.contains(QStringLiteral("opacity"))) {
            const qreal opacity = background.value(QStringLiteral("opacity")).toDouble(1.0);
            out->background.opacity = qBound(qreal(0.0), opacity, qreal(1.0));
        }
        if (!isSafeAssetPath(out->background.image))
            out->background.image.clear();
    }

    const QJsonArray capabilities = root.value(QStringLiteral("capabilities")).toArray();
    for (const QJsonValue& value : capabilities) {
        const QString capability = value.toString();
        if (!capability.isEmpty() && !out->capabilities.contains(capability))
            out->capabilities.append(capability);
    }
    return true;
}

// 读取主题包内的 layout.json（T4）。文件不存在 = 默认形态（主题不管布局）。
bool loadLayout(const QString& themeRoot, ThemeLayout* out)
{
    const QString path = themeRoot + QStringLiteral("/layout.json");
    if (!QFileInfo::exists(path))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.isEmpty())
        return false;

    const auto readVisible = [](const QJsonObject& object, const char* key)
        -> std::optional<bool> {
        if (!object.contains(QLatin1String(key)))
            return std::nullopt;
        return object.value(QLatin1String(key)).toBool();
    };
    const auto objectOf = [&root](const char* key) {
        return root.value(QLatin1String(key)).toObject();
    };

    const QJsonObject sidebar = objectOf("sidebar");
    const QJsonObject activityBar = objectOf("activityBar");
    const QJsonObject statusBar = objectOf("statusBar");
    const QJsonObject outlinePanel = objectOf("outlinePanel");
    const QJsonObject previewPane = objectOf("previewPane");
    const QJsonObject tabs = objectOf("tabs");
    const QJsonObject editor = objectOf("editor");

    // immersive 是预设：先套用，再让显式键覆盖
    if (root.value(QStringLiteral("immersive")).toBool()) {
        out->immersive = true;
        out->activityBarVisible = false;
        out->statusBarVisible = false;
        out->sidebarVisible = false;
        out->outlineVisible = false;
        out->previewVisible = false;
        out->centered = true;
    }

    if (const auto v = readVisible(sidebar, "visible"))
        out->sidebarVisible = v;
    if (const auto v = readVisible(activityBar, "visible"))
        out->activityBarVisible = v;
    if (const auto v = readVisible(statusBar, "visible"))
        out->statusBarVisible = v;
    if (const auto v = readVisible(outlinePanel, "visible"))
        out->outlineVisible = v;
    if (const auto v = readVisible(previewPane, "visible"))
        out->previewVisible = v;
    if (const auto v = readVisible(editor, "centered"))
        out->centered = v;
    if (const auto v = readVisible(editor, "headingUnderline"))
        out->headingUnderline = v;

    const QString sidebarStyle = sidebar.value(QStringLiteral("style")).toString();
    if (sidebarStyle == QLatin1String("plain") || sidebarStyle == QLatin1String("card"))
        out->sidebarStyle = sidebarStyle;
    const QString tabsStyle = tabs.value(QStringLiteral("style")).toString();
    if (tabsStyle == QLatin1String("tab") || tabsStyle == QLatin1String("underline")
        || tabsStyle == QLatin1String("pill") || tabsStyle == QLatin1String("hidden"))
        out->tabsStyle = tabsStyle;

    if (editor.contains(QStringLiteral("maxWidth"))) {
        const int maxWidth = editor.value(QStringLiteral("maxWidth")).toInt();
        if (maxWidth > 0)
            out->maxWidth = maxWidth;
    }
    if (out->immersive && !out->maxWidth)
        out->maxWidth = 760; // 沉浸写作的合理默认栏宽
    return true;
}

QString ThemeLayout::summary() const
{
    QStringList parts;
    if (activityBarVisible && !*activityBarVisible)
        parts.append(QObject::tr("隐藏活动栏"));
    if (statusBarVisible && !*statusBarVisible)
        parts.append(QObject::tr("隐藏状态栏"));
    if (sidebarVisible && !*sidebarVisible)
        parts.append(QObject::tr("隐藏侧栏（文件树/搜索）"));
    if (outlineVisible && !*outlineVisible)
        parts.append(QObject::tr("隐藏大纲"));
    if (previewVisible && !*previewVisible)
        parts.append(QObject::tr("隐藏预览"));
    if (sidebarStyle == QLatin1String("card"))
        parts.append(QObject::tr("侧栏卡片化"));
    if (tabsStyle == QLatin1String("underline"))
        parts.append(QObject::tr("标签下划线风"));
    else if (tabsStyle == QLatin1String("pill"))
        parts.append(QObject::tr("标签胶囊风"));
    else if (tabsStyle == QLatin1String("hidden"))
        parts.append(QObject::tr("隐藏标签栏"));
    if (centered && *centered)
        parts.append(maxWidth ? QObject::tr("编辑区居中限宽 %1px").arg(*maxWidth)
                              : QObject::tr("编辑区居中"));
    if (headingUnderline && *headingUnderline)
        parts.append(QObject::tr("标题下划线"));
    if (immersive)
        parts.append(QObject::tr("专注写作模式"));
    return parts.join(QStringLiteral("、"));
}

// 多来源合并：内置资源 → 用户主题目录（同名 id 后者胜出）。
QList<ThemeDefinition> ThemeManager::discover()
{
    QHash<QString, ThemeDefinition> byId;
    QStringList order; // 首次出现顺序，最终会重新排序

    const auto addFlat = [&](const QString& root, bool builtin,
                            const QString& sourcePluginId = QString()) {
        const QDir dir(root);
        for (const QString& entry : dir.entryList(QStringList { QStringLiteral("*.json") },
                                                  QDir::Files, QDir::Name)) {
            ThemeDefinition definition;
            if (!loadDefinition(root + QLatin1Char('/') + entry, builtin, &definition))
                continue;
            const QString preview = root + QLatin1Char('/') + definition.id
                                    + QStringLiteral(".png");
            if (QFileInfo::exists(preview))
                definition.previewPath = preview;
            definition.rootPath = root;
            definition.sourcePluginId = sourcePluginId;
            appendInferredCapabilities(&definition);
            if (!byId.contains(definition.id))
                order.append(definition.id);
            byId.insert(definition.id, definition);
        }
    };

    const auto addDirectories = [&](const QString& root, bool builtin,
                                   const QString& sourcePluginId = QString()) {
        const QDir dir(root);
        if (!dir.exists())
            return;
        for (const QString& entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                  QDir::Name)) {
            const QString themeJson = root + QLatin1Char('/') + entry
                                      + QLatin1Char('/') + QLatin1String(kThemeFileName);
            if (!QFileInfo::exists(themeJson))
                continue;
            ThemeDefinition definition;
            if (!loadDefinition(themeJson, builtin, &definition))
                continue;
            definition.rootPath = root + QLatin1Char('/') + entry;
            definition.sourcePluginId = sourcePluginId;
            const QString preview = definition.rootPath + QStringLiteral("/preview.png");
            if (QFileInfo::exists(preview))
                definition.previewPath = preview;
            loadLayout(definition.rootPath, &definition.layout);
            appendInferredCapabilities(&definition);
            if (!byId.contains(definition.id))
                order.append(definition.id);
            byId.insert(definition.id, definition);
        }
    };

    addFlat(builtinThemeDirectory(), true);
    addDirectories(builtinThemeDirectory(), true);

    const QString userRoot = ThemeManager::userThemeDirectory();
    addDirectories(userRoot, false);
    addFlat(userRoot, false);

    // 插件提供的主题（<plugin>/themes/…）：标记来源插件，随插件启停出现/消失，
    // 不能单独删除（见 uninstallTheme）。
    for (const QString& pluginRoot : plugins::PluginManager::instance().themeRoots()) {
        const QString pluginId = pluginIdForThemeRoot(pluginRoot);
        addDirectories(pluginRoot, false, pluginId);
        addFlat(pluginRoot, false, pluginId);
    }

    QList<ThemeDefinition> themes;
    themes.reserve(order.size());
    for (const QString& id : order)
        themes.append(byId.value(id));
    std::sort(themes.begin(), themes.end(),
              [](const ThemeDefinition& a, const ThemeDefinition& b) {
                  if (a.id == QLatin1String(kDefaultThemeId))
                      return true;
                  if (b.id == QLatin1String(kDefaultThemeId))
                      return false;
                  if (a.builtin != b.builtin)
                      return a.builtin;
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
    return themes;
}

QList<ThemeDefinition> ThemeManager::availableThemes()
{
    return discover();
}

ThemeDefinition ThemeManager::themeDefinition(const QString& themeId)
{
    for (const ThemeDefinition& definition : discover()) {
        if (definition.id == themeId)
            return definition;
    }
    return ThemeDefinition {};
}

const ThemeLayout& ThemeManager::currentLayout()
{
    return s_theme.layout;
}

ThemeDefinition ThemeManager::currentTheme()
{
    return s_theme;
}

QString ThemeManager::assetPath(const QString& relativePath, const QString& themeId)
{
    if (!isSafeAssetPath(relativePath))
        return QString();

    ThemeDefinition definition;
    if (themeId.isEmpty())
        definition = s_theme;
    else
        definition = themeDefinition(themeId);
    if (definition.rootPath.isEmpty())
        return QString();

    const QString path = definition.rootPath + QLatin1Char('/') + relativePath;
    return QFileInfo::exists(path) ? path : QString();
}

void ThemeManager::applyTheme(const QString& themeId)
{
    ThemeDefinition definition = themeDefinition(themeId);
    if (definition.id.isEmpty()) {
        if (themeId != QLatin1String(kDefaultThemeId))
            applyTheme(QLatin1String(kDefaultThemeId));
        return;
    }
    applyDefinition(definition);
}

void ThemeManager::applyDefinition(const ThemeDefinition& theme)
{
    s_theme = theme;
    s_themeId = theme.id;
    s_metrics = mergedMetrics(theme);

    const auto set = [&](QColor& slot, const char* key) {
        slot = theme.color(QLatin1String(key));
    };
    set(s_tokens.accent, "accent");
    set(s_tokens.titleBarBackground, "titleBarBg");
    set(s_tokens.editorBackground, "editorBg");
    set(s_tokens.editorForeground, "editorFg");
    set(s_tokens.currentLineBackground, "currentLineBg");
    set(s_tokens.selectionBackground, "selectionBg");
    set(s_tokens.lineNumberBackground, "editorBg");
    set(s_tokens.lineNumberForeground, "lineNumberFg");
    set(s_tokens.lineNumberActiveForeground, "lineNumberActiveFg");
    set(s_tokens.activityBarBackground, "activityBarBg");
    set(s_tokens.sidebarBackground, "sidebarBg");
    set(s_tokens.searchHighlightBackground, "searchHighlight");

    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    applyPalette(theme);
    qApp->setStyleSheet(renderStyleSheet(theme, s_metrics));
}

QString ThemeManager::renderStyleSheet(const ThemeDefinition& theme,
                                       const QHash<QString, QString>& metrics)
{
    QFile templateFile(QString::fromLatin1(kTemplatePath));
    if (!templateFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QString css = QString::fromUtf8(templateFile.readAll());

    // 度量先替换（@m.<key>），再替换颜色（@<key>）；两者前缀不同，互不干扰。
    // 长键优先，避免 "fontSizeSmall" 截断 "fontSizeSmallX" 这类前缀冲突。
    QStringList metricKeys = metrics.keys();
    std::sort(metricKeys.begin(), metricKeys.end(),
              [](const QString& a, const QString& b) { return a.size() > b.size(); });
    for (const QString& key : metricKeys) {
        const QString value = metrics.value(key);
        if (value.isEmpty())
            continue;
        css.replace(QStringLiteral("@m.%1").arg(key), value);
    }
    // 颜色同样长键优先：否则 "@accent" 会把 "@accentHover" 破坏成 "#1f8a6dHover"
    // （QHash 顺序随机，曾导致偶发的颜色解析失败）。
    QStringList colorKeys = theme.colors.keys();
    std::sort(colorKeys.begin(), colorKeys.end(),
              [](const QString& a, const QString& b) { return a.size() > b.size(); });
    for (const QString& key : colorKeys)
        css.replace(QStringLiteral("@%1").arg(key), theme.colors.value(key).name());
    return css;
}

QString ThemeManager::renderedStyleSheet(const QString& themeId)
{
    ThemeDefinition definition = themeDefinition(themeId);
    if (definition.id.isEmpty())
        definition = themeDefinition(QLatin1String(kDefaultThemeId));
    return renderStyleSheet(definition, mergedMetrics(definition));
}

void ThemeManager::applyPalette(const ThemeDefinition& theme)
{
    QPalette pal;
    pal.setColor(QPalette::Window, theme.color("windowBg"));
    pal.setColor(QPalette::WindowText, theme.color("titleBarText"));
    pal.setColor(QPalette::Base, theme.color("editorBg"));
    pal.setColor(QPalette::AlternateBase, theme.color("sidebarBg"));
    pal.setColor(QPalette::ToolTipBase, theme.color("tooltipBg"));
    pal.setColor(QPalette::ToolTipText, theme.color("tooltipFg"));
    pal.setColor(QPalette::Text, theme.color("editorFg"));
    pal.setColor(QPalette::Button, theme.color("buttonBg"));
    pal.setColor(QPalette::ButtonText, theme.color("buttonFg"));
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Link, theme.color("accent"));
    pal.setColor(QPalette::Highlight, theme.color("selectionBg"));
    pal.setColor(QPalette::HighlightedText, theme.color("editorFg"));
    const QColor disabled = theme.color("menuDisabledFg");
    pal.setColor(QPalette::Disabled, QPalette::Text, disabled);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    QApplication::setPalette(pal);
}

QString ThemeManager::theme()
{
    return s_themeId;
}

bool ThemeManager::currentIsDark()
{
    return s_theme.dark;
}

const ThemeTokens& ThemeManager::tokens()
{
    return s_tokens;
}

QString ThemeManager::metric(const QString& key)
{
    if (s_metrics.isEmpty())
        s_metrics = defaultMetrics();
    return s_metrics.value(key);
}

int ThemeManager::metricInt(const QString& key, int fallback)
{
    bool ok = false;
    const int value = metric(key).toInt(&ok);
    return ok ? value : fallback;
}

qreal ThemeManager::metricReal(const QString& key, qreal fallback)
{
    bool ok = false;
    const qreal value = metric(key).toDouble(&ok);
    return ok ? value : fallback;
}

QColor ThemeManager::syntaxColor(const QString& role)
{
    return s_theme.color(QStringLiteral("syntax.") + role);
}

QColor ThemeManager::color(const QString& key)
{
    return s_theme.color(key);
}

QString ThemeManager::userThemeDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + QStringLiteral("/themes");
}

bool ThemeManager::installThemePackage(const QString& packagePath, QString* themeId,
                                       QString* error)
{
    const themepkg::InstallOutcome outcome
        = themepkg::install(packagePath, userThemeDirectory());
    if (themeId)
        *themeId = outcome.themeId;
    if (!outcome.ok) {
        if (error)
            *error = outcome.error;
        return false;
    }
    // 安装产物必须能被解析，否则退回（避免出现"装了但列不出来"的主题）。
    const ThemeDefinition definition = themeDefinition(outcome.themeId);
    if (definition.id.isEmpty()) {
        QString ignored;
        themepkg::remove(userThemeDirectory(), outcome.themeId, &ignored);
        if (error)
            *error = QObject::tr("主题包内容无效：theme.json 缺少合法的 id。");
        return false;
    }
    if (error)
        error->clear();
    return true;
}

// 导出：把主题根目录下的所有文件打成 <id>/... 的 ZIP（扁平 JSON 转为 theme.json）。
bool ThemeManager::exportThemePackage(const QString& themeId, const QString& targetFile,
                                      QString* error)
{
    const ThemeDefinition definition = themeDefinition(themeId);
    if (definition.id.isEmpty()) {
        if (error)
            *error = QObject::tr("未找到主题：%1").arg(themeId);
        return false;
    }

    // 收集 (归档路径, 源路径)
    QList<QPair<QString, QString>> files;
    const bool directoryForm = definition.rootPath.endsWith(QLatin1Char('/') + definition.id);
    if (directoryForm) {
        QDirIterator it(definition.rootPath,
                        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QFileInfo info = it.fileInfo();
            const QString relative = QDir(definition.rootPath).relativeFilePath(path);
            if (info.isDir())
                continue;
            files.append({ definition.id + QLatin1Char('/') + relative, path });
        }
    } else {
        // 扁平主题：<root>/<id>.json -> <id>/theme.json
        const QString json = definition.rootPath + QLatin1Char('/') + definition.id
                             + QStringLiteral(".json");
        files.append({ definition.id + QStringLiteral("/theme.json"), json });
        const QString preview = definition.rootPath + QLatin1Char('/') + definition.id
                                 + QStringLiteral(".png");
        if (QFileInfo::exists(preview))
            files.append({ definition.id + QStringLiteral("/preview.png"), preview });
    }

    if (files.isEmpty()) {
        if (error)
            *error = QObject::tr("主题没有可导出的文件。");
        return false;
    }

    core::ZipWriter writer(targetFile);
    for (const auto& file : files) {
        QFile source(file.second);
        if (!source.open(QIODevice::ReadOnly)) {
            if (error)
                *error = QObject::tr("无法读取主题文件：%1").arg(file.second);
            return false;
        }
        if (!writer.addFile(file.first, source.readAll())) {
            if (error)
                *error = writer.errorString();
            return false;
        }
    }
    if (!writer.close()) {
        if (error)
            *error = writer.errorString();
        return false;
    }
    if (error)
        error->clear();
    return true;
}

bool ThemeManager::uninstallTheme(const QString& themeId, QString* error)
{
    if (!themepkg::isValidThemeId(themeId)) {
        if (error)
            *error = QObject::tr("主题 id 不合法。");
        return false;
    }
    for (const ThemeDefinition& definition : discover()) {
        if (definition.id != themeId)
            continue;
        if (definition.builtin || !definition.sourcePluginId.isEmpty()) {
            if (error)
                *error = !definition.sourcePluginId.isEmpty()
                             ? QObject::tr("该主题由插件「%1」提供，请通过禁用/卸载插件移除。")
                                   .arg(definition.sourcePluginId)
                             : QObject::tr("内置主题不能删除。");
            return false;
        }
        return themepkg::remove(userThemeDirectory(), themeId, error);
    }
    if (error)
        *error = QObject::tr("未找到该主题。");
    return false;
}

} // namespace editor
