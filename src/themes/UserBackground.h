// Hutaomu Editor - Per-user background customization for themes.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
//
// 用户可以给任何主题（内置或安装的）指定自定义编辑区背景：
// - 从本地选一张图片（复制到用户数据目录，避免源文件被删后丢失）
// - 选择模式（平铺/居中/拉伸）
// - 调透明度（叠在主题底色上）
// - 随时可移除，回到主题自身的背景
#pragma once

#include <QString>

namespace editor::background {

struct UserBackground {
    QString imagePath;     // 已复制到用户数据目录的图片绝对路径（空 = 无自定义）
    QString mode;          // tile / center / stretch（默认 tile）
    qreal opacity = 0.5;   // 0..1
    bool isValid() const { return !imagePath.isEmpty(); }
};

// 当前主题的自定义背景（无 → 主题自带背景生效）
UserBackground current();

// 保存/清除（清除后回到主题自带背景）
void save(const UserBackground& bg);
void clear();

// 把用户选的图片复制到用户数据目录（返回新路径），失败返回空串
QString importImage(const QString& sourcePath);

// 用户数据目录下的背景存储目录
QString storageDirectory();

} // namespace editor::background
