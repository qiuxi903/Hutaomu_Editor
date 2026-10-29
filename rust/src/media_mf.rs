// Hutaomu Editor (Rust) - Media Foundation 内嵌视频播放器。
// 对标 Qt 版 viewers/ 的 MF 集成：IMFSourceReader 解码（视频 RGB32 + 音频 PCM16 重采样），
// WASAPI 共享模式输出；worker 线程解码，UI 经通道收帧。
// SPDX-License-Identifier: AGPL-3.0-only
#![allow(clippy::too_many_arguments)]

use std::sync::mpsc;
use std::time::Instant;

use windows::core::HSTRING;
use windows::Win32::Media::Audio::{
    IAudioClient, IAudioRenderClient, IMMDeviceEnumerator, MMDeviceEnumerator,
    AUDCLNT_SHAREMODE_SHARED, WAVEFORMATEX, eConsole, eRender,
};
use windows::Win32::Media::MediaFoundation::{
    MFCreateAttributes, MFCreateMediaType, MFCreateMFByteStreamOnStream,
    MFCreateSourceReaderFromByteStream, MFAudioFormat_PCM,
    MFMediaType_Audio, MFMediaType_Video, MFVideoFormat_RGB32, MFStartup,
    MF_MT_AUDIO_BITS_PER_SAMPLE, MF_MT_AUDIO_NUM_CHANNELS, MF_MT_AUDIO_SAMPLES_PER_SECOND,
    MF_MT_FRAME_SIZE, MF_MT_MAJOR_TYPE, MF_MT_SUBTYPE, MF_PD_DURATION,
    MF_SOURCE_READER_DISABLE_DXVA, MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING,
    MF_SOURCE_READER_FIRST_AUDIO_STREAM,
    MF_SOURCE_READER_FIRST_VIDEO_STREAM, MF_VERSION, IMFSourceReader,
};
use std::os::windows::ffi::OsStrExt;
use windows::Win32::System::Com::{
    CoCreateInstance, CoInitializeEx, CLSCTX_ALL, COINIT_MULTITHREADED,
    StructuredStorage::PROPVARIANT,
};
use windows::Win32::System::Variant::{VT_I8, VT_UI8};

/// GUID_NULL（Seek 的时间格式 = 100ns 单位）
const GUID_NULL: windows::core::GUID = windows::core::GUID::zeroed();

pub enum MediaCommand {
    Play,
    Pause,
    Seek(f64),
}

pub enum MediaEvent {
    Frame { rgba: Vec<u8>, w: u32, h: u32 },
    Duration(f64),
    Progress(f64),
    Playing(bool),
    Ended,
    AudioReady,
    Error(String),
}

pub struct MediaPlayer {
    pub cmd_tx: mpsc::Sender<MediaCommand>,
    evt_rx: mpsc::Receiver<MediaEvent>,
    pub playing: bool,
    pub position: f64,
    pub duration: f64,
    pub texture: Option<egui::TextureHandle>,
    pub ended: bool,
    pub audio_ok: bool,
    pub last_error: Option<String>,
}

impl MediaPlayer {
    pub fn spawn(path: String) -> MediaPlayer {
        let (cmd_tx, cmd_rx) = mpsc::channel();
        let (evt_tx, evt_rx) = mpsc::channel();
        std::thread::Builder::new()
            .name("media-mf".into())
            .spawn(move || unsafe {
                worker(path, cmd_rx, evt_tx);
            })
            .ok();
        MediaPlayer {
            cmd_tx,
            evt_rx,
            playing: false,
            position: 0.0,
            duration: 0.0,
            texture: None,
            ended: false,
            audio_ok: false,
            last_error: None,
        }
    }

