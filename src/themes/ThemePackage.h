// Hutaomu Editor - Theme package (.htmtpi) installation: ZIP unpacking with
// path-traversal protection, plus directory-form packages. Kept separate from
// ThemeManager so it can be unit-tested without a QApplication.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QString>

namespace editor::themepkg {

struct InstallOutcome {
    bool ok = false;
    QString themeId;
    bool overwritten = false;
    QString error; // 面向用户的失败原因
};

// 主题 id 是否为安全的目录名（字母/数字/-/_/.，且不含 ".."）。
bool isValidThemeId(const QString& id);

// 安装一个主题包到 targetRoot/<id>/。
// packagePath 可以是 .htmtpi / .zip 文件，也可以是包含 theme.json 的目录。
InstallOutcome install(const QString& packagePath, const QString& targetRoot);

// 删除 targetRoot/<id>/（仅限该目录下，id 会被校验）。
bool remove(const QString& targetRoot, const QString& themeId, QString* error);

// 读取 zip 内（或目录内）theme.json 的 id，不做安装（用于冲突提示）。
QString peekThemeId(const QString& packagePath, QString* error);

} // namespace editor::themepkg
