// Hutaomu Editor - Per-user file associations and default-app guidance (Windows).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
//
// 背景：Win8 起"默认程序"由 HKCU\...\FileExts\.ext\UserChoice 决定，且带系统哈希，
// 程序无法改写（能改的都是篡改，不做）。我们只能：
//   1) 写传统关联（HKCU\Software\Classes）+ OpenWithProgids + 能力注册；
//   2) 如实报告哪些扩展名被 UserChoice 占着；
//   3) 引导用户到系统「默认应用」页点一次。
// 在没有 UserChoice 的扩展名上，第 1 步就直接生效。
#pragma once

#include <QString>
#include <QStringList>

namespace app::associations {

QStringList markdownExtensions();   // md markdown mkd
QStringList textExtensions();       // txt log
QStringList codeExtensions();       // c/cpp/py/js/… 与安装包一致
QStringList allExtensions();        // 三类合并（带点）

struct ExtensionStatus {
    QString extension;             // ".md"
    QString progId;                // 当前生效的 ProgId（可能来自 UserChoice）
    bool isOurs = false;           // 是否已归 Hutaomu Editor
    bool heldByUserChoice = false; // 被系统"默认应用"锁定，程序改不了
};

// 以当前用户身份写入关联（免管理员，全用户安装也能用）。
// 返回写入的扩展名列表；error 非空表示失败。
QStringList writeUserAssociations(QString* error);

// 只写指定分组（"markdown" / "text" / "code"）；传空列表等价于全部。
// 用途：只夺回被别的软件抢走的某几类，不动其它扩展名。
QStringList writeUserAssociations(const QStringList& groups, QString* error);

ExtensionStatus statusForExtension(const QString& extension);
QList<ExtensionStatus> statusForAll();
// 需要用户去系统设置里点一次的那些（被别的程序占着的）
QList<ExtensionStatus> choicesNeedingUserAction();

// 打开 Windows「默认应用」设置页（ms-settings:defaultapps）
bool openDefaultAppsSettings();

// 首次启动引导：仅提示一次（QSettings 里记 fileAssoc/promptShown）。
bool shouldPromptOnStartup();
void markStartupPromptShown();

} // namespace app::associations
