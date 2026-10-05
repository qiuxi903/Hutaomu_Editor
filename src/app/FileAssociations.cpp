// Hutaomu Editor - Per-user file associations implementation.
// SPDX-License-Identifier: LicenseRef-Proprietary
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
#ifdef Q_OS_WIN
    QSettings classes(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes"),
                      QSettings::NativeFormat);
    if (classes.status() != QSettings::NoError) {
        if (error)
            *error = QStringLiteral("无法写入注册表（HKCU\\Software\\Classes）。");
        return {};
    }

    writeProgId(classes, kProgIdMarkdown, QStringLiteral("Markdown 文档"));
    writeProgId(classes, kProgIdText, QStringLiteral("文本文档"));
    writeProgId(classes, kProgIdCode, QStringLiteral("源代码文件"));

    // 默认值 + OpenWithProgids：后者让"打开方式"里出现稳定条目，
    // 用户选一次"始终"即写入 UserChoice（这一步只有系统能做）
    QStringList written;
    const auto claim = [&](const QStringList& extensions, const QString& progId) {
        for (const QString& extension : extensions) {
            const QString key = QLatin1Char('.') + extension;
            classes.setValue(key, progId);
            classes.beginGroup(key);
            classes.setValue(QStringLiteral("OpenWithProgids/") + progId, QString());
            classes.endGroup();
            written.append(key);
        }
    };
    claim(markdownExtensions(), kProgIdMarkdown);
    claim(textExtensions(), kProgIdText);
    claim(codeExtensions(), kProgIdCode);
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
    caps.setValue(QStringLiteral("FileAssociations/.md"), kProgIdMarkdown);
    caps.setValue(QStringLiteral("FileAssociations/.txt"), kProgIdText);
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
