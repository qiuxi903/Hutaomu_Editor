// Hutaomu Editor - Viewer factory: create the right viewer for a file.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include "viewers/DocumentViewer.h"

namespace viewers {

// 按 viewerKindForFile 创建对应查看器；kind == None 返回 nullptr。
DocumentViewer* createViewer(const QString& filePath, QWidget* parent);

} // namespace viewers