    /// 消费 worker 事件（每帧调用）
    pub fn poll(&mut self, ctx: &egui::Context) {
        // 无纹理期间低频重绘（海报帧/看门狗需要 UI 持续运转）
        if self.texture.is_none() {
            ctx.request_repaint_after(std::time::Duration::from_millis(500));
        }
        while let Ok(ev) = self.evt_rx.try_recv() {
            match ev {
                MediaEvent::Frame { rgba, w, h } => {
                    let img = egui::ColorImage::from_rgba_unmultiplied(
                        [w as usize, h as usize],
                        &rgba,
                    );
                    match &mut self.texture {
                        Some(t) => t.set(img, Default::default()),
                        None => {
                            self.texture =
                                Some(ctx.load_texture("media_frame", img, Default::default()))
                        }
                    }
                    ctx.request_repaint();
                }
                MediaEvent::Duration(d) => self.duration = d,
                MediaEvent::Progress(p) => self.position = p,
                MediaEvent::Playing(p) => self.playing = p,
                MediaEvent::Ended => {
                    self.playing = false;
                    self.ended = true;
                }
                MediaEvent::AudioReady => self.audio_ok = true,
                MediaEvent::Error(e) => {
                    self.playing = false;
                    self.last_error = Some(e);
                }
            }
        }
    }
}

// ---------------- worker ----------------

const HNS_PER_SEC: f64 = 10_000_000.0;

struct AudioOut {
    client: IAudioClient,
    render: IAudioRenderClient,
    buf_frames: u32,
    frame_bytes: usize, // ch * 2 (PCM16)
}

