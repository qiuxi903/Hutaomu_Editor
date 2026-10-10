// Hutaomu Editor - Beautiful message boxes (VS Code / macOS style).
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
//
// 统一的弹窗样式：大图标 + 粗体标题 + 正文 + 圆角按钮，替代原生 QMessageBox
// 的紧凑灰暗风格。所有确认/警告/信息弹窗都走这一套。
#pragma once

#include <QString>
#include <QWidget>

class QDialog;
class QPushButton;

namespace app::dialog {

// 按钮角色
enum class Button {
    Ok,          // 确定（主按钮，accent 色）
    Cancel,      // 取消（次要按钮）
    Save,        // 保存
    Discard,     // 不保存（destructive）
    Yes,
    No,
};

// 图标类型
enum class Icon {
    None,
    Question,    // 蓝色问号
    Warning,     // 橙色感叹号
    Error,       // 红色叉
    Success,     // 绿色对勾
    Info,        // 蓝色 i
};

// 显示一个美观的弹窗。返回被点击的按钮（nullptr = 关闭/取消）。
QPushButton* show(QWidget* parent,
                  const QString& title,
                  const QString& message,
                  const QString& informativeText = QString(),
                  Icon icon = Icon::Info,
                  const QList<QPair<QString, Button>>& buttons
                      = { { QStringLiteral("确定"), Button::Ok } });

// 常用快捷：确认弹窗（返回 true = 确认）
bool confirm(QWidget* parent, const QString& title, const QString& message,
             const QString& confirmText = QStringLiteral("确定"));

// 常用快捷：信息弹窗（只有一个确定按钮）
void info(QWidget* parent, const QString& title, const QString& message);

// 常用快捷：警告弹窗
void warning(QWidget* parent, const QString& title, const QString& message);

} // namespace app::dialog
