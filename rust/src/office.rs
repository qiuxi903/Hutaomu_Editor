// Hutaomu Editor (Rust) - Office/PDF 只读查看器（对标 Qt 版 viewers/：docx/xlsx/pptx/pdf）。
// docx/xlsx/pptx = ZIP + OOXML 解析（quick-xml）；pdf = pdfium 动态加载渲染。
// SPDX-License-Identifier: AGPL-3.0-only
use quick_xml::events::Event;
use quick_xml::Reader;

/// 查看器文档（Tab.viewer）
#[derive(Clone)]
pub enum ViewerDoc {
    Docx(DocxDoc),
    Xlsx(XlsxDoc),
    Pptx(PptxDoc),
    /// bytes + 当前页 + 渲染缓存（按 (页, 宽) 缓存）
    Pdf(PdfDoc),
    /// 音视频：egui 内嵌播放不可行（需 Media Foundation COM），
    /// v1 以卡片呈现并交由系统播放器打开（Qt 用 MF 渲染，此处为降级实现）
    Media(MediaDoc),
}

#[derive(Clone)]
pub struct MediaDoc {
    pub path: String,
    pub video: bool,
}

#[derive(Clone)]
pub struct PdfDoc {
    pub bytes: Vec<u8>,
    pub page: usize,
    pub page_count: usize,
    pub target_width: u32,
    pub tex: std::collections::HashMap<(usize, u32), egui::TextureHandle>,
    /// pdfium.dll 定位失败等原因
    pub error: Option<String>,
}

// ---------------- docx ----------------

#[derive(Clone, Debug)]
pub enum DocxBlock {
    Paragraph {
        runs: Vec<DocxRun>,
        heading: u8, // 0 = 正文
        list: bool,
        align_center: bool,
    },
    Table {
        rows: Vec<Vec<String>>,
    },
}

#[derive(Clone, Debug)]
pub struct DocxRun {
    pub text: String,
    pub bold: bool,
    pub italic: bool,
    pub sz: Option<f32>, // 磅值
}

#[derive(Default, Clone)]
pub struct DocxDoc {
    pub blocks: Vec<DocxBlock>,
}

/// XML 实体反转义
fn xml_unescape(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut rest = s;
    while let Some(pos) = rest.find('&') {
        out.push_str(&rest[..pos]);
        let tail = &rest[pos..];
        if let Some(semi) = tail.find(';') {
            let ent = &tail[1..semi];
            let rep = match ent {
                "amp" => Some('&'),
                "lt" => Some('<'),
                "gt" => Some('>'),
                "quot" => Some('"'),
                "apos" => Some('\''),
                _ => {
                    // 数字实体
                    if let Some(hex) = ent.strip_prefix("#x").or_else(|| ent.strip_prefix("#X")) {
                        u32::from_str_radix(hex, 16).ok().and_then(char::from_u32)
                    } else if let Some(dec) = ent.strip_prefix('#') {
                        dec.parse::<u32>().ok().and_then(char::from_u32)
                    } else {
                        None
                    }
                }
            };
            match rep {
                Some(c) => {
                    out.push(c);
                    rest = &tail[semi + 1..];
                }
                None => {
                    out.push('&');
                    rest = &tail[1..];
                }
            }
        } else {
            out.push('&');
            rest = &tail[1..];
        }
    }
    out.push_str(rest);
    out
}