unsafe fn worker(
    path: String,
    cmd_rx: mpsc::Receiver<MediaCommand>,
    evt_tx: mpsc::Sender<MediaEvent>,
) {
    let send_err = |msg: String| {
        let _ = evt_tx.send(MediaEvent::Error(msg));
    };
    eprintln!("[media] worker 启动");
    CoInitializeEx(None, COINIT_MULTITHREADED);
    if let Err(e) = MFStartup(MF_VERSION, 0) {
        send_err(format!("Media Foundation 初始化失败：{}", e));
        return;
    }
    // 文件经 IStream → 字节流（矩阵探针验证：IStream 路线在任意 COM 组合下均工作；
    // file:/// URL 路线对 COM 初始化方式敏感，已弃用）
    let wide: Vec<u16> = std::ffi::OsStr::new(&path)
        .encode_wide()
        .chain(std::iter::once(0))
        .collect();
    let stream = match windows::Win32::UI::Shell::SHCreateStreamOnFileW(
        windows::core::PCWSTR(wide.as_ptr()),
        0, // STGM_READ
    ) {
        Ok(s) => s,
        Err(e) => {
            send_err(format!("无法打开媒体文件：{}", e));
            return;
        }
    };
    let bytestream = match MFCreateMFByteStreamOnStream(&stream) {
        Ok(b) => b,
        Err(e) => {
            send_err(format!("创建字节流失败：{}", e));
            return;
        }
    };
    let mut attrs = None;
    MFCreateAttributes(&mut attrs, 2).expect("reader attrs");
    if let Some(a) = attrs.as_ref() {
        // RGB32 转换必须开启高级视频处理；
        // DISABLE_DXVA 强制软解——避免与 eframe 的 D3D 设备争用导致解码挂死
        let _ = a.SetUINT32(&MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, 1);
        let _ = a.SetUINT32(&MF_SOURCE_READER_DISABLE_DXVA, 1);
    }
    let reader: IMFSourceReader = match MFCreateSourceReaderFromByteStream(&bytestream, attrs.as_ref()) {
        Ok(r) => {
            eprintln!("[media] SourceReader ok");
            r
        }
        Err(e) => {
            eprintln!("[media] SourceReader 失败：{}", e);
            send_err(format!("无法打开媒体文件：{}", e));
            return;
        }
    };
    // 时长：dwStreamIndex=0xFFFFFFFF（ANY_STREAM；crate 的 ALL_STREAMS=-2 是错的）
    if let Ok(pv) = reader.GetPresentationAttribute(0xFFFFFFFFu32, &MF_PD_DURATION) {
        if let Some(d) = propvar_u64(&pv) {
            let _ = evt_tx.send(MediaEvent::Duration(d as f64 / HNS_PER_SEC));
        }
    }
    // 视频输出：克隆原生类型改子类型为 RGB32（从零构造会被拒 0xC00D36B4）
    let vt = match reader
        .GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM.0 as u32, 0)
        .and_then(|native| {
            let mt = MFCreateMediaType()?;
            native.CopyAllItems(&mt)?;
            Ok(mt)
        }) {
        Ok(t) => t,
        Err(e) => {
            send_err(format!("读取原生视频类型失败：{}", e));
            return;
        }
    };
    let _ = vt.SetGUID(&MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    if let Err(e) = reader.SetCurrentMediaType(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM.0 as u32,
        None,
        &vt,
    ) {
        send_err(format!("视频流不支持 RGB32 输出：{}", e));
        return;
    }
    // 帧尺寸
    let (mut vid_w, mut vid_h) = (0u32, 0u32);
    if let Ok(t) = reader.GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM.0 as u32, 0) {
        if let Ok(sz) = t.GetUINT64(&MF_MT_FRAME_SIZE) {
            vid_w = (sz >> 32) as u32;
            vid_h = sz as u32;
        }
    }
    eprintln!("[media] 视频 {}x{}", vid_w, vid_h);

    // 音频（失败不阻断播放，仅无声）
    let mut audio: Option<AudioOut> = None;
    let mut audio_stream_idx = MF_SOURCE_READER_FIRST_AUDIO_STREAM.0 as u32;
    match open_audio(&reader, &mut audio_stream_idx) {
        Ok(a) => {
            let _ = evt_tx.send(MediaEvent::AudioReady);
            audio = Some(a);
        }
        Err(e) => {
            let _ = evt_tx.send(MediaEvent::Error(format!("音频不可用（{}），将以无声播放", e)));
        }
    }

    // 主循环状态
    enum St {
        Priming,          // 解码海报帧，等待 Play
        Playing,          // 播放中
        Paused,           // 暂停（含 Seek 后停帧）
        Ended,            // 播完
    }
    let mut state = St::Priming;
    let mut media_base = 0f64; // 播放开始时的媒体时间
    let mut wall_base = Instant::now();
    let mut media_pos = 0f64;
    let mut video_done = false;
    let mut audio_done = audio.is_none();
    let mut audio_pending: Option<(Vec<u8>, f64)> = None; // 待写 PCM 块及其媒体时间
    let mut video_pending: Option<(Vec<u8>, u32, u32, f64)> = None;

    loop {
        // ---- 命令处理（所有状态共用） ----
        let mut exit = false;
        loop {
            match cmd_rx.try_recv() {
                Ok(MediaCommand::Play) => {
                    match state {
                        St::Ended => {
                            // 从头重播
                            if seek_to(&reader, 0.0).is_err() {
                                break;
                            }
                            reset_audio(audio.as_ref());
                            media_base = 0.0;
                            wall_base = Instant::now();
                            video_done = false;
                            audio_done = audio.is_none();
                            video_pending = None;
                            audio_pending = None;
                            state = St::Playing;
                            let _ = evt_tx.send(MediaEvent::Playing(true));
                        }
                        St::Paused | St::Priming => {
                            media_base = media_pos;
                            wall_base = Instant::now();
                            resume_audio(audio.as_ref());
                            state = St::Playing;
                            let _ = evt_tx.send(MediaEvent::Playing(true));
                        }
                        St::Playing => {}
                    }
                }
                Ok(MediaCommand::Pause) => {
                    if let St::Playing = state {
                        pause_audio(audio.as_ref());
                        state = St::Paused;
                        let _ = evt_tx.send(MediaEvent::Playing(false));
                    }
                }
                Ok(MediaCommand::Seek(t)) => {
                    if seek_to(&reader, t).is_ok() {
                        reset_audio(audio.as_ref());
                        media_base = t;
                        media_pos = t;
                        wall_base = Instant::now();
                        video_done = false;
                        audio_done = audio.is_none();
                        video_pending = None;
                        audio_pending = None;
                        match state {
                            St::Playing => {}
                            _ => {
                                state = St::Paused;
                                let _ = evt_tx.send(MediaEvent::Playing(false));
                            }
                        }
                        let _ = evt_tx.send(MediaEvent::Progress(t));
                    }
                }
                Ok(MediaCommand::Pause) => {}
                Err(mpsc::TryRecvError::Disconnected) => {
                    exit = true;
                    break;
                }
                Err(mpsc::TryRecvError::Empty) => break,
            }
        }
        if exit {
            pause_audio(audio.as_ref());
            return;
        }

        // ---- 状态机 ----
        match state {
            St::Priming => {
                // 解码首帧作为海报
                match read_video_frame(&reader, vid_w, vid_h) {
                    Ok(Some((rgba, time))) => {
                        media_pos = time as f64 / HNS_PER_SEC;
                        let _ = evt_tx.send(MediaEvent::Frame { rgba, w: vid_w, h: vid_h });
                        let _ = evt_tx.send(MediaEvent::Progress(media_pos));
                        state = St::Paused;
                    }
                    Ok(None) => {
                        send_err("无法解码视频帧".into());
                        return;
                    }
                    Err(e) => {
                        send_err(e);
                        return;
                    }
                }
            }
            St::Playing => {
                let now_media = media_base + wall_base.elapsed().as_secs_f64();
                media_pos = now_media.min(media_base.max(media_pos));
                let _ = evt_tx.send(MediaEvent::Progress(media_pos));
                // 音频：把媒体时间 ≤ 当前位置 的 PCM 写入 WASAPI（缓冲写满即天然节流）
                if let Some(out) = audio.as_ref() {
                    while !audio_done {
                        let at = match &audio_pending {
                            Some((_, t)) => *t,
                            None => match read_audio_pcm(&reader, out.frame_bytes) {
                                Ok(Some((pcm, t))) => {
                                    audio_pending = Some((pcm, t));
                                    t
                                }
                                Ok(None) => {
                                    audio_done = true;
                                    break;
                                }
                                Err(_) => {
                                    audio_done = true;
                                    break;
                                }
                            },
                        };
                        if at > now_media {
                            break;
                        }
                        let (pcm, t) = audio_pending.take().unwrap();
                        if !write_audio(out, &pcm, &cmd_rx, &mut |c| match c {
                            MediaCommand::Pause => true,
                            _ => false,
                        }) {
                            // 写入期间收到暂停 → 回退：重新排队
                            audio_pending = Some((pcm, t));
                            break;
                        }
                    }
                }
                // 视频：到达时间即发帧
                if !video_done {
                    if video_pending.is_none() {
                        match read_video_frame(&reader, vid_w, vid_h) {
                            Ok(Some((rgba, time))) => {
                                video_pending =
                                    Some((rgba, vid_w, vid_h, time as f64 / HNS_PER_SEC));
                            }
                            Ok(None) => video_done = true,
                            Err(_) => video_done = true,
                        }
                    }
                    if let Some((rgba, w, h, t)) = &video_pending {
                        if *t <= media_pos + 0.005 {
                            let _ = evt_tx.send(MediaEvent::Frame {
                                rgba: rgba.clone(),
                                w: *w,
                                h: *h,
                            });
                            video_pending = None;
                        } else {
                            std::thread::sleep(std::time::Duration::from_millis(3));
                        }
                    }
                }
                if video_done && audio_done {
                    if let Some(a) = audio.as_ref() {
                        // 等音频放完缓冲
                        let padding = a.client.GetCurrentPadding().unwrap_or(0);
                        if padding == 0 {
                            let _ = evt_tx.send(MediaEvent::Ended);
                            state = St::Ended;
                        } else {
                            std::thread::sleep(std::time::Duration::from_millis(30));
                        }
                    } else {
                        let _ = evt_tx.send(MediaEvent::Ended);
                        state = St::Ended;
                    }
                }
                std::thread::sleep(std::time::Duration::from_millis(2));
            }
            St::Paused | St::Ended => {
                std::thread::sleep(std::time::Duration::from_millis(15));
            }
        }
    }
}

