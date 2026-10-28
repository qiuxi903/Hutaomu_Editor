// MF 矩阵探针（GUI 子系统版）
#![windows_subsystem = "windows"]
use std::os::windows::ffi::OsStrExt;
use windows::Win32::Media::MediaFoundation::{
    MFCreateAttributes, MFCreateMediaType, MFCreateMFByteStreamOnStream,
    MFCreateSourceReaderFromByteStream, MFMediaType_Audio, MFMediaType_Video, MFAudioFormat_PCM,
    MFVideoFormat_RGB32, MFStartup, MF_MT_MAJOR_TYPE, MF_MT_SUBTYPE,
    MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, MF_SOURCE_READER_FIRST_VIDEO_STREAM,
    MF_VERSION, IMFSourceReader,
};
use windows::Win32::System::Com::{
    CoInitializeEx, CLSCTX_ALL, COINIT_APARTMENTTHREADED, COINIT_MULTITHREADED,
};

#[derive(Clone, Copy, PartialEq)]
enum CoInit {
    None,
    Mta,
    Sta,
}

unsafe fn run(path: &str, co: CoInit) -> Result<String, String> {
    match co {
        CoInit::Mta => {
            CoInitializeEx(None, COINIT_MULTITHREADED);
        }
        CoInit::Sta => {
            CoInitializeEx(None, COINIT_APARTMENTTHREADED);
        }
        CoInit::None => {}
    }
    MFStartup(MF_VERSION, 0).map_err(|e| e.to_string())?;
    let wide: Vec<u16> = std::ffi::OsStr::new(path)
        .encode_wide()
        .chain(std::iter::once(0))
        .collect();
    let stream = windows::Win32::UI::Shell::SHCreateStreamOnFileW(
        windows::core::PCWSTR(wide.as_ptr()),
        0,
    )
    .map_err(|e| format!("IStream: {}", e))?;
    let bs = MFCreateMFByteStreamOnStream(&stream).map_err(|e| format!("ByteStream: {}", e))?;
    let mut attrs = None;
    MFCreateAttributes(&mut attrs, 2).map_err(|e| e.to_string())?;
    if let Some(a) = attrs.as_ref() {
        a.SetUINT32(
            &MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING,
            1,
        )
        .map_err(|e| e.to_string())?;
    }
    let reader: IMFSourceReader =
        MFCreateSourceReaderFromByteStream(&bs, attrs.as_ref()).map_err(|e| format!("reader: {}", e))?;
    let at = MFCreateMediaType().map_err(|e| e.to_string())?;
    at.SetGUID(&MF_MT_MAJOR_TYPE, &MFMediaType_Audio).map_err(|e| e.to_string())?;
    at.SetGUID(&MF_MT_SUBTYPE, &MFAudioFormat_PCM).map_err(|e| e.to_string())?;
    let mut time = 0i64;
    let mut sample = None;
    let mut actual = 0u32;
    let mut flags = 0u32;
    reader
        .ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM.0 as u32,
            0,
            Some(&mut actual),
            Some(&mut flags),
            Some(&mut time),
            Some(&mut sample),
        )
        .map_err(|e| format!("ReadSample: {}", e))?;
    let mut out = format!("样本到达 t={}", time);
    if let Some(s) = &sample {
        let buf = s.ConvertToContiguousBuffer().map_err(|e| e.to_string())?;
        let mut ptr = std::ptr::null_mut();
        let mut maxl = 0u32;
        let mut curl = 0u32;
        let _ = buf.Lock(&mut ptr, Some(&mut maxl), Some(&mut curl));
        out += &format!(" bytes={}", curl);
        let _ = buf.Unlock();
    }
    Ok(out)
}

fn probe(label: &str, path: &str, co: CoInit) {
    let p = path.to_string();
    let (tx, rx) = std::sync::mpsc::channel::<Result<String, String>>();
    std::thread::spawn(move || {
        let r = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| unsafe { run(&p, co) }))
            .unwrap_or_else(|_| Err("panic".into()));
        let _ = tx.send(r);
    });
    match rx.recv_timeout(std::time::Duration::from_secs(10)) {
        Ok(Ok(msg)) => println!("[{}] OK {}", label, msg),
        Ok(Err(e)) => println!("[{}] FAIL {}", label, e),
        Err(_) => println!("[{}] !!! 10 秒超时", label),
    }
}

fn main() {
    let mp4 = "C:/Users/anr10/Desktop/testvideo.mp4";
    probe("视频 主线程 无CoInit", mp4, CoInit::None);
    probe("视频 主线程 MTA   ", mp4, CoInit::Mta);
    probe("视频 主线程 STA   ", mp4, CoInit::Sta);
    probe("视频 子线程 无CoInit", mp4, CoInit::None);
    probe("视频 子线程 MTA   ", mp4, CoInit::Mta);
    probe("视频 子线程 STA   ", mp4, CoInit::Sta);
}
