// Hutaomu Editor - Non-text document viewer base class.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#pragma once

#include <QString>
#include <QWidget>

namespace viewers {

// 非文本文档查看器基类：图片 / 视频 / PDF / Office 文档共用。
// 生命周期与 CodeEditor 一致——由 MainWindow 的标签页托管。
//   - filePath(): 唯一标识（重复打开检测用）
//   - isModified(): "可编辑"查看器有脏状态时返回 true（关闭前询问保存）
//   - save(): 把编辑结果写回原文件（不支持保存的查看器返回 false）
//   - zoomIn/zoomOut/resetZoom(): Ctrl+滚轮 / 菜单缩放
class DocumentViewer : public QWidget {
    Q_OBJECT
public:
    explicit DocumentViewer(QWidget* parent = nullptr);

    virtual QString filePath() const = 0;
    virtual bool isModified() const { return false; }
    virtual bool save() { return false; }
    virtual void zoomIn() {}
    virtual void zoomOut() {}
    virtual void resetZoom() {}
    virtual bool supportsZoom() const { return false; }

signals:
    void modifiedChanged(bool modified);
};

// 按扩展名判定文件是否由查看器接管（而非文本编辑器打开）
enum class ViewerKind { None, Image, Video, Audio, Pdf, Docx, Xlsx, Pptx };

ViewerKind viewerKindForFile(const QString& filePath);
QString viewerKindName(ViewerKind kind); // 状态栏显示用

} // namespace viewers