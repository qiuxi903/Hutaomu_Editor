// Hutaomu Editor - Media viewer implementation (MFPlay).
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "MediaViewer.h"

#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <mfplay.h>
#include <windows.h>
#endif

namespace viewers {

namespace {
QString formatTime(qint64 ms)
{
    const qint64 s = ms / 1000;
    return QStringLiteral("%1:%2:%3")
        .arg(s / 3600, 2, 10, QLatin1Char('0'))
        .arg((s % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(s % 60, 2, 10, QLatin1Char('0'));
}
} // namespace

MediaViewer::MediaViewer(const QString& filePath, QWidget* parent)
    : DocumentViewer(parent)
    , m_filePath(filePath)
{
    setObjectName(QStringLiteral("mediaViewer"));
    m_isAudioOnly = viewerKindForFile(filePath) == ViewerKind::Audio;
}

MediaViewer::~MediaViewer()
{
#ifdef Q_OS_WIN
    stopPlayback();
#endif
}

void MediaViewer::showEvent(QShowEvent* event)
{
    DocumentViewer::showEvent(event);
#ifndef Q_OS_WIN
    // 非 Windows：Media Foundation 不可用。v1 显示明确占位（macOS/Linux
    // 的播放后端在 M3 里程碑引入 QtMultimedia 后启用），不崩溃。
    if (!m_uiBuilt) {
        m_uiBuilt = true;
        auto* layout = new QVBoxLayout(this);
        auto* hint = new QLabel(
            tr("此平台暂不支持应用内播放。\n文件：%1").arg(m_filePath), this);
        hint->setAlignment(Qt::AlignCenter);
        hint->setObjectName(QStringLiteral("mediaPlaceholder"));
        layout->addWidget(hint);
    }
#else
    if (!m_uiBuilt) {
        m_uiBuilt = true;

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        if (!m_isAudioOnly) {
            m_videoArea = new QWidget(this);
            m_videoArea->setObjectName(QStringLiteral("mediaVideoArea"));
            m_videoArea->setStyleSheet(
                QStringLiteral("background-color: #000000;"));
            layout->addWidget(m_videoArea, 1);
        } else {
            auto* art = new QLabel(tr("♪ 音频播放"), this);
            art->setAlignment(Qt::AlignCenter);
            art->setObjectName(QStringLiteral("mediaAudioArt"));
            layout->addWidget(art, 1);
        }

        auto* controls = new QWidget(this);
        auto* controlsLayout = new QHBoxLayout(controls);
        controlsLayout->setContentsMargins(10, 4, 10, 4);

        m_playButton = new QPushButton(tr("播放"), controls);
        m_playButton->setObjectName(QStringLiteral("mediaPlayButton"));
        connect(m_playButton, &QPushButton::clicked,
                this, &MediaViewer::togglePlay);
        controlsLayout->addWidget(m_playButton);

        m_timeLabel = new QLabel(QStringLiteral("0:00:00"), controls);
        m_timeLabel->setObjectName(QStringLiteral("mediaTimeLabel"));
        controlsLayout->addWidget(m_timeLabel);

        m_positionSlider = new QSlider(Qt::Horizontal, controls);
        m_positionSlider->setObjectName(QStringLiteral("mediaPositionSlider"));
        m_positionSlider->setRange(0, 1000);
        connect(m_positionSlider, &QSlider::sliderMoved, this,
                [this](int value) {
#ifdef Q_OS_WIN
                    if (m_player && m_durationUs > 0) {
                        PROPVARIANT pos;
                        PropVariantInit(&pos);
                        pos.vt = VT_I8;
                        pos.hVal.QuadPart = m_durationUs * value / 1000;
                        m_player->SetPosition(MFP_POSITIONTYPE_100NS, &pos);
                    }
#endif
                });
        controlsLayout->addWidget(m_positionSlider, 1);

        controlsLayout->addWidget(new QLabel(tr("音量"), controls));
        m_volumeSlider = new QSlider(Qt::Horizontal, controls);
        m_volumeSlider->setObjectName(QStringLiteral("mediaVolumeSlider"));
        m_volumeSlider->setRange(0, 100);
        m_volumeSlider->setValue(80);
        m_volumeSlider->setFixedWidth(100);
#ifdef Q_OS_WIN
        connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int v) {
            if (m_player)
                m_player->SetVolume(qreal(v) / 100.0);
        });
#endif
        controlsLayout->addWidget(m_volumeSlider);

        layout->addWidget(controls);

        startPlayback(); // 打开即播放（VS Code 预览体验）
    } else if (!m_startedOnce) {
        startPlayback();
    }
#endif
}

void MediaViewer::hideEvent(QHideEvent* event)
{
    DocumentViewer::hideEvent(event);
#ifdef Q_OS_WIN
    if (m_player && m_playing)
        m_player->Pause(); // 切走标签页自动暂停
#endif
}

#ifdef Q_OS_WIN
bool MediaViewer::startPlayback()
{
    if (m_player) {
        m_player->Play();
        m_playing = true;
        m_playButton->setText(tr("暂停"));
        return true;
    }
    HRESULT hr = MFPCreateMediaPlayer(
        reinterpret_cast<LPCWSTR>(m_filePath.utf16()),
        TRUE /* startPlayback */,
        MFP_OPTION_NONE,
        nullptr,
        reinterpret_cast<HWND>(winId()),
        &m_player);
    if (FAILED(hr) || !m_player) {
        if (m_playButton)
            m_playButton->setEnabled(false);
        return false;
    }
    m_playing = true;
    m_startedOnce = true;
    if (m_playButton)
        m_playButton->setText(tr("暂停"));

    // 查询时长
    IMFPMediaItem* item = nullptr;
    if (SUCCEEDED(m_player->CreateMediaItemFromURL(
            reinterpret_cast<LPCWSTR>(m_filePath.utf16()), FALSE, 0, &item))
        && item) {
        PROPVARIANT dur;
        PropVariantInit(&dur);
        if (SUCCEEDED(item->GetDuration(MFP_POSITIONTYPE_100NS, &dur))
            && dur.vt == VT_I8) {
            m_durationUs = dur.hVal.QuadPart / 10.0;
        }
        PropVariantClear(&dur);
        item->Release();
    }
    return true;
}

void MediaViewer::stopPlayback()
{
    if (m_player) {
        m_player->Stop();
        m_player->Shutdown();
        m_player->Release();
        m_player = nullptr;
        m_playing = false;
    }
}
#endif

void MediaViewer::togglePlay()
{
#ifdef Q_OS_WIN
    if (!m_player) {
        startPlayback();
        return;
    }
    if (m_playing) {
        m_player->Pause();
        m_playing = false;
        m_playButton->setText(tr("播放"));
    } else {
        m_player->Play();
        m_playing = true;
        m_playButton->setText(tr("暂停"));
    }
#endif
}

void MediaViewer::seekBy(int seconds)
{
#ifdef Q_OS_WIN
    if (!m_player || m_durationUs <= 0)
        return;
    PROPVARIANT pos;
    PropVariantInit(&pos);
    pos.vt = VT_I8;
    // 读取当前位置再偏移
    m_player->GetPosition(MFP_POSITIONTYPE_100NS, &pos);
    pos.hVal.QuadPart += qint64(seconds) * 10'000'000;
    m_player->SetPosition(MFP_POSITIONTYPE_100NS, &pos);
#else
    Q_UNUSED(seconds);
#endif
}

void MediaViewer::updatePositionLabel()
{
#ifdef Q_OS_WIN
    if (!m_player)
        return;
    PROPVARIANT pos;
    PropVariantInit(&pos);
    if (SUCCEEDED(m_player->GetPosition(MFP_POSITIONTYPE_100NS, &pos))
        && pos.vt == VT_I8) {
        const qint64 curMs = pos.hVal.QuadPart / 10'000;
        m_timeLabel->setText(formatTime(curMs));
        if (m_durationUs > 0)
            m_positionSlider->setValue(
                qRound(qreal(pos.hVal.QuadPart) / qreal(m_durationUs) * 1000.0));
    }
    PropVariantClear(&pos);
#endif
}

void MediaViewer::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        togglePlay();
        return;
    }
    if (event->key() == Qt::Key_Left) {
        seekBy(-5);
        return;
    }
    if (event->key() == Qt::Key_Right) {
        seekBy(5);
        return;
    }
    DocumentViewer::keyPressEvent(event);
}

#ifdef Q_OS_WIN
bool MediaViewer::nativeEvent(const QByteArray& eventType, void* message,
                              qintptr* result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_TIMER && m_player) {
        updatePositionLabel();
    }
    return false;
}
#endif

} // namespace viewers