// ---------------- 视频 ----------------

/// 读一帧并翻转为 RGBA（MF RGB32 底朝上）
unsafe fn read_video_frame(
    reader: &IMFSourceReader,
    w: u32,
    h: u32,
) -> Result<Option<(Vec<u8>, i64)>, String> {
    eprintln!("[media] ReadSample(视频)…");
    let mut time = 0i64;
    let mut sample: Option<windows::Win32::Media::MediaFoundation::IMFSample> = None;
    reader
        .ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM.0 as u32,
            0,
            None,
            None,
            Some(&mut time),
            Some(&mut sample),
        )
        .map_err(|e| format!("视频解码失败：{}", e))?;
    let Some(sample) = sample else {
        return Ok(None); // EOF
    };
    eprintln!("[media] 视频样本到手 t={}", time);
    let buf = sample
        .ConvertToContiguousBuffer()
        .map_err(|e| format!("样本缓冲失败：{}", e))?;
    let mut ptr: *mut u8 = std::ptr::null_mut();
    let mut max_len = 0u32;
    let mut cur_len = 0u32;
    buf.Lock(&mut ptr, Some(&mut max_len), Some(&mut cur_len))
        .map_err(|e| format!("缓冲锁定失败：{}", e))?;
    let expected = (w as usize) * (h as usize) * 4;
    let mut rgba = vec![0u8; expected];
    if !ptr.is_null() && cur_len as usize >= expected && h > 0 {
        let src = std::slice::from_raw_parts(ptr, cur_len as usize);
        let stride = cur_len as usize / h as usize;
        for row in 0..h as usize {
            let src_row = &src[(h as usize - 1 - row) * stride..(h as usize - 1 - row) * stride + w as usize * 4];
            rgba[row * w as usize * 4..(row + 1) * w as usize * 4].copy_from_slice(src_row);
        }
    }
    let _ = buf.Unlock();
    Ok(Some((rgba, time)))
}

