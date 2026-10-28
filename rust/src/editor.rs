// Hutaomu Editor (Rust) - 代码编辑器组件：syntect 高亮 + 行号列 + 当前行 + ScrollArea。
// SPDX-License-Identifier: AGPL-3.0-only
use egui::{
    text::LayoutJob, Color32, FontId, Pos2, Rect, TextEdit, TextFormat, Vec2,
};

pub struct EditorOutput {
    pub changed: bool,
    pub cursor_line: usize,
    /// 光标列（1 基，字符数）
    pub cursor_col: usize,
    /// 可视区域（含行号列），屏幕坐标
    pub rect: Rect,
    /// 滚动内容可视区（不含行号列），屏幕坐标
    pub text_rect: Rect,
    /// 上一帧 galley（屏幕坐标原点 galley_pos），供查找定位换算
    pub galley: std::sync::Arc<egui::text::Galley>,
    pub galley_pos: Pos2,
    pub scroll_y: f32,
    pub line_height: f32,
    pub focused: bool,
    pub id: egui::Id,
}

/// 语法高亮缓存：文本/语言/主题未变时不重复跑 syntect
pub struct HighlightCache {
    pub key: u64,
    pub lines: Vec<Vec<HighlightSpan>>,
}

#[derive(Clone)]
pub struct HighlightSpan {
    pub fg: Color32,
    pub bold: bool,
    pub italic: bool,
    pub text: String,
}

pub struct EditorTheme {
    pub editor_bg: Color32,
    pub editor_fg: Color32,
    pub current_line_bg: Color32,
    pub selection_bg: Color32,
    pub line_number_fg: Color32,
    pub line_number_active_fg: Color32,
    pub line_number_bg: Color32,
    /// 主题强调色（Markdown 实时预览装饰用）
    pub accent: Color32,
}

pub struct EditorParams<'a> {
    pub id: egui::Id,
    pub text: &'a mut String,
    pub font_size: f32,
    pub wrap: bool,
    pub language_name: &'a str,
    pub theme: &'a EditorTheme,
    pub cache: &'a mut Option<HighlightCache>,
    pub syntax: &'a syntect::parsing::SyntaxReference,
    pub syntax_set: &'a syntect::parsing::SyntaxSet,
    /// syntect 主题（随编辑器主题切换，主题标识参与缓存键）
    pub syntax_theme: &'a syntect::highlighting::Theme,
    pub syntax_theme_rev: u64,
    /// Markdown 实时预览装饰层（Qt 实时预览：标题/代码块/引用行内装饰）
    pub markdown_decor: bool,
    /// 查找跳转目标（字符索引区间），在滚动区内容坐标中居中
    pub scroll_to: Option<(usize, usize)>,
    /// 上一帧 galley（滚动目标字符 → 内容 y 坐标换算）
    pub prev_galley: Option<&'a std::sync::Arc<egui::text::Galley>>,
}

const LINE_HEIGHT_FACTOR: f32 = 1.42;
const GUTTER_WIDTH: f32 = 52.0;

