// Hutaomu Editor - Per-user file associations implementation.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "FileAssociations.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QSettings>
#include <QUrl>

namespace app::associations {

namespace {

const QString kProgIdMarkdown = QStringLiteral("HutaomuEditor.md");
const QString kProgIdText = QStringLiteral("HutaomuEditor.txt");
const QString kProgIdCode = QStringLiteral("HutaomuEditor.code");
const QString kAppExe = QStringLiteral("HutaomuEditor.exe");
const QString kPromptKey = QStringLiteral("fileAssoc/promptShown");

QString exePath()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

void writeProgId(QSettings& classes, const QString& progId, const QString& label)
{
    classes.beginGroup(progId);
    classes.setValue(QStringLiteral("."), label);
    classes.setValue(QStringLiteral("DefaultIcon/."), exePath() + QStringLiteral(",0"));
    classes.setValue(QStringLiteral("shell/open/command/."),
                     QStringLiteral("\"%1\" \"%2\"").arg(exePath(), QStringLiteral("%1")));
    classes.endGroup();
}

bool isOursProgId(const QString& progId)
{
    return progId.startsWith(QStringLiteral("HutaomuEditor"));
}

} // namespace

QStringList markdownExtensions()
{
    return { QStringLiteral("md"), QStringLiteral("markdown"), QStringLiteral("mkd") };
}

QStringList textExtensions()
{
    return { QStringLiteral("txt"), QStringLiteral("log") };
}

QStringList codeExtensions()
{
    return { QStringLiteral("c"),    QStringLiteral("h"),    QStringLiteral("cpp"),
             QStringLiteral("hpp"),  QStringLiteral("cc"),   QStringLiteral("py"),
             QStringLiteral("js"),   QStringLiteral("mjs"),  QStringLiteral("ts"),
             QStringLiteral("tsx"),  QStringLiteral("jsx"),  QStringLiteral("json"),
             QStringLiteral("html"), QStringLiteral("htm"),  QStringLiteral("css"),
             QStringLiteral("xml"),  QStringLiteral("yml"),  QStringLiteral("yaml"),
             QStringLiteral("toml"), QStringLiteral("ini"),  QStringLiteral("sql"),
             QStringLiteral("sh") };
}

QStringList allExtensions()
{
    QStringList all;
    for (const QString& extension : markdownExtensions() + textExtensions() + codeExtensions())
        all.append(QLatin1Char('.') + extension);
    return all;
}

QStringList writeUserAssociations(QString* error)
{
    return writeUserAssociations(QStringList(), error);
}

// groups ∈ {"markdown","text","code"}；空列表 = 全部。
// 用途：只夺回被别的软件抢走的某几类，不动其它扩展名。
QStringList writeUserAssociations(const QStringList& groups, QString* error)
{
#ifdef Q_OS_WIN
    struct Group {
        const char* name;
        QStringList extensions;
        QString progId;
        QString label;
        bool capabilities; // 是否登记到"默认应用"能力清单
    };
    const QList<Group> table = {
        { "markdown", markdownExtensions(), kProgIdMarkdown,
          QStringLiteral("Markdown 文档"), true },
        { "text", textExtensions(), kProgIdText, QStringLiteral("文本文档"), true },
        { "code", codeExtensions(), kProgIdCode, QStringLiteral("源代码文件"), false },
    };
    const bool all = groups.isEmpty();

    QSettings classes(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes"),
                      QSettings::NativeFormat);
    if (classes.status() != QSettings::NoError) {
        if (error)
            *error = QStringLiteral("无法写入注册表（HKCU\\Software\\Classes）。");
        return {};
    }

    // 默认值 + OpenWithProgids：后者让"打开方式"里出现稳定条目，
    // 用户选一次"始终"即写入 UserChoice（这一步只有系统能做）
    QStringList written;
    QStringList capabilityEntries;
    for (const Group& group : table) {
        if (!all && !groups.contains(QString::fromLatin1(group.name)))
            continue;
        writeProgId(classes, group.progId, group.label);
        for (const QString& extension : group.extensions) {
            const QString key = QLatin1Char('.') + extension;
            // 关联要写"子键的默认值"，QSettings 里写作 key/. —— 少了 /. 会写成
            // 一个名为 ".md" 的普通值（曾因此导致"夺回"实际无效，靠安装包写对）。
            // 先 remove(key) 顺手清掉历史误写的同名值。
            classes.remove(key);
            classes.setValue(key + QStringLiteral("/."), group.progId);
            classes.beginGroup(key);
            classes.setValue(QStringLiteral("OpenWithProgids/") + group.progId, QString());
            classes.endGroup();
            written.append(key);
            if (group.capabilities)
                capabilityEntries.append(key);
        }
    }
    classes.sync();

    // "打开方式"里作为正式应用条目出现
    QSettings apps(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\Applications\\")
                       + kAppExe,
                   QSettings::NativeFormat);
    apps.setValue(QStringLiteral("shell/open/command/."),
                  QStringLiteral("\"%1\" \"%2\"").arg(exePath(), QStringLiteral("%1")));
    apps.setValue(QStringLiteral("DefaultIcon/."), exePath() + QStringLiteral(",0"));
    for (const QString& extension : markdownExtensions() + textExtensions())
        apps.setValue(QStringLiteral("SupportedTypes/.") + extension, QString());
    apps.sync();

    // 能力注册（让"默认应用"页能列出我们）
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\RegisteredApplications"),
                  QSettings::NativeFormat);
    reg.setValue(QStringLiteral("HutaomuEditor"),
                 QStringLiteral("Software\\Hutaomu\\Hutaomu Editor\\Capabilities"));
    reg.sync();
    QSettings caps(QStringLiteral("HKEY_CURRENT_USER\\Software\\Hutaomu\\Hutaomu Editor\\"
                                  "Capabilities"),
                   QSettings::NativeFormat);
    caps.setValue(QStringLiteral("ApplicationName"), QStringLiteral("Hutaomu Editor"));
    caps.setValue(QStringLiteral("ApplicationDescription"),
                  QStringLiteral("原生跨平台的文本、代码与 Markdown 编辑器"));
    // 只登记本次选中的分组（避免"只夺回 md"时顺手把别的类型也写进能力清单）
    for (const QString& extension : capabilityEntries) {
        const bool isTextGroup = extension == QLatin1String(".txt")
                                 || extension == QLatin1String(".log");
        caps.setValue(QStringLiteral("FileAssociations/") + extension,
                      isTextGroup ? kProgIdText : kProgIdMarkdown);
    }
    caps.sync();