fn wxml_events(xml: &str) -> impl Iterator<Item = (String, bool, String)> {
    // (标签名, 是否结束标签, 属性串或文本)
    let mut reader = Reader::from_str(xml);
    reader.config_mut().trim_text(false);
    let mut out = Vec::new();
    loop {
        match reader.read_event() {
            Ok(Event::Start(e)) => {
                let attrs = e
                    .attributes()
                    .flatten()
                    .map(|a| format!("{}={}", a.key.as_ref(), a.value.as_ref()))
                    .collect::<Vec<_>>()
                    .join(" ");
                out.push((e.name().as_ref().to_string(), false, attrs));
            }
            Ok(Event::Empty(e)) => {
                let attrs = e
                    .attributes()
                    .flatten()
                    .map(|a| format!("{}={}", a.key.as_ref(), a.value.as_ref()))
                    .collect::<Vec<_>>()
                    .join(" ");
                let name = e.name().as_ref().to_string();
                out.push((name.clone(), false, attrs));
                out.push((name, true, String::new()));
            }
            Ok(Event::End(e)) => {
                out.push((e.name().as_ref().to_string(), true, String::new()));
            }
            Ok(Event::Text(t)) => {
                out.push((
                    "#text".into(),
                    false,
                    xml_unescape(&t),
                ));
            }
            // &amp; &lt; 等实体引用（quick-xml 0.42 单独派发，内容不含 & ;）
            Ok(Event::GeneralRef(r)) => {
                let name = r.as_ref();
                let rep = match name {
                    "amp" => "&",
                    "lt" => "<",
                    "gt" => ">",
                    "quot" => "\"",
                    "apos" => "'",
                    _ => "",
                };
                out.push(("#text".into(), false, rep.to_string()));
            }
            Ok(Event::Eof) => break,
            Err(_) => break,
            _ => {}
        }
    }
    out.into_iter()
}

/// 从 zip 中按名称读取文件（跳过目录）
fn zip_read<'a>(zip: &'a mut zip::ZipArchive<std::io::BufReader<std::io::Cursor<Vec<u8>>>>, name: &str) -> Option<Vec<u8>> {
    let i = (0..zip.len()).find(|&i| zip.by_index(i).ok().map(|f| f.name().to_string()) == Some(name.to_string()))?;
    let mut f = zip.by_index(i).ok()?;
    let mut buf = Vec::new();
    std::io::Read::read_to_end(&mut f, &mut buf).ok()?;
    Some(buf)
}

fn zip_open(bytes: &[u8]) -> Result<zip::ZipArchive<std::io::BufReader<std::io::Cursor<Vec<u8>>>>, String> {
    let cur = std::io::Cursor::new(bytes.to_vec());
    zip::ZipArchive::new(std::io::BufReader::new(cur)).map_err(|_| "无法解压文档（不是有效的 OOXML 包）".to_string())
}