pub fn show(
    ui: &mut egui::Ui,
    params: EditorParams<'_>,
) -> EditorOutput {
    let font_id = FontId::monospace(params.font_size);
    let line_height = params.font_size * LINE_HEIGHT_FACTOR;

    // ---- 高亮缓存 ----
    let text_hash = {
        use std::hash::{Hash, Hasher};
        let mut h = std::collections::hash_map::DefaultHasher::new();
        params.text.hash(&mut h);
        params.language_name.hash(&mut h);
        params.syntax_theme_rev.hash(&mut h);
        h.finish()
    };
    let need_refresh = match params.cache {
        Some(c) => c.key != text_hash,
        None => true,
    };
    if need_refresh {
        let lines = if params.markdown_decor {
            build_md_spans(params.text, &params.theme)
        } else {
            let mut highlighter =
                syntect::easy::HighlightLines::new(params.syntax, params.syntax_theme);
            let mut lines = Vec::new();
            for line in syntect::util::LinesWithEndings::from(params.text) {
                let spans: Vec<HighlightSpan> = highlighter
                    .highlight_line(line, params.syntax_set)
                    .unwrap_or_default()
                    .into_iter()
                    .map(|(style, chunk)| HighlightSpan {
                        fg: Color32::from_rgb(style.foreground.r, style.foreground.g, style.foreground.b),
                        bold: style.font_style.contains(syntect::highlighting::FontStyle::BOLD),
                        italic: style.font_style.contains(syntect::highlighting::FontStyle::ITALIC),
                        text: chunk.to_string(),
                    })
                    .collect();
                lines.push(spans);
            }
            lines
        };
        *params.cache = Some(HighlightCache {
            key: text_hash,
            lines,
        });
    }

    // ---- 行号列宽（按最大行数位数自适应）----
    let line_count = params.text.lines().count().max(1);
    let digits = line_count.to_string().len() as f32;
    let gutter_width = (GUTTER_WIDTH).max(18.0 + digits * params.font_size * 0.62);

    // ---- 滚动编辑区（直铺 TopDown 布局；行号列以固定覆盖层绘制在左缘留白上）----
    // 注：egui 0.32 的 ui.horizontal 只给子控件一行高度，不能用横向布局承载 ScrollArea，
    // 改为在 TextEdit 内容左侧加留白，行号画在留白区。
    // ---- 当前行高亮（画在文本之下：用上一帧光标行，随内容滚动）----
    let prev_cursor_line = egui::widgets::text_edit::TextEditState::load(ui.ctx(), params.id)
        .and_then(|st| st.cursor.char_range())
        .map(|r| {
            let idx = r.primary.index;
            // 上一帧的文本长度可能不同（正在输入），钳制到合法范围
            let idx = idx.min(params.text.len());
            params.text[..idx].matches('\n').count()
        })
        .unwrap_or(0);

    let margin_left = gutter_width.min(120.0) as i8;
    let scroll_out = egui::ScrollArea::vertical()
        .id_source(("ed_scroll", params.id))
        .auto_shrink([false, false])
        .show(ui, |ui| {
            // 查找跳转：按上一帧 galley 把目标行滚到视口中央
            if let (Some((sc, _)), Some(g)) = (params.scroll_to, params.prev_galley) {
                let y = g
                    .pos_from_cursor(egui::text::CCursor::new(sc))
                    .top();
                ui.scroll_to_rect(
                    Rect::from_min_size(Pos2::new(0.0, y), Vec2::new(1.0, line_height)),
                    Some(egui::Align::Center),
                );
            }
            // 当前行高亮（内容坐标，随滚动移动；文本之后绘制自然覆盖其上）
            let row_y = prev_cursor_line as f32 * line_height;
            ui.painter().rect_filled(
                Rect::from_min_max(
                    Pos2::new(0.0, row_y),
                    Pos2::new(ui.available_width(), row_y + line_height),
                ),
                0.0,
                params.theme.current_line_bg,
            );
            // Markdown 装饰底：代码块底色 + 引用左条（绘制在文本之下）
            if params.markdown_decor {
                paint_md_block_decor(ui, params.text, ui.available_width(), line_height, params.theme);
            }
            let out = egui::Frame::new()
                .inner_margin(egui::Margin {
                    left: margin_left,
                    ..Default::default()
                })
                .show(ui, |ui| {
                    TextEdit::multiline(params.text)
                        .id_source(params.id)
                        .font(font_id.clone())
                        .desired_width(ui.available_width())
                        .layouter(&mut |ui: &egui::Ui,
                                        buf: &dyn egui::TextBuffer,
                                        wrap_width: f32|
                         -> std::sync::Arc<egui::text::Galley> {
                            build_galley(
                                ui,
                                buf,
                                wrap_width,
                                params.cache.as_ref().map(|c| c.lines.as_slice()),
                                &font_id,
                                params.wrap,
                            )
                        })
                        .frame(false)
                        .show(ui)
                });
            out.inner
        });
    let text_edit_out = scroll_out.inner;
    let scroll_state = scroll_out.state;
    let text_rect = scroll_out.inner_rect;
    let gutter_rect = Rect::from_min_max(
        text_rect.left_top(),
        Pos2::new(text_rect.left() + gutter_width, text_rect.bottom()),
    );

    let view_rect = text_rect;
    let offset_y = scroll_state.offset.y;
    let galley = text_edit_out.galley.clone();
    let galley_pos = text_edit_out.galley_pos;
    let (cursor_line, cursor_col) = text_edit_out
        .cursor_range
        .map(|r| {
            let idx = r.primary.index.min(params.text.len());
            let line = params.text[..idx].matches('\n').count();
            let line_start = params.text[..idx].rfind('\n').map(|p| p + 1).unwrap_or(0);
            let col = params.text[line_start..idx].chars().count() + 1;
            (line, col)
        })
        .unwrap_or((0, 1));
    let has_focus = text_edit_out.response.has_focus();

    // ---- 行号列（固定不滚动）----
    {
        let painter = ui.painter_at(gutter_rect);
        painter.rect_filled(gutter_rect, 0.0, params.theme.line_number_bg);
        // 当前行在行号列的延续（与内容区同一上一帧光标行，保持一致）
        let line_y = gutter_rect.top() + prev_cursor_line as f32 * line_height - offset_y;
        if line_y + line_height > gutter_rect.top() && line_y < gutter_rect.bottom() {
            painter.rect_filled(
                Rect::from_min_max(
                    Pos2::new(gutter_rect.left(), line_y),
                    Pos2::new(gutter_rect.right(), line_y + line_height),
                )
                .intersect(gutter_rect),
                0.0,
                params.theme.current_line_bg,
            );
        }
        let first_visible = ((offset_y / line_height).floor() as usize).saturating_sub(1);
        let visible_count = (gutter_rect.height() / line_height).ceil() as usize + 2;
        for i in 0..visible_count {
            let line = first_visible + i;
            if line >= line_count {
                break;
            }
            let y = gutter_rect.top() + line as f32 * line_height - offset_y + line_height * 0.5;
            let color = if line == prev_cursor_line {
                params.theme.line_number_active_fg
            } else {
                params.theme.line_number_fg
            };
            painter.text(
                Pos2::new(gutter_rect.right() - 12.0, y),
                egui::Align2::RIGHT_CENTER,
                (line + 1).to_string(),
                FontId::monospace(params.font_size * 0.85),
                color,
            );
        }
    }

    EditorOutput {
        changed: text_edit_out.response.changed(),
        cursor_line,
        cursor_col,
        rect: view_rect,
        text_rect,
        galley,
        galley_pos,
        scroll_y: offset_y,
        line_height,
        focused: has_focus,
        id: params.id,
    }
}