// ---------------- 音频 ----------------

unsafe fn open_audio(
    reader: &IMFSourceReader,
    stream_idx: &mut u32,
) -> Result<AudioOut, String> {
    // 默认输出设备
    let enumerator: IMMDeviceEnumerator =
        CoCreateInstance(&MMDeviceEnumerator, None, CLSCTX_ALL).map_err(|e| e.to_string())?;
    let device = enumerator
        .GetDefaultAudioEndpoint(eRender, eConsole)
        .map_err(|e| e.to_string())?;
    let client: IAudioClient = device
        .Activate(CLSCTX_ALL, None)
        .map_err(|e| e.to_string())?;
    let mix = client.GetMixFormat().map_err(|e| e.to_string())?;
    let (rate, channels) = if mix.is_null() {
        (48000u32, 2u16)
    } else {
        ((*mix).nSamplesPerSec, (*mix).nChannels)
    };
    client
        .Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 2_000_000, 0, mix, None)
        .map_err(|e| format!("WASAPI 初始化失败：{}", e))?;
    let render: IAudioRenderClient = client.GetService().map_err(|e| e.to_string())?;
    let buf_frames = client.GetBufferSize().map_err(|e| e.to_string())?;
    // SourceReader 音频输出：PCM16、混音格式的采样率/声道（MF 自动重采样）
    let at = MFCreateMediaType().map_err(|e| e.to_string())?;
    let _ = at.SetGUID(&MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    let _ = at.SetGUID(&MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    let _ = at.SetUINT32(&MF_MT_AUDIO_SAMPLES_PER_SECOND, rate);
    let _ = at.SetUINT32(&MF_MT_AUDIO_NUM_CHANNELS, channels as u32);
    let _ = at.SetUINT32(&MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    reader
        .SetCurrentMediaType(*stream_idx, None, &at)
        .map_err(|e| format!("音频流配置失败：{}", e))?;
    client.Start().map_err(|e| e.to_string())?;
    Ok(AudioOut {
        client,
        render,
        buf_frames,
        frame_bytes: channels as usize * 2,
    })
}

/// 读一块音频 PCM（返回 (数据, 媒体时间秒)）；None = 音频流结束
unsafe fn read_audio_pcm(
    reader: &IMFSourceReader,
    frame_bytes: usize,
) -> Result<Option<(Vec<u8>, f64)>, String> {
    let mut time = 0i64;
    let mut sample: Option<windows::Win32::Media::MediaFoundation::IMFSample> = None;
    reader
        .ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM.0 as u32,
            0,
            None,
            None,
            Some(&mut time),
            Some(&mut sample),
        )
        .map_err(|e| format!("音频解码失败：{}", e))?;
    let Some(sample) = sample else {
        return Ok(None);
    };
    let buf = sample.ConvertToContiguousBuffer().map_err(|e| e.to_string())?;
    let mut ptr: *mut u8 = std::ptr::null_mut();
    let mut max_len = 0u32;
    let mut cur_len = 0u32;
    buf.Lock(&mut ptr, Some(&mut max_len), Some(&mut cur_len))
        .map_err(|e| e.to_string())?;
    let data = if ptr.is_null() {
        Vec::new()
    } else {
        std::slice::from_raw_parts(ptr, cur_len as usize).to_vec()
    };
    let _ = buf.Unlock();
    if data.is_empty() || data.len() < frame_bytes {
        return Ok(None);
    }
    Ok(Some((data, time as f64 / HNS_PER_SEC)))
}

/// 写 PCM 到 WASAPI；期间轮询暂停命令，被暂停返回 false（调用方回退未写完的数据）
unsafe fn write_audio(
    out: &AudioOut,
    data: &[u8],
    cmd_rx: &mpsc::Receiver<MediaCommand>,
    is_pause: &mut dyn FnMut(&MediaCommand) -> bool,
) -> bool {
    let mut rest = data;
    while !rest.is_empty() {
        // 轮询暂停
        if let Ok(c) = cmd_rx.try_recv() {
            if is_pause(&c) {
                return false;
            }
            // 非暂停命令（Play/Seek）在暂停时不可能到达此处；忽略
        }
        let padding = match out.client.GetCurrentPadding() {
            Ok(p) => p,
            Err(_) => return true,
        };
        let free = out.buf_frames.saturating_sub(padding);
        if free == 0 {
            std::thread::sleep(std::time::Duration::from_millis(6));
            continue;
        }
        let take_frames = free.min((rest.len() / out.frame_bytes) as u32);
        if take_frames == 0 {
            // 剩余不足一帧（理论不可能），丢弃
            return true;
        }
        let take_bytes = take_frames as usize * out.frame_bytes;
        let Ok(dst) = out.render.GetBuffer(take_frames) else {
            return true;
        };
        std::ptr::copy_nonoverlapping(rest.as_ptr(), dst, take_bytes);
        out.render.ReleaseBuffer(take_frames, 0);
        rest = &rest[take_bytes..];
    }
    true
}

fn reset_audio(audio: Option<&AudioOut>) {
    if let Some(a) = audio {
        unsafe {
            let _ = a.client.Stop();
            let _ = a.client.Reset();
            let _ = a.client.Start();
        }
    }
}

fn pause_audio(audio: Option<&AudioOut>) {
    if let Some(a) = audio {
        unsafe {
            let _ = a.client.Stop();
        }
    }
}

fn resume_audio(audio: Option<&AudioOut>) {
    if let Some(a) = audio {
        unsafe {
            let _ = a.client.Start();
        }
    }
}

/// Seek（便捷 API 直接吃 100ns 单位）
unsafe fn seek_to(reader: &IMFSourceReader, secs: f64) -> Result<(), String> {
    let mut pv = PROPVARIANT::default();
    // ManuallyDrop 为 repr(transparent)，指针转换后直接写字段
    let inner = std::ptr::addr_of_mut!(pv.Anonymous.Anonymous)
        as *mut windows::Win32::System::Com::StructuredStorage::PROPVARIANT_0_0;
    unsafe {
        (*inner).vt = VT_I8;
        (*inner).Anonymous.hVal = (secs * HNS_PER_SEC) as i64;
    }
    reader
        .SetCurrentPosition(&GUID_NULL, &pv)
        .map_err(|e| format!("Seek 失败：{}", e))
}

/// PROPVARIANT → u64（VT_UI8）
unsafe fn propvar_u64(pv: &PROPVARIANT) -> Option<u64> {
    let inner = std::ptr::addr_of!(pv.Anonymous.Anonymous)
        as *const windows::Win32::System::Com::StructuredStorage::PROPVARIANT_0_0;
    let inner = unsafe { &*inner };
    if inner.vt == VT_UI8 {
        Some(inner.Anonymous.hVal as u64)
    } else {
        None
    }
}