pub fn parse_docx(bytes: &[u8]) -> Result<DocxDoc, String> {
    let mut zip = zip_open(bytes)?;
    let xml_bytes = zip_read(&mut zip, "word/document.xml").ok_or("docx 中缺少 word/document.xml")?;
    let xml = String::from_utf8_lossy(&xml_bytes).to_string();

    let mut doc = DocxDoc::default();
    // 顶层扫描：<w:p> 段落与 <w:tbl> 表格（不递归嵌套表格内段落）
    let events: Vec<(String, bool, String)> = wxml_events(&xml).collect();
    let mut i = 0usize;
    let mut para_depth = 0i32; // <w:p> 嵌套深度（表格内段落不计为独立块）
    let mut tbl_depth = 0i32;
    let mut cur_para: Option<(u8, bool, bool, Vec<DocxRun>)> = None;
    let mut cur_run: Option<DocxRun> = None;
    let mut cur_table: Option<Vec<Vec<String>>> = None;
    let mut cur_row: Option<Vec<String>> = None;
    let mut cur_cell = String::new();

    while i < events.len() {
        let (name, is_end, attrs) = events[i].clone();
        match (name.as_str(), is_end) {
            ("w:tbl", false) if tbl_depth == 0 => {
                // 先收掉未闭合段落
                if let Some((h, l, c, runs)) = cur_para.take() {
                    doc.blocks.push(DocxBlock::Paragraph { runs, heading: h, list: l, align_center: c });
                }
                tbl_depth = 1;
                para_depth = 0;
                cur_table = Some(Vec::new());
            }
            ("w:tbl", true) => {
                tbl_depth = 0;
                if let Some(t) = cur_table.take() {
                    doc.blocks.push(DocxBlock::Table { rows: t });
                }
            }
            ("w:tr", false) if tbl_depth > 0 => cur_row = Some(Vec::new()),
            ("w:tr", true) if tbl_depth > 0 => {
                if let (Some(row), Some(t)) = (cur_row.take(), cur_table.as_mut()) {
                    t.push(row);
                }
            }
            ("w:tc", false) if tbl_depth > 0 => cur_cell = String::new(),
            ("w:tc", true) if tbl_depth > 0 => {
                if let (Some(row), Some(t)) = (cur_row.as_mut(), cur_table.as_mut()) {
                    row.push(cur_cell.trim().to_string());
                    let _ = t;
                }
            }
            ("w:p", false) => {
                if tbl_depth > 0 {
                    // 表格内段落：文本进 cell
                    para_depth += 1;
                } else {
                    para_depth = 1;
                    cur_para = Some((0, false, false, Vec::new()));
                }
            }
            ("w:p", true) => {
                if tbl_depth > 0 && para_depth > 0 {
                    para_depth -= 1;
                    if para_depth > 0 {
                        // 同一单元格内的多段落：以空格分隔
                        cur_cell.push(' ');
                    }
                    // 单元格文本由 /w:tc 统一收行
                } else if let Some((heading, list, align_center, mut runs)) = cur_para.take() {
                    if let Some(r) = cur_run.take() {
                        if !r.text.is_empty() {
                            runs.push(r);
                        }
                    }
                    doc.blocks.push(DocxBlock::Paragraph { runs, heading, list, align_center });
                }
            }
            ("w:r", false) => {
                cur_run = Some(DocxRun { text: String::new(), bold: false, italic: false, sz: None });
            }
            ("w:r", true) => {
                // 注意：不能写成 (Some(r), Some(p)) = (take(), as_mut()) 元组——
                // take() 无论匹配与否都会执行，吞掉文本
                if let Some(r) = cur_run.take() {
                    if let Some(p) = cur_para.as_mut() {
                        if !r.text.is_empty() {
                            p.3.push(r);
                        }
                    } else if tbl_depth > 0 && !r.text.is_empty() {
                        cur_cell.push_str(&r.text);
                    }
                }
            }
            ("w:b", false) => {
                if let Some(r) = cur_run.as_mut() {
                    r.bold = true;
                }
            }
            ("w:i", false) => {
                if let Some(r) = cur_run.as_mut() {
                    r.italic = true;
                }
            }
            ("w:sz", false) => {
                // <w:sz w:val="44"/> 半磅
                if let Some(v) = attr_value(&attrs, "w:val") {
                    if let Ok(half) = v.parse::<f32>() {
                        if let Some(r) = cur_run.as_mut() {
                            r.sz = Some(half / 2.0);
                        }
                    }
                }
            }
            ("#text", false) => {
                let decoded = xml_unescape(&attrs);
                if let Some(r) = cur_run.as_mut() {
                    r.text.push_str(&decoded);
                } else if tbl_depth > 0 && para_depth > 0 {
                    cur_cell.push_str(&decoded);
                }
            }
            ("w:jc", false) => {
                if let Some(v) = attr_value(&attrs, "w:val") {
                    if v == "center" {
                        if let Some(p) = cur_para.as_mut() {
                            p.2 = true;
                        }
                    }
                }
            }
            ("w:pStyle", false) => {
                if let Some(v) = attr_value(&attrs, "w:val") {
                    let h = if v.eq_ignore_ascii_case("Heading1") || v.eq_ignore_ascii_case("Ttulo1") {
                        1
                    } else if v.eq_ignore_ascii_case("Heading2") {
                        2
                    } else if v.eq_ignore_ascii_case("Heading3") {
                        3
                    } else {
                        0
                    };
                    if h > 0 {
                        if let Some(p) = cur_para.as_mut() {
                            p.0 = h;
                        }
                    }
                }
            }
            ("w:numPr", false) => {
                if let Some(p) = cur_para.as_mut() {
                    p.1 = true;
                }
            }
            ("w:t", false) => {
                // 文本由全局 #text / GeneralRef 事件统一收集
            }
            _ => {}
        }
        i += 1;
    }
    Ok(doc)
}

