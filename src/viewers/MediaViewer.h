// Hutaomu Editor - Video/audio viewer (Windows Media Foundation, MFPlay).
// SPDX-License-Identifier: LicenseRef-Proprietary
#pragma once

#include <QString>
#include <QWidget>

#include "viewers/DocumentViewer.h"

struct IMFPMediaPlayer;
struct IMFPMediaItem;

class QPushButton;
class QSlider;
class QLabel;

namespace viewers {

// 视频/音频查看器：Media Foundation 原生播放（Windows 内建编解码，
// MP4/H.264、MKV 部分、MP3/WAV/AAC 等）。播放/暂停/进度条/音量。
// 仅 Windows 可用；其它平台显示占位提示。
class MediaViewer : public DocumentViewer {
    Q_OBJECT
public:
    explicit MediaViewer(const QString& filePath, QWidget* parent = nullptr);
    ~MediaViewer() override;

    QString filePath() const override { return m_filePath; }

protected:
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message,
                     qintptr* result) override;
#endif
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void togglePlay();
    void seekBy(int seconds);
    void updatePositionLabel();

private:
#ifdef Q_OS_WIN
    bool startPlayback();
    void stopPlayback();
#endif

    QString m_filePath;
    bool m_isAudioOnly = false;
#ifdef Q_OS_WIN
    IMFPMediaPlayer* m_player = nullptr;
    bool m_playing = false;
    qint64 m_durationUs = 0;
#endif
    QPushButton* m_playButton = nullptr;
    QSlider* m_positionSlider = nullptr;
    QSlider* m_volumeSlider = nullptr;
    QLabel* m_timeLabel = nullptr;
    QWidget* m_videoArea = nullptr;
    bool m_uiBuilt = false;
    bool m_startedOnce = false;
};

} // namespace viewers