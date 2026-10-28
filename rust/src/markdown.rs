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

// ---------------- 表格（原位渲染用，GFM 管道表格） ----------------

/// 解析出的 Markdown 表格
#[derive(Debug, PartialEq)]
pub struct MdTable {
    pub header: Vec<String>,
    pub rows: Vec<Vec<String>>,
}

/// 行是否为表格行（| ... |）
fn is_table_line(line: &str) -> bool {
    let t = line.trim();
    t.starts_with('|') && t.ends_with('|') && t.len() >= 2
}

/// 行是否为分隔行（| --- | :---: |）
fn is_sep_line(line: &str) -> bool {
    let t = line.trim().trim_matches('|');
    !t.is_empty() && t.split('|').all(|c| {
        let c = c.trim();
        !c.is_empty() && c.chars().all(|ch| ch == '-' || ch == ':')
    })
}

/// 找出全部表格块的行范围 [a, b)（0 基，b 不含）
pub fn table_blocks(md: &str) -> Vec<(usize, usize)> {
    let lines: Vec<&str> = md.lines().collect();
    let mut out = Vec::new();
    let mut in_code = false;
    let mut start: Option<usize> = None;
    for (i, line) in lines.iter().enumerate() {
        let t = line.trim();
        if t.starts_with("```") {
            in_code = !in_code;
            if let Some(a) = start.take() {
                out.push((a, i));
            }
            continue;
        }
        if in_code {
            continue;
        }
        if is_table_line(line) {
            if start.is_none() {
                start = Some(i);
            }
        } else if let Some(a) = start.take() {
            out.push((a, i));
        }
    }
    if let Some(a) = start {
        out.push((a, lines.len()));
    }
    out.into_iter()
        .filter(|(a, b)| {
            // 至少两行且第二行是分隔行
            b - a >= 2 && is_sep_line(lines[a + 1])
        })
        .collect()
}

/// 解析表格块（含表头与分隔行的行序列）
pub fn parse_table(lines: &[&str]) -> Option<MdTable> {
    if lines.len() < 2 || !is_sep_line(lines[1]) {
        return None;
    }
    let split_row = |line: &str| -> Vec<String> {
        // 转义感知切分：'\|' → '|'、'\\' → '\'
        let inner = line.trim().trim_start_matches('|').trim_end_matches('|');
        let mut cells: Vec<String> = Vec::new();
        let mut cur = String::new();
        let mut it = inner.chars();
        while let Some(c) = it.next() {
            if c == '\\' {
                if let Some(n) = it.next() {
                    cur.push(n);
                } else {
                    cur.push(c);
                }
            } else if c == '|' {
                cells.push(cur.trim().to_string());
                cur = String::new();
            } else {
                cur.push(c);
            }
        }
        cells.push(cur.trim().to_string());
        cells
    };
    Some(MdTable {
        header: split_row(lines[0]),
        rows: lines[2..].iter().map(|l| split_row(l)).collect(),
    })
}

/// 单元格显示宽度（中文按 2 计）
pub fn display_width(s: &str) -> usize {
    s.chars().map(|c| if c.len_utf8() > 1 { 2 } else { 1 }).sum()
}

#[cfg(test)]
mod table_tests {
    use super::*;

    #[test]
    fn table_blocks_and_parse() {
        let md = "前文\n\n| 名称 | 值 |\n| --- | :-: |\n| 甲 | 1 |\n| 乙\\|丙 | 2 |\n\n后文\n```rust\n| 代码块内不算 |\n```\n";
        let blocks = table_blocks(md);
        assert_eq!(blocks, vec![(2, 6)]);
        let lines: Vec<&str> = md.lines().collect();
        let t = parse_table(&lines[2..6]).unwrap();
        assert_eq!(t.header, vec!["名称", "值"]);
        assert_eq!(t.rows, vec![vec!["甲", "1"], vec!["乙|丙", "2"]]);
    }

    #[test]
    fn table_block_at_eof_and_non_table() {
        assert!(table_blocks("| 只有单行 |").is_empty());
        assert!(table_blocks("| 表头 |\n| 不是分隔 |").is_empty());
        let md = "| a | b |\n| --- | --- |";
        assert_eq!(table_blocks(md), vec![(0, 2)]);
    }

    #[test]
    fn display_width_cjk() {
        assert_eq!(display_width("ab"), 2);
        assert_eq!(display_width("中"), 2);
        assert_eq!(display_width("中a"), 3);
    }
}