fn attr_value(attrs: &str, key: &str) -> Option<String> {
    for part in attrs.split(' ') {
        let part = part.trim();
        if let Some(rest) = part.strip_prefix(&format!("{}=", key)) {
            return Some(rest.trim_matches('"').to_string());
        }
    }
    None
}

// ---------------- xlsx ----------------

#[derive(Default, Clone)]
pub struct XlsxDoc {
    pub sheets: Vec<XlsxSheet>,
    pub active: usize,
}

#[derive(Clone)]
pub struct XlsxSheet {
    pub name: String,
    pub cells: Vec<Vec<String>>, // [row][col]
}

pub fn parse_xlsx(bytes: &[u8]) -> Result<XlsxDoc, String> {
    let mut zip = zip_open(bytes)?;
    // sharedStrings
    let mut shared: Vec<String> = Vec::new();
    if let Some(ss) = zip_read(&mut zip, "xl/sharedStrings.xml") {
        let xml = String::from_utf8_lossy(&ss).to_string();
        let events: Vec<(String, bool, String)> = wxml_events(&xml).collect();
        let mut cur = String::new();
        let mut in_si = false;
        let mut in_t = false;
        for (n, e, text) in &events {
            match (n.as_str(), *e) {
                ("si", false) => {
                    in_si = true;
                    cur = String::new();
                }
                ("si", true) => {
                    in_si = false;
                    shared.push(std::mem::take(&mut cur));
                }
                ("t", false) => in_t = true,
                ("t", true) => in_t = false,
                ("#text", false) if in_si && in_t => cur.push_str(&xml_unescape(text)),
                _ => {}
            }
        }
    }
    // workbook sheets（name + r:id）
    let wb = zip_read(&mut zip, "xl/workbook.xml").ok_or("xlsx 中缺少 xl/workbook.xml")?;
    let xml = String::from_utf8_lossy(&wb).to_string();
    let events: Vec<(String, bool, String)> = wxml_events(&xml).collect();
    let mut sheet_refs: Vec<(String, String)> = Vec::new();
    for (n, e, attrs) in &events {
        if n == "sheet" && !*e {
            let name = attr_value(attrs, "name").unwrap_or_default();
            let rid = attr_value(attrs, "r:id")
                .or_else(|| attr_value(attrs, "id"))
                .unwrap_or_default();
            sheet_refs.push((name, rid));
        }
    }
    // rels：rId → 目标
    let mut rels: std::collections::HashMap<String, String> = Default::default();
    if let Some(r) = zip_read(&mut zip, "xl/_rels/workbook.xml.rels") {
        let xml = String::from_utf8_lossy(&r).to_string();
        for (n, e, attrs) in wxml_events(&xml) {
            if n == "Relationship" && !e {
                if let (Some(id), Some(target)) = (attr_value(&attrs, "Id"), attr_value(&attrs, "Target")) {
                    rels.insert(id, target);
                }
            }
        }
    }
    let mut doc = XlsxDoc::default();
    for (name, rid) in sheet_refs {
        let target = rels.get(&rid).cloned().unwrap_or_default();
        let path = format!("xl/{}", target.trim_start_matches('/'));
        let cells = zip_read(&mut zip, &path)
            .map(|sb| parse_sheet(&String::from_utf8_lossy(&sb), &shared))
            .unwrap_or_default();
        doc.sheets.push(XlsxSheet { name, cells });
    }
    Ok(doc)
}

