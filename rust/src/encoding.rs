// Hutaomu Editor (Rust) - 编码检测与转换（对标 Qt 版：UTF-8/BOM/UTF-16/GB18030/Latin-1）。
use encoding_rs::Encoding;

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum TextEncoding {
    Utf8,
    Utf8Bom,
    Utf16Le,
    Utf16Be,
    Gb18030,
    Latin1,
}

impl TextEncoding {
    pub fn display(&self) -> &'static str {
        match self {
            TextEncoding::Utf8 => "UTF-8",
            TextEncoding::Utf8Bom => "UTF-8 BOM",
            TextEncoding::Utf16Le => "UTF-16 LE",
            TextEncoding::Utf16Be => "UTF-16 BE",
            TextEncoding::Gb18030 => "GB18030",
            TextEncoding::Latin1 => "Latin-1",
        }
    }

    pub fn had_bom(&self) -> bool {
        matches!(self, TextEncoding::Utf8Bom)
    }
}

/// 从字节检测编码（BOM 优先；无 BOM 时严格 UTF-8，失败回退 GB18030）
pub fn detect(bytes: &[u8]) -> TextEncoding {
    if bytes.starts_with(&[0xEF, 0xBB, 0xBF]) {
        return TextEncoding::Utf8Bom;
    }
    if bytes.starts_with(&[0xFF, 0xFE]) {
        return TextEncoding::Utf16Le;
    }
    if bytes.starts_with(&[0xFE, 0xFF]) {
        return TextEncoding::Utf16Be;
    }
    if bytes.len() >= 2 && bytes[0] == 0 && bytes[1] != 0 {
        return TextEncoding::Utf16Be; // 罕见对齐
    }
    if bytes.len() >= 2 && bytes[0] != 0 && bytes[1] == 0 && bytes.len() > 8 {
        // 大量 0 字节间隔 → UTF-16LE（ASCII 密集）
        let zeros = bytes.chunks(2).filter(|c| c[1] == 0).count();
        if zeros * 2 > bytes.len() {
            return TextEncoding::Utf16Le;
        }
    }
    // 严格 UTF-8 校验
    match std::str::from_utf8(bytes) {
        Ok(_) => TextEncoding::Utf8,
        Err(_) => {
            // GB18030 能覆盖大部分中文场景；Latin-1 永不失败
            let (cow, _, had_errors) = encoding_rs::GB18030.decode(bytes);
            if had_errors {
                TextEncoding::Latin1
            } else {
                let _ = cow;
                TextEncoding::Gb18030
            }
        }
    }
}

/// 解码为字符串（同时返回使用的编码）
pub fn decode(bytes: &[u8]) -> (String, TextEncoding) {
    let enc = detect(bytes);
    let text = match enc {
        TextEncoding::Utf8 => String::from_utf8_lossy(bytes).to_string(),
        TextEncoding::Utf8Bom => {
            String::from_utf8_lossy(&bytes[3..]).to_string()
        }
        TextEncoding::Utf16Le => {
            let (cow, _, had_errors) = encoding_rs::UTF_16LE.decode(bytes);
            if had_errors {
                return (String::from_utf8_lossy(bytes).to_string(), TextEncoding::Utf8);
            }
            cow.to_string()
        }
        TextEncoding::Utf16Be => {
            let (cow, _, had_errors) = encoding_rs::UTF_16BE.decode(bytes);
            if had_errors {
                return (String::from_utf8_lossy(bytes).to_string(), TextEncoding::Utf8);
            }
            cow.to_string()
        }
        TextEncoding::Gb18030 => {
            let (cow, _, _) = encoding_rs::GB18030.decode(bytes);
            cow.to_string()
        }
        TextEncoding::Latin1 => {
            let (cow, _, _) = encoding_rs::WINDOWS_1252.decode(bytes);
            cow.to_string()
        }
    };
    (text, enc)
}

/// 按编码编码回字节（BOM 保持）
pub fn encode(text: &str, enc: TextEncoding) -> Vec<u8> {
    match enc {
        TextEncoding::Utf8 => text.as_bytes().to_vec(),
        TextEncoding::Utf8Bom => {
            let mut out = vec![0xEF, 0xBB, 0xBF];
            out.extend_from_slice(text.as_bytes());
            out
        }
        // encoding_rs 的 Encoding::encode 对 UTF-16LE/BE 会静默降级为 UTF-8
        // （output_encoding 映射），因此这里用标准库手写，BOM 必带（与 Qt 版一致）。
        TextEncoding::Utf16Le => {
            let mut out = vec![0xFF, 0xFE];
            for unit in text.encode_utf16() {
                out.extend_from_slice(&unit.to_le_bytes());
            }
            out
        }
        TextEncoding::Utf16Be => {
            let mut out = vec![0xFE, 0xFF];
            for unit in text.encode_utf16() {
                out.extend_from_slice(&unit.to_be_bytes());
            }
            out
        }
        TextEncoding::Gb18030 => {
            let (cow, _, _) = encoding_rs::GB18030.encode(text);
            cow.to_vec()
        }
        TextEncoding::Latin1 => {
            let (cow, _, _) = encoding_rs::WINDOWS_1252.encode(text);
            cow.to_vec()
        }
    }
}

/// 检测换行符
pub fn detect_eol(text: &str) -> Eol {
    if text.contains("\r\n") {
        Eol::Crlf
    } else if text.contains('\r') {
        Eol::Cr
    } else {
        Eol::Lf
    }
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum Eol {
    Lf,
    Crlf,
    Cr,
}

impl Eol {
    pub fn display(&self) -> &'static str {
        match self {
            Eol::Lf => "LF",
            Eol::Crlf => "CRLF",
            Eol::Cr => "CR",
        }
    }
    pub fn as_str(&self) -> &'static str {
        match self {
            Eol::Lf => "\n",
            Eol::Crlf => "\r\n",
            Eol::Cr => "\r",
        }
    }
}

/// 原子保存：写临时文件后 rename
pub fn atomic_write(path: &str, bytes: &[u8]) -> Result<(), String> {
    let tmp = format!("{}.hutmp", path);
    std::fs::write(&tmp, bytes).map_err(|e| e.to_string())?;
    std::fs::rename(&tmp, path).map_err(|e| e.to_string())?;
    Ok(())
}

/// 编码名 → 静态引用（状态栏/保存用）
pub fn by_display(name: &str) -> Option<TextEncoding> {
    Some(match name {
        "UTF-8" => TextEncoding::Utf8,
        "UTF-8 BOM" => TextEncoding::Utf8Bom,
        "UTF-16 LE" => TextEncoding::Utf16Le,
        "UTF-16 BE" => TextEncoding::Utf16Be,
        "GB18030" => TextEncoding::Gb18030,
        "Latin-1" => TextEncoding::Latin1,
        _ => return None,
    })
}

pub fn all_displays() -> [&'static str; 6] {
    ["UTF-8", "UTF-8 BOM", "UTF-16 LE", "UTF-16 BE", "GB18030", "Latin-1"]
}

pub fn encoding_ref(e: &TextEncoding) -> &'static Encoding {
    match e {
        TextEncoding::Utf8 | TextEncoding::Utf8Bom => encoding_rs::UTF_8,
        TextEncoding::Utf16Le => encoding_rs::UTF_16LE,
        TextEncoding::Utf16Be => encoding_rs::UTF_16BE,
        TextEncoding::Gb18030 => encoding_rs::GB18030,
        TextEncoding::Latin1 => encoding_rs::WINDOWS_1252,
    }
}