/// 带语法高亮的 galley 构建（syntect chunk 与缓冲行对齐覆盖）
fn build_galley(
    ui: &egui::Ui,
    buf: &dyn egui::TextBuffer,
    wrap_width: f32,
    cache: Option<&[Vec<HighlightSpan>]>,
    font_id: &FontId,
    wrap: bool,
) -> std::sync::Arc<egui::text::Galley> {
    let mut job = LayoutJob::default();
    let mut line_no = 0usize;
    let flush = |job: &mut LayoutJob, spans: &[HighlightSpan], last: &str| {
        if spans.is_empty() {
            job.append(last, 0.0, TextFormat::simple(font_id.clone(), Color32::BLACK));
            return;
        }
        // syntect 的 chunk 边界与缓冲区行尾不完全对齐：末 span 补上行尾
        let mut consumed = String::new();
        for (i, span) in spans.iter().enumerate() {
            let is_last = i == spans.len() - 1;
            let chunk = if is_last {
                if span.text.ends_with('\n') {
                    span.text.clone()
                } else {
                    format!("{}{}", span.text, last)
                }
            } else {
                span.text.clone()
            };
            job.append(
                &chunk,
                0.0,
                TextFormat {
                    font_id: if span.bold {
                        egui::FontId::new(font_id.size, egui::FontFamily::Name("bold".into()))
                    } else {
                        font_id.clone()
                    },
                    color: span.fg,
                    italics: span.italic,
                    underline: egui::Stroke::NONE,
                    ..Default::default()
                },
            );
            consumed.push_str(&chunk);
        }
        if consumed.is_empty() {
            job.append(last, 0.0, TextFormat::simple(font_id.clone(), Color32::BLACK));
        }
    };
    let mut rest = buf.as_str();
    while let Some(nl) = rest.find('\n') {
        let (line, tail) = rest.split_at(nl + 1);
        let spans = cache.and_then(|c| c.get(line_no)).map(|v| v.as_slice()).unwrap_or(&[]);
        flush(&mut job, spans, line);
        line_no += 1;
        rest = tail;
    }
    let spans = cache.and_then(|c| c.get(line_no)).map(|v| v.as_slice()).unwrap_or(&[]);
    flush(&mut job, spans, rest);
    job.wrap.max_width = if wrap { wrap_width } else { f32::INFINITY };
    ui.fonts(|f| f.layout_job(job))
}