/// (row, col) 从 "A1" / "AB12"
fn cell_ref(r: &str) -> (usize, usize) {
    let mut col = 0usize;
    let mut row = 0usize;
    let mut in_letters = true;
    for ch in r.chars() {
        if ch.is_ascii_alphabetic() && in_letters {
            col = col * 26 + (ch.to_ascii_uppercase() as usize - 'A' as usize + 1);
        } else {
            in_letters = false;
            row = row * 10 + ch.to_digit(10).unwrap_or(0) as usize;
        }
    }
    (row.saturating_sub(1), col.saturating_sub(1))
}

fn parse_sheet(xml: &str, shared: &[String]) -> Vec<Vec<String>> {
    let events: Vec<(String, bool, String)> = wxml_events(xml).collect();
    let mut grid: Vec<Vec<String>> = Vec::new();
    let mut cur_cell_ref = String::new();
    let mut cur_type = String::new();
    let mut cur_inline = String::new();
    let mut in_inline = false;
    let mut pending: Option<(usize, usize, String, String)> = None; // (row, col, type, value)
    let mut push = |grid: &mut Vec<Vec<String>>, row: usize, col: usize, mut text: String| {
        while grid.len() <= row {
            grid.push(Vec::new());
        }
        while grid[row].len() <= col {
            grid[row].push(String::new());
        }
        if text.contains('\n') {
            text = text.replace('\r', "");
        }
        grid[row][col] = text;
    };
    for (n, e, attrs) in &events {
        match (n.as_str(), *e) {
            ("c", false) => {
                cur_cell_ref = attr_value(attrs, "r").unwrap_or_default();
                cur_type = attr_value(attrs, "t").unwrap_or_default();
                cur_inline = String::new();
                in_inline = false;
            }
            ("c", true) => {
                if let Some((row, col, t, v)) = pending.take() {
                    let text = match t.as_str() {
                        "s" => shared
                            .get(v.parse::<usize>().unwrap_or(usize::MAX))
                            .cloned()
                            .unwrap_or(v),
                        "inlineStr" => cur_inline.clone(),
                        _ => v,
                    };
                    if !text.is_empty() {
                        push(&mut grid, row, col, text);
                    }
                }
            }
            ("is", false) => in_inline = true,
            ("is", true) => in_inline = false,
            ("v", false) => {}
            ("v", true) => {}
            ("#text", false) => {
                let decoded = xml_unescape(attrs);
                if in_inline {
                    cur_inline.push_str(&decoded);
                } else {
                    let (row, col) = cell_ref(&cur_cell_ref);
                    pending = Some((row, col, cur_type.clone(), decoded));
                }
            }
            _ => {}
        }
    }
    grid
}

// ---------------- pptx ----------------

#[derive(Clone)]
pub struct PptxDoc {
    pub slides: Vec<PptxSlide>,
    pub active: usize,
}

#[derive(Clone)]
pub struct PptxSlide {
    pub shapes: Vec<PptxShape>,
}

#[derive(Clone)]
pub struct PptxShape {
    /// 位置与尺寸（0..1 归一化到 16:9 画布）
    pub x: f32,
    pub y: f32,
    pub w: f32,
    pub h: f32,
    pub text: String,
    pub font_px: f32,
}

