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
                    font_id: font_id.clone(),
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