/// 计算一个匹配区间在 galley 中覆盖的矩形列表（支持换行跨行），屏幕坐标
pub fn match_rects(
    galley: &egui::text::Galley,
    galley_pos: Pos2,
    start_char: usize,
    end_char: usize,
    view: Rect,
) -> Vec<Rect> {
    let s = galley.pos_from_cursor(egui::text::CCursor::new(start_char));
    let e = galley.pos_from_cursor(egui::text::CCursor::new(end_char));
    let mut out = Vec::new();
    let same_row = (s.top() - e.top()).abs() < 0.5;
    if same_row {
        let r = Rect::from_min_max(
            Pos2::new(s.left(), s.top()),
            Pos2::new(e.left().max(s.left() + 2.0), s.bottom()),
        )
        .translate(galley_pos.to_vec2());
        if r.bottom() > view.top() && r.top() < view.bottom() {
            out.push(r);
        }
        return out;
    }
    // 跨行：起始行半段 + 中间整行 + 末行半段
    for row in &galley.rows {
        let (ry0, ry1) = (row.min_y(), row.max_y());
        if ry1 < s.top() || ry0 > e.top() {
            continue;
        }
        let left = if (ry0 - s.top()).abs() < 0.5 {
            s.left()
        } else {
            galley.rect.left()
        };
        let right = if (ry0 - e.top()).abs() < 0.5 {
            e.left().max(left + 2.0)
        } else {
            galley.rect.right()
        };
        let r = Rect::from_min_max(Pos2::new(left, ry0), Pos2::new(right, ry1))
            .translate(galley_pos.to_vec2());
        if r.bottom() > view.top() && r.top() < view.bottom() {
            out.push(r);
        }
    }
    out
}

/// Markdown 行内 span 拆分：`代码`、**粗体**、*斜体*、[链接](url)
fn inline_md_spans(line: &str, theme: &EditorTheme) -> Vec<HighlightSpan> {
    let base = HighlightSpan {
        fg: theme.editor_fg,
        bold: false,
        italic: false,
        text: String::new(),
    };
    let mut out: Vec<HighlightSpan> = Vec::new();
    // 按反引号切代码段
    let mut segments: Vec<(String, bool)> = Vec::new();
    let mut rest = line;
    let mut code = false;
    while let Some(pos) = rest.find('`') {
        if pos > 0 {
            segments.push((rest[..pos].to_string(), code));
        }
        code = !code;
        rest = &rest[pos + 1..];
    }
    segments.push((rest.to_string(), code));
    for (seg_text, seg_code) in segments {
        if seg_text.is_empty() {
            continue;
        }
        if seg_code {
            out.push(HighlightSpan {
                fg: theme.accent,
                bold: false,
                italic: false,
                text: seg_text,
            });
            continue;
        }
        // 非代码段：切 **粗体** / *斜体* / [链接](url)
        let mut plain = seg_text.as_str();
        while !plain.is_empty() {
            if let Some(r2) = plain.strip_prefix("**") {
                if let Some(end) = r2.find("**") {
                    if !r2[..end].is_empty() {
                        out.push(HighlightSpan {
                            fg: theme.editor_fg,
                            bold: true,
                            italic: false,
                            text: r2[..end].to_string(),
                        });
                        plain = &r2[end + 2..];
                        continue;
                    }
                }
                out.push(HighlightSpan {
                    fg: theme.editor_fg,
                    bold: false,
                    italic: false,
                    text: "**".to_string(),
                });
                plain = r2;
                continue;
            }
            if let Some(r2) = plain.strip_prefix('*') {
                if let Some(end) = r2.find('*') {
                    if !r2[..end].is_empty() {
                        out.push(HighlightSpan {
                            fg: theme.editor_fg,
                            bold: false,
                            italic: true,
                            text: r2[..end].to_string(),
                        });
                        plain = &r2[end + 1..];
                        continue;
                    }
                }
                out.push(HighlightSpan {
                    fg: theme.editor_fg,
                    bold: false,
                    italic: false,
                    text: "*".to_string(),
                });
                plain = r2;
                continue;
            }
            if plain.starts_with('[') {
                if let Some(close) = plain.find(']') {
                    if plain[close + 1..].starts_with('(') {
                        if let Some(pend) = plain[close + 1..].find(')') {
                            out.push(HighlightSpan {
                                fg: theme.accent,
                                bold: false,
                                italic: false,
                                text: plain[..close + pend + 2].to_string(),
                            });
                            plain = &plain[close + pend + 2..];
                            continue;
                        }
                    }
                }
            }
            // 普通文本吃到下一个标记或结尾
            let mut next = plain.len();
            // 从首个字符之后搜索（get(1..) 在多字节首字符处返回 None）；
            // next 取标记起始字节位（字符边界）
            let tail_start = plain.chars().next().map(|c| c.len_utf8()).unwrap_or(0);
            if let Some(tail) = plain.get(tail_start..) {
                if let Some(p) = tail.find("**") {
                    next = next.min(tail_start + p);
                }
                if let Some(p) = tail.find('*') {
                    next = next.min(tail_start + p);
                }
                if let Some(p) = tail.find('[') {
                    next = next.min(tail_start + p);
                }
            }
            let take = next.max(1).min(plain.len());
            out.push(HighlightSpan {
                fg: theme.editor_fg,
                bold: false,
                italic: false,
                text: plain[..take].to_string(),
            });
            plain = &plain[take..];
        }
    }
    out
}