pub fn parse_pptx(bytes: &[u8]) -> Result<PptxDoc, String> {
    let mut zip = zip_open(bytes)?;
    // 收集 ppt/slides/slideN.xml 并按编号排序
    let mut slide_names: Vec<(usize, String)> = Vec::new();
    for i in 0..zip.len() {
        let name = zip.by_index(i).ok().map(|f| f.name().to_string()).unwrap_or_default();
        if let Some(rest) = name.strip_prefix("ppt/slides/slide") {
            if let Some(num) = rest.split('.').next() {
                if let Ok(n) = num.parse::<usize>() {
                    slide_names.push((n, name));
                }
            }
        }
    }
    slide_names.sort();
    let mut doc = PptxDoc { slides: Vec::new(), active: 0 };
    for (_, sn) in slide_names {
        let Some(sb) = zip_read(&mut zip, &sn) else { continue };
        let xml = String::from_utf8_lossy(&sb).to_string();
        let events: Vec<(String, bool, String)> = wxml_events(&xml).collect();
        let mut shapes: Vec<PptxShape> = Vec::new();
        let mut cur_shape: Option<(f32, f32, f32, f32, String, f32)> = None;
        let mut in_text = false;
        let mut cur_sz: f32 = 18.0;
        for (n, e, attrs) in &events {
            match (n.as_str(), *e) {
                ("p:sp", false) => cur_shape = Some((0.0, 0.0, 0.4, 0.12, String::new(), 18.0)),
                ("a:p", false) => {
                    if let Some(s) = cur_shape.as_mut() {
                        if !s.4.is_empty() {
                            s.4.push_str("\n");
                            s.4.push_str("\n");
                        }
                    }
                }
                ("p:sp", true) => {
                    if let Some((x, y, w, h, text, sz)) = cur_shape.take() {
                        if !text.trim().is_empty() {
                            shapes.push(PptxShape { x, y, w, h, text, font_px: sz });
                        }
                    }
                }
                ("a:off", false) => {
                    if let (Some(s), Some(emu_x), Some(emu_y)) =
                        (cur_shape.as_mut(), attr_value(attrs, "x"), attr_value(attrs, "y"))
                    {
                        s.0 = emu_x.parse::<f32>().unwrap_or(0.0) / 12192000.0; // 16:9 画布宽 EMU
                        s.1 = emu_y.parse::<f32>().unwrap_or(0.0) / 6858000.0;
                    }
                }
                ("a:ext", false) => {
                    if let (Some(s), Some(emu_w), Some(emu_h)) =
                        (cur_shape.as_mut(), attr_value(attrs, "cx"), attr_value(attrs, "cy"))
                    {
                        s.2 = emu_w.parse::<f32>().unwrap_or(0.0) / 12192000.0;
                        s.3 = emu_h.parse::<f32>().unwrap_or(0.0) / 6858000.0;
                    }
                }
                ("a:rPr", false) => {
                    if let Some(sz) = attr_value(attrs, "sz") {
                        if let Ok(hpt) = sz.parse::<f32>() {
                            cur_sz = hpt / 100.0; // 百分之一磅 → 磅
                        }
                    }
                }
                ("a:t", false) => in_text = true,
                ("a:t", true) => in_text = false,
                ("#text", false) if in_text => {
                    if let Some(s) = cur_shape.as_mut() {
                        s.4.push_str(&xml_unescape(attrs));
                        s.5 = s.5.max(cur_sz * (96.0 / 72.0));
                    }
                }
                _ => {}
            }
        }
        doc.slides.push(PptxSlide { shapes });
    }
    Ok(doc)
}

// ---------------- pdf ----------------

/// 在候选路径中定位 pdfium.dll
pub fn find_pdfium_dll() -> Option<std::path::PathBuf> {
    let mut candidates: Vec<std::path::PathBuf> = Vec::new();
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            candidates.push(dir.join("pdfium.dll"));
            // 开发环境：rust/target/debug|release → 仓库 third_party
            for up in ["../../..", "../../../.."] {
                candidates.push(dir.join(up).join("third_party").join("pdfium").join("bin").join("pdfium.dll"));
            }
        }
    }
    candidates.push(std::path::PathBuf::from("third_party/pdfium/bin/pdfium.dll"));
    candidates.into_iter().map(|p| std::fs::canonicalize(&p).unwrap_or(p)).find(|p| p.is_file())
}