    if (error)
        error->clear();
    return written;
#else
    if (error)
        *error = QStringLiteral("文件关联目前只支持 Windows。");
    return {};
#endif
}

ExtensionStatus statusForExtension(const QString& extension)
{
    ExtensionStatus status;
    status.extension = extension.startsWith(QLatin1Char('.')) ? extension
                                                              : QLatin1Char('.') + extension;
#ifdef Q_OS_WIN
    QSettings choice(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                                    "CurrentVersion\\Explorer\\FileExts\\")
                         + status.extension + QStringLiteral("/UserChoice"),
                     QSettings::NativeFormat);
    const QString chosen = choice.value(QStringLiteral("ProgId")).toString();
    if (!chosen.isEmpty()) {
        status.progId = chosen;
        status.heldByUserChoice = true;
        status.isOurs = isOursProgId(chosen);
        return status;
    }

    for (const QString& root :
         { QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\"),
           QStringLiteral("HKEY_CLASSES_ROOT\\") }) {
        QSettings classes(root + status.extension, QSettings::NativeFormat);
        const QString progId = classes.value(QStringLiteral(".")).toString();
        if (!progId.isEmpty()) {
            status.progId = progId;
            status.isOurs = isOursProgId(progId);
            return status;
        }
    }
#endif
    return status;
}

QList<ExtensionStatus> statusForAll()
{
    QList<ExtensionStatus> statuses;
    for (const QString& extension : allExtensions())
        statuses.append(statusForExtension(extension));
    return statuses;
}

QList<ExtensionStatus> choicesNeedingUserAction()
{
    QList<ExtensionStatus> pending;
    for (const ExtensionStatus& status : statusForAll()) {
        if (status.heldByUserChoice && !status.isOurs)
            pending.append(status);
    }
    return pending;
}

bool openDefaultAppsSettings()
{
#ifdef Q_OS_WIN
    return QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:defaultapps")));
#else
    return false;
#endif
}

bool shouldPromptOnStartup()
{
    QSettings settings;
    return !settings.value(kPromptKey, false).toBool();
}

void markStartupPromptShown()
{
    QSettings settings;
    settings.setValue(kPromptKey, true);
    settings.sync();
}

} // namespace app::associations