/// Markdown 整篇 span 构建（行级形态 + 行内装饰；保持等高行，几何不受影响）
fn build_md_spans(text: &str, theme: &EditorTheme) -> Vec<Vec<HighlightSpan>> {
    let mut out: Vec<Vec<HighlightSpan>> = Vec::new();
    let mut in_fence = false;
    for line in text.split('\n') {
        let trimmed = line.trim_start();
        let line_nl = format!("{}\n", line);
        if trimmed.starts_with("```") || trimmed.starts_with("~~~") {
            in_fence = !in_fence;
            out.push(vec![HighlightSpan {
                fg: theme.line_number_fg,
                bold: true,
                italic: false,
                text: line_nl,
            }]);
            continue;
        }
        if in_fence {
            out.push(vec![HighlightSpan {
                fg: theme.editor_fg,
                bold: false,
                italic: false,
                text: line_nl,
            }]);
            continue;
        }
        if trimmed.starts_with('#')
            && (trimmed == "#" || trimmed.starts_with("# ") || trimmed[1..].starts_with('#'))
        {
            out.push(vec![HighlightSpan {
                fg: theme.accent,
                bold: true,
                italic: false,
                text: line_nl,
            }]);
            continue;
        }
        if trimmed.starts_with('>') {
            out.push(vec![HighlightSpan {
                fg: theme.line_number_active_fg,
                bold: false,
                italic: true,
                text: line_nl,
            }]);
            continue;
        }
        if line.is_empty() {
            out.push(Vec::new());
            continue;
        }
        let mut spans = inline_md_spans(line, theme);
        // 行尾换行必须挂在最后一个 span 上（flush 依赖它判定行边界）
        match spans.last_mut() {
            Some(last) => last.text.push('\n'),
            None => out.push(vec![HighlightSpan {
                fg: theme.editor_fg,
                bold: false,
                italic: false,
                text: "\n".to_string(),
            }]),
        }
        if !spans.is_empty() {
            out.push(spans);
        }
    }
    out
}

/// Markdown 装饰底：代码块底色 + 引用左条（内容坐标，随滚动移动，绘制在文本之下）
fn paint_md_block_decor(
    ui: &egui::Ui,
    text: &str,
    width: f32,
    line_height: f32,
    theme: &EditorTheme,
) {
    let mut in_fence = false;
    let mut fence_start: Option<usize> = None;
    let mut quote_start: Option<usize> = None;
    let mut fence_ranges: Vec<(usize, usize)> = Vec::new();
    let mut quote_ranges: Vec<(usize, usize)> = Vec::new();
    let total = text.split('\n').count();
    for (i, raw) in text.split('\n').enumerate() {
        let trimmed = raw.trim_start();
        if trimmed.starts_with("```") || trimmed.starts_with("~~~") {
            if in_fence {
                fence_ranges.push((fence_start.unwrap_or(i), i + 1));
                in_fence = false;
                fence_start = None;
            } else {
                in_fence = true;
                fence_start = Some(i);
            }
        }
        if !in_fence && trimmed.starts_with('>') {
            if quote_start.is_none() {
                quote_start = Some(i);
            }
        } else if let Some(qs) = quote_start {
            quote_ranges.push((qs, i));
            quote_start = None;
        }
    }
    if let Some(qs) = quote_start {
        quote_ranges.push((qs, total));
    }
    if in_fence {
        fence_ranges.push((fence_start.unwrap_or(0), total));
    }
    let cb = theme.current_line_bg;
    let code_bg = egui::Color32::from_rgba_unmultiplied(cb.r(), cb.g(), cb.b(), 170);
    for (a, b) in fence_ranges {
        ui.painter().rect_filled(
            Rect::from_min_max(
                Pos2::new(0.0, a as f32 * line_height),
                Pos2::new(width, b as f32 * line_height),
            ),
            0.0,
            code_bg,
        );
        ui.painter().rect_filled(
            Rect::from_min_max(
                Pos2::new(0.0, a as f32 * line_height),
                Pos2::new(3.0, b as f32 * line_height),
            ),
            0.0,
            theme.accent,
        );
    }
    for (a, b) in quote_ranges {
        ui.painter().rect_filled(
            Rect::from_min_max(
                Pos2::new(0.0, a as f32 * line_height),
                Pos2::new(3.0, b as f32 * line_height),
            ),
            0.0,
            theme.accent,
        );
    }
}