/// 渲染 PDF 单页为 RGBA 缓冲
pub fn render_pdf_page(
    bytes: &[u8],
    page_index: usize,
    target_width: u32,
) -> Result<image::RgbaImage, String> {
    use pdfium_render::prelude::*;
    let dll = find_pdfium_dll().ok_or(
        "未找到 pdfium.dll（需置于程序目录或 third_party/pdfium/bin/）".to_string(),
    )?;
    let bindings = Pdfium::bind_to_library(dll).map_err(|e| format!("加载 pdfium 失败：{}", e))?;
    let pdfium = Pdfium::new(bindings);
    let doc = pdfium
        .load_pdf_from_byte_slice(bytes, None)
        .map_err(|e| format!("打开 PDF 失败：{}", e))?;
    let page = doc
        .pages()
        .get(page_index as PdfPageIndex)
        .map_err(|e| format!("读取第 {} 页失败：{}", page_index + 1, e))?;
    let config = PdfRenderConfig::new().set_target_width((target_width.max(200)) as i32);
    let dyn_img = page
        .render_with_config(&config)
        .map_err(|e| format!("渲染失败：{}", e))?
        .as_image();
    Ok(dyn_img.to_rgba8())
}

pub fn pdf_page_count(bytes: &[u8]) -> Result<usize, String> {
    use pdfium_render::prelude::*;
    let dll = find_pdfium_dll().ok_or("未找到 pdfium.dll".to_string())?;
    let bindings = Pdfium::bind_to_library(dll).map_err(|e| format!("加载 pdfium 失败：{}", e))?;
    let pdfium = Pdfium::new(bindings);
    let doc = pdfium
        .load_pdf_from_byte_slice(bytes, None)
        .map_err(|e| format!("打开 PDF 失败：{}", e))?;
    Ok(doc.pages().len() as usize)
}

/// 按扩展名解析为查看器文档
pub fn parse_viewer(ext: &str, bytes: &[u8]) -> Result<ViewerDoc, String> {
    match ext {
        "docx" => Ok(ViewerDoc::Docx(parse_docx(bytes)?)),
        "xlsx" | "xlsm" => Ok(ViewerDoc::Xlsx(parse_xlsx(bytes)?)),
        "pptx" => Ok(ViewerDoc::Pptx(parse_pptx(bytes)?)),
        "pdf" => {
            let page_count = pdf_page_count(bytes).unwrap_or(0);
            Ok(ViewerDoc::Pdf(PdfDoc {
                bytes: bytes.to_vec(),
                page: 0,
                page_count,
                target_width: 1400,
                tex: Default::default(),
                error: None,
            }))
        }
        "mp4" | "mkv" | "avi" | "mov" | "wmv" | "flv" | "webm" | "m4v" => Ok(ViewerDoc::Media(
            MediaDoc {
                path: String::from_utf8_lossy(bytes).to_string(),
                video: true,
            },
        )),
        "mp3" | "wav" | "flac" | "ogg" | "m4a" | "wma" | "aac" => Ok(ViewerDoc::Media(
            MediaDoc {
                path: String::from_utf8_lossy(bytes).to_string(),
                video: false,
            },
        )),
        _ => Err("不支持的格式".into()),
    }
}

#[cfg(test)]
mod office_tests {
    use super::*;
    use std::io::Write as _;

    const DOC_XML: &str = r#"<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body>
    <w:p><w:pPr><w:pStyle w:val="Heading1"/></w:pPr><w:r><w:t>标题</w:t></w:r></w:p>
    <w:p><w:r><w:rPr><w:b/></w:rPr><w:t>粗</w:t></w:r><w:r><w:t>普通</w:t></w:r></w:p>
    <w:tbl><w:tr><w:tc><w:p><w:r><w:t>甲</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>乙 &amp; 丙</w:t></w:r></w:p></w:tc></w:tr></w:tbl>
    <w:p><w:r><w:t>实体 &lt; 测试</w:t></w:r></w:p>
    </w:body></w:document>"#;

