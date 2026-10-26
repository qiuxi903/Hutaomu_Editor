// Hutaomu Editor (Rust) - Markdown 预览渲染（pulldown-cmark → egui 富文本块）。
use egui::RichText;

pub struct MdBlock {
    pub kind: MdKind,
    pub text: String,
}

pub enum MdKind {
    H1,
    H2,
    H3,
    H4,
    Paragraph,
    Bullet,
    Numbered(u64),
    Quote,
    Code,
    Rule,
}

/// 解析 Markdown 为块序列（v1：标题/段落/列表/引用/代码块/分隔线）
pub fn parse(md: &str) -> Vec<MdBlock> {
    let mut blocks = Vec::new();
    let mut code = false;
    let mut code_text = String::new();
    let mut para = String::new();
    let mut in_quote = false;

    let flush_para = |blocks: &mut Vec<MdBlock>, para: &mut String| {
        if !para.trim().is_empty() {
            blocks.push(MdBlock {
                kind: MdKind::Paragraph,
                text: std::mem::take(para),
            });
        }
    };

    for line in md.lines() {
        let trimmed = line.trim();
        if trimmed.starts_with("```") {
            if code {
                blocks.push(MdBlock {
                    kind: MdKind::Code,
                    text: std::mem::take(&mut code_text),
                });
                code = false;
            } else {
                flush_para(&mut blocks, &mut para);
                code = true;
            }
            continue;
        }
        if code {
            code_text.push_str(line);
            code_text.push('\n');
            continue;
        }
        if trimmed.starts_with('#') {
            flush_para(&mut blocks, &mut para);
            let level = trimmed.chars().take_while(|c| *c == '#').count();
            let text = trimmed.trim_start_matches('#').trim().to_string();
            let kind = match level {
                1 => MdKind::H1,
                2 => MdKind::H2,
                3 => MdKind::H3,
                _ => MdKind::H4,
            };
            blocks.push(MdBlock { kind, text });
            continue;
        }
        if trimmed.starts_with("- ") || trimmed.starts_with("* ") {
            flush_para(&mut blocks, &mut para);
            blocks.push(MdBlock {
                kind: MdKind::Bullet,
                text: trimmed[2..].to_string(),
            });
            continue;
        }
        if let Some(rest) = trimmed.strip_prefix("> ") {
            in_quote = true;
            blocks.push(MdBlock {
                kind: MdKind::Quote,
                text: rest.to_string(),
            });
            continue;
        }
        if trimmed == "---" || trimmed == "***" {
            flush_para(&mut blocks, &mut para);
            blocks.push(MdBlock {
                kind: MdKind::Rule,
                text: String::new(),
            });
            continue;
        }
        if let Some((num, rest)) = split_numbered(trimmed) {
            flush_para(&mut blocks, &mut para);
            blocks.push(MdBlock {
                kind: MdKind::Numbered(num),
                text: rest.to_string(),
            });
            continue;
        }
        if trimmed.is_empty() {
            flush_para(&mut blocks, &mut para);
            in_quote = false;
            continue;
        }
        // 引用块内的续行
        if in_quote {
            if let Some(last) = blocks.last_mut() {
                if matches!(last.kind, MdKind::Quote) {
                    last.text.push(' ');
                    last.text.push_str(trimmed);
                    continue;
                }
            }
        }
        // 行内格式（粗体/斜体/行内代码 → 直接文本，v1 去标记）
        para.push_str(&inline_strip(trimmed));
        para.push(' ');
    }
    if code && !code_text.is_empty() {
        blocks.push(MdBlock {
            kind: MdKind::Code,
            text: code_text,
        });
    }
    flush_para(&mut blocks, &mut para);
    blocks
}

fn split_numbered(line: &str) -> Option<(u64, &str)> {
    let dot = line.find(". ")?;
    let num: u64 = line[..dot].parse().ok()?;
    Some((num, line[dot + 2..].trim()))
}

/// 行内格式剥离：**粗体**、*斜体*、`代码` → 纯文本（v1）
fn inline_strip(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut chars = s.chars().peekable();
    while let Some(c) = chars.next() {
        match c {
            '`' | '*' | '_' => continue,
            '[' => {
                // [text](url) → text
                let rest: String = chars.clone().collect();
                if let Some(close) = rest.find(']') {
                    let url_part = &rest[close + 1..];
                    if let Some(url) = url_part.strip_prefix('(') {
                        if let Some(end) = url.find(')') {
                            out.push_str(&rest[..close]);
                            for _ in 0..(close + 1 + url.len()) {
                                chars.next();
                            }
                            continue;
                        }
                    }
                }
                out.push(c);
            }
            _ => out.push(c),
        }
    }
    out
}

/// 预览块 → egui 富文本
pub fn rich(block: &MdBlock, fg: egui::Color32, accent: egui::Color32, muted: egui::Color32) -> RichText {
    match &block.kind {
        MdKind::H1 => RichText::new(&block.text).size(24.0).strong().color(fg),
        MdKind::H2 => RichText::new(&block.text).size(20.0).strong().color(fg),
        MdKind::H3 => RichText::new(&block.text).size(17.0).strong().color(fg),
        MdKind::H4 => RichText::new(&block.text).size(15.0).strong().color(fg),
        MdKind::Paragraph => RichText::new(&block.text).size(14.5).color(fg),
        MdKind::Bullet => RichText::new(format!("•  {}", block.text)).size(14.5).color(fg),
        MdKind::Numbered(n) => {
            RichText::new(format!("{}.  {}", n, block.text)).size(14.5).color(fg)
        }
        MdKind::Quote => RichText::new(format!("▎ {}", block.text))
            .size(14.0)
            .color(accent),
        MdKind::Code => RichText::new(&block.text).size(13.0).color(muted),
        MdKind::Rule => RichText::new("─────────────────────").size(12.0).color(muted),
    }
}

/// 从 Markdown 提取标题（大纲面板用）
pub fn headings(md: &str) -> Vec<(u8, String, usize)> {
    let mut out = Vec::new();
    let mut in_code = false;
    for (i, line) in md.lines().enumerate() {
        let t = line.trim();
        if t.starts_with("```") {
            in_code = !in_code;
            continue;
        }
        if in_code || !t.starts_with('#') {
            continue;
        }
        let level = t.chars().take_while(|c| *c == '#').count() as u8;
        if level <= 6 {
            out.push((level, t.trim_start_matches('#').trim().to_string(), i + 1));
        }
    }
    out
}
