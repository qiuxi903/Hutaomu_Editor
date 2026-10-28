// Hutaomu Editor (Rust) - Windows 文件关联（对标 Qt FileAssociations：HKCU 注册表，无需管理员）。
// SPDX-License-Identifier: AGPL-3.0-only

#[cfg(windows)]
use winreg::enums::*;
#[cfg(windows)]
use winreg::RegKey;

/// ProgId（Qt: HutaomuEditor.*，Rust 版独立前缀）
pub const PROG_ID: &str = "HutaomuEditorRust.Document";
pub const PROG_ID_DESC: &str = "Hutaomu Editor (Rust) 文档";

/// 可关联的预设扩展名（安装包 [Tasks] 与设置界面共用）
pub const PRESET_EXTS: [&str; 18] = [
    "md", "txt", "log", "json", "toml", "yaml", "yml", "ini", "csv", "rs", "py", "js", "ts", "c",
    "cpp", "h", "html", "css",
];

fn exe_path() -> String {
    std::env::current_exe()
        .map(|p| p.to_string_lossy().to_string())
        .unwrap_or_default()
}

/// 当前扩展名是否已关联到本程序
#[cfg(windows)]
pub fn is_associated(ext: &str) -> bool {
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    let Ok(classes) = hkcu.open_subkey("Software\\Classes") else {
        return false;
    };
    classes
        .open_subkey(format!(".{}", ext))
        .and_then(|k| k.get_value::<String, _>(""))
        .map(|v| v == PROG_ID)
        .unwrap_or(false)
}

#[cfg(not(windows))]
pub fn is_associated(_ext: &str) -> bool {
    false
}

/// 写入 ProgId 与扩展名映射（HKCU，无 UAC）
#[cfg(windows)]
pub fn associate(exts: &[String]) -> Result<(), String> {
    let exe = exe_path();
    if exe.is_empty() {
        return Err("无法定位程序路径".into());
    }
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    let classes = hkcu
        .open_subkey_with_flags("Software\\Classes", KEY_SET_VALUE | KEY_CREATE_SUB_KEY)
        .map_err(|e| format!("无法写入注册表（HKCU\\Software\\Classes）：{}", e))?;
    // ProgId：shell\open\command
    let prog = classes
        .create_subkey(PROG_ID)
        .map_err(|e| e.to_string())?
        .0;
    prog.set_value("", &PROG_ID_DESC).map_err(|e| e.to_string())?;
    let cmd = prog
        .create_subkey("shell\\open\\command")
        .map_err(|e| e.to_string())?
        .0;
    cmd.set_value("", &format!("\"{}\" \"%1\"", exe))
        .map_err(|e| e.to_string())?;
    // 扩展名 → ProgId
    for ext in exts {
        let key = classes
            .create_subkey(format!(".{}", ext))
            .map_err(|e| e.to_string())?
            .0;
        key.set_value("", &PROG_ID).map_err(|e| e.to_string())?;
    }
    Ok(())
}

#[cfg(not(windows))]
pub fn associate(_exts: &[String]) -> Result<(), String> {
    Err("文件关联仅支持 Windows".into())
}

/// 取消关联（若当前指向本程序才清除）
#[cfg(windows)]
pub fn dissociate(exts: &[String]) -> Result<(), String> {
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    let Ok(classes) = hkcu.open_subkey_with_flags("Software\\Classes", KEY_SET_VALUE | KEY_QUERY_VALUE)
    else {
        return Ok(());
    };
    for ext in exts {
        let name = format!(".{}", ext);
        if let Ok(k) = classes.open_subkey_with_flags(&name, KEY_SET_VALUE | KEY_QUERY_VALUE) {
            let ours: bool = k.get_value::<String, _>("").map(|v| v == PROG_ID).unwrap_or(false);
            if ours {
                drop(k);
                classes.delete_subkey(&name).map_err(|e| e.to_string())?;
            }
        }
    }
    Ok(())
}

#[cfg(not(windows))]
pub fn dissociate(_exts: &[String]) -> Result<(), String> {
    Ok(())
}

/// 通知外壳刷新关联（关联变更后调用）
#[cfg(windows)]
pub fn notify_shell() {
    use std::os::windows::ffi::OsStrExt;
    let wide: Vec<u16> = std::ffi::OsStr::new("Environment")
        .encode_wide()
        .chain(std::iter::once(0))
        .collect();
    unsafe {
        SendMessageTimeoutW(
            HWND_BROADCAST,
            WM_SETTINGCHANGE,
            0usize,
            wide.as_ptr() as isize,
            SMTO_ABORTIFHUNG,
            2000,
            None,
        );
    }
}

#[cfg(not(windows))]
pub fn notify_shell() {}

#[cfg(windows)]
const HWND_BROADCAST: isize = 0xFFFF;
#[cfg(windows)]
const WM_SETTINGCHANGE: u32 = 0x001A;
#[cfg(windows)]
const SMTO_ABORTIFHUNG: u32 = 0x0002;

#[cfg(windows)]
#[link(name = "user32")]
extern "system" {
    fn SendMessageTimeoutW(
        hwnd: isize,
        msg: u32,
        wparam: usize,
        lparam: isize,
        flags: u32,
        timeout: u32,
        result: Option<*mut usize>,
    ) -> isize;
}
