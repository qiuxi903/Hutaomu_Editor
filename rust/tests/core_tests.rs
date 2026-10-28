// Hutaomu Editor (Rust) - 核心逻辑单元测试（纯逻辑，无需 GUI）。
use hutaomu_editor::encoding::{self, Eol, TextEncoding};
use hutaomu_editor::markdown;

#[test]
fn encoding_detect_utf8() {
    assert_eq!(encoding::detect("hello 世界".as_bytes()), TextEncoding::Utf8);
}

#[test]
fn encoding_detect_utf8_bom() {
    let mut bytes = vec![0xEF, 0xBB, 0xBF];
    bytes.extend_from_slice("内容".as_bytes());
    assert_eq!(encoding::detect(&bytes), TextEncoding::Utf8Bom);
}

#[test]
fn encoding_detect_utf16le_bom() {
    // encode() 输出自带 BOM，无需手动拼接
    let bytes = encoding::encode("测试文本", TextEncoding::Utf16Le);
    assert_eq!(encoding::detect(&bytes), TextEncoding::Utf16Le);
}

#[test]
fn encoding_roundtrip_utf16be_bom() {
    let original = "UTF-16 BE 往返测试";
    let bytes = encoding::encode(original, TextEncoding::Utf16Be);
    assert_eq!(&bytes[..2], &[0xFE, 0xFF]);
    let (decoded, enc) = encoding::decode(&bytes);
    assert_eq!(enc, TextEncoding::Utf16Be);
    assert_eq!(decoded, original);
}

#[test]
fn encoding_roundtrip_gb18030() {
    let original = "中文内容 Rust 重构测试";
    let bytes = encoding::encode(original, TextEncoding::Gb18030);
    assert_eq!(encoding::detect(&bytes), TextEncoding::Gb18030);
    let (decoded, _) = encoding::decode(&bytes);
    assert_eq!(decoded, original);
}

#[test]
fn encoding_roundtrip_utf16le_bom() {
    // 无 BOM 的 UTF-16 与 GB18030/Latin-1 难以可靠区分（与 Qt 版一致：需 BOM）
    let original = "UTF-16 往返测试";
    let bytes = encoding::encode(original, TextEncoding::Utf16Le);
    assert_eq!(&bytes[..2], &[0xFF, 0xFE]); // encode() 自动加 BOM
    let (decoded, enc) = encoding::decode(&bytes);
    assert_eq!(enc, TextEncoding::Utf16Le);
    assert_eq!(decoded, original);
}

#[test]
fn eol_detect() {
    assert_eq!(encoding::detect_eol("a\nb"), Eol::Lf);
    assert_eq!(encoding::detect_eol("a\r\nb"), Eol::Crlf);
    assert_eq!(encoding::detect_eol("a\rb"), Eol::Cr);
}

#[test]
fn atomic_write_roundtrip() {
    let dir = std::env::temp_dir().join("hutamu_test");
    std::fs::create_dir_all(&dir).unwrap();
    let path = dir.join("atomic.txt");
    let bytes = encoding::encode("原子写入测试", TextEncoding::Utf8);
    encoding::atomic_write(path.to_str().unwrap(), &bytes).unwrap();
    let read = std::fs::read_to_string(&path).unwrap();
    assert_eq!(read, "原子写入测试");
    std::fs::remove_file(&path).ok();
}

#[test]
fn markdown_headings() {
    let md = "# 标题一\n\n正文\n## 子标题\n```rust\n# 不是标题\n```\n### 三级";
    let h = markdown::headings(md);
    assert_eq!(h.len(), 3);
    assert_eq!(h[0], (1, "标题一".to_string(), 1));
    assert_eq!(h[1], (2, "子标题".to_string(), 4));
    assert_eq!(h[2], (3, "三级".to_string(), 8));
}

#[test]
fn markdown_parse_blocks() {
    let md = "# 标题\n\n段落文字\n- 列表项\n```\ncode\n```\n---";
    let blocks = markdown::parse(md);
    let kinds: Vec<&str> = blocks
        .iter()
        .map(|b| match b.kind {
            markdown::MdKind::H1 => "h1",
            markdown::MdKind::Paragraph => "p",
            markdown::MdKind::Bullet => "li",
            markdown::MdKind::Code => "code",
            markdown::MdKind::Rule => "hr",
            _ => "other",
        })
        .collect();
    assert!(kinds.contains(&"h1"));
    assert!(kinds.contains(&"p"));
    assert!(kinds.contains(&"li"));
    assert!(kinds.contains(&"code"));
    assert!(kinds.contains(&"hr"));
}

#[test]
fn markdown_inline_strip() {
    let blocks = markdown::parse("**粗体** 和 `代码`");
    // 剥离标记后归入段落
    assert!(!blocks.is_empty());
    let text = &blocks[0].text;
    assert!(!text.contains("**"));
    assert!(!text.contains('`'));
}

#[test]
fn settings_roundtrip() {
    let dir = std::env::temp_dir().join("hutamu_test");
    std::fs::create_dir_all(&dir).unwrap();
    let path = dir.join("settings.json");
    let settings = hutaomu_editor::settings::Settings {
        sidebar_visible: false,
        font_size: 16.0,
        word_wrap: true,
        md_preview: true,
        theme_id: "graphite".into(),
        maximized: false,
        workspace: Some("G:/test".into()),
        recent: vec!["a.txt".into(), "b.txt".into()],
        bg_image: None,
        bg_opacity: 0.15,
        shell_trusted: Vec::new(),
    };
    let json = serde_json::to_string(&settings).unwrap();
    std::fs::write(&path, json).unwrap();
    let loaded = hutaomu_editor::settings::Settings::load_from(&path);
    assert!(!loaded.sidebar_visible);
    assert_eq!(loaded.font_size, 16.0);
    assert!(loaded.word_wrap);
    assert_eq!(loaded.theme_id, "graphite");
    assert_eq!(loaded.recent.len(), 2);
    std::fs::remove_file(&path).ok();
}

#[test]
fn file_save_roundtrip_with_eol() {
    let dir = std::env::temp_dir().join("hutamu_test");
    std::fs::create_dir_all(&dir).unwrap();
    let path = dir.join("eol_crlf.txt");
    // CRLF 内容保存后逐字节一致
    let original = "line1\r\nline2\r\n";
    let bytes = encoding::encode(original, TextEncoding::Utf8);
    encoding::atomic_write(path.to_str().unwrap(), &bytes).unwrap();
    let read_bytes = std::fs::read(&path).unwrap();
    assert_eq!(read_bytes, original.as_bytes());
    let (text, _) = encoding::decode(&read_bytes);
    assert_eq!(encoding::detect_eol(&text), Eol::Crlf);
    std::fs::remove_file(&path).ok();
}