    #[test]
    fn docx_parse_blocks_and_entities() {
        let mut z = zip::ZipWriter::new(std::io::Cursor::new(Vec::new()));
        let opt =
            zip::write::SimpleFileOptions::default().compression_method(zip::CompressionMethod::Stored);
        z.start_file("word/document.xml", opt).unwrap();
        z.write_all(DOC_XML.as_bytes()).unwrap();
        let bytes = z.finish().unwrap().into_inner();
        let doc = parse_docx(&bytes).unwrap();
        assert_eq!(doc.blocks.len(), 4);
        match &doc.blocks[0] {
            DocxBlock::Paragraph { runs, heading, .. } => {
                assert_eq!(*heading, 1);
                assert_eq!(runs[0].text, "标题");
            }
            _ => panic!("expected paragraph"),
        }
        match &doc.blocks[2] {
            DocxBlock::Table { rows } => {
                assert_eq!(rows, &vec![vec!["甲".to_string(), "乙 & 丙".to_string()]]);
            }
            _ => panic!("expected table"),
        }
        match &doc.blocks[3] {
            DocxBlock::Paragraph { runs, .. } => assert_eq!(runs[0].text, "实体 < 测试"),
            _ => panic!("expected paragraph"),
        }
    }

    #[test]
    fn xlsx_parse_cells() {
        let mut z = zip::ZipWriter::new(std::io::Cursor::new(Vec::new()));
        let opt =
            zip::write::SimpleFileOptions::default().compression_method(zip::CompressionMethod::Stored);
        z.start_file("xl/workbook.xml", opt).unwrap();
        z.write_all("<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets><sheet name=\"表一\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>".as_bytes()).unwrap();
        z.start_file("xl/_rels/workbook.xml.rels", opt).unwrap();
        z.write_all(r#"<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Target="worksheets/sheet1.xml"/></Relationships>"#.as_bytes()).unwrap();
        z.start_file("xl/sharedStrings.xml", opt).unwrap();
        z.write_all("<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><si><t>名称</t></si></sst>".as_bytes()).unwrap();
        z.start_file("xl/worksheets/sheet1.xml", opt).unwrap();
        z.write_all(r#"<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><row><c r="A1" t="s"><v>0</v></c><c r="B1"><v>42</v></c></row></worksheet>"#.as_bytes()).unwrap();
        let bytes = z.finish().unwrap().into_inner();
        let doc = parse_xlsx(&bytes).unwrap();
        assert_eq!(doc.sheets.len(), 1);
        assert_eq!(doc.sheets[0].name, "表一");
        assert_eq!(doc.sheets[0].cells, vec![vec!["名称".to_string(), "42".to_string()]]);
    }

    #[test]
    fn pptx_parse_slides() {
        let mut z = zip::ZipWriter::new(std::io::Cursor::new(Vec::new()));
        let opt =
            zip::write::SimpleFileOptions::default().compression_method(zip::CompressionMethod::Stored);
        z.start_file("ppt/slides/slide2.xml", opt).unwrap();
        z.write_all(r#"<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"><p:sp><p:spPr><a:off x="1219200" y="685800"/><a:ext cx="6096000" cy="685800"/></p:spPr><p:txBody><a:p><a:r><a:rPr sz="2400"/><a:t>第二页</a:t></a:r></a:p></p:txBody></p:sp></p:sld>"#.as_bytes()).unwrap();
        z.start_file("ppt/slides/slide1.xml", opt).unwrap();
        z.write_all(r#"<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"><p:sp><p:txBody><a:p><a:r><a:t>第一页</a:t></a:r></a:p></p:txBody></p:sp></p:sld>"#.as_bytes()).unwrap();
        let bytes = z.finish().unwrap().into_inner();
        let doc = parse_pptx(&bytes).unwrap();
        assert_eq!(doc.slides.len(), 2);
        assert_eq!(doc.slides[0].shapes[0].text, "第一页");
        assert_eq!(doc.slides[1].shapes[0].text, "第二页");
        assert!((doc.slides[1].shapes[0].x - 0.1).abs() < 0.01);
    }
}
