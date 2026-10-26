// Hutaomu Editor (Rust) - 代码编辑器组件：syntect 高亮 + 行号列 + 当前行。
// SPDX-License-Identifier: AGPL-3.0-only
use egui::{
    text::LayoutJob, Color32, FontId, Pos2, Rect, TextEdit, TextFormat, Vec2,
};

pub struct EditorOutput {
    pub changed: bool,
    pub cursor_line: usize,
}

/// 语法高亮缓存：文本/语言未变时不重复跑 syntect
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
    pub show: bool,
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
        h.finish()
    };
    let need_refresh = match params.cache {
        Some(c) => c.key != text_hash,
        None => true,
    };
    if need_refresh {
        let mut theme_set = syntect::highlighting::ThemeSet::load_defaults();
        let theme = theme_set
            .themes
            .remove("InspiredGitHub")
            .unwrap_or_else(syntect::highlighting::Theme::default);
        let mut highlighter =
            syntect::easy::HighlightLines::new(params.syntax, &theme);
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

    // ---- TextEdit ----
    let cache = params.cache.as_ref().map(|c| c.lines.clone()).unwrap_or_default();
    let font_size = params.font_size;
    let wrap = params.wrap;
    let font_id_cl = font_id.clone();
    let cache_cl = params.cache.as_ref().map(|c| c.lines.clone()).unwrap_or_default();
    let wrap_cl = wrap;
    let mut layouter = move |ui: &egui::Ui, buf: &dyn egui::TextBuffer, _wrap_width: f32| -> std::sync::Arc<egui::Galley> {
        let mut job = LayoutJob::default();
        let mut line_spans: &[HighlightSpan] = &[];
        let mut line_no = 0usize;
        let flush = |job: &mut LayoutJob, spans: &[HighlightSpan], last: &str| {
            if spans.is_empty() {
                job.append(last, 0.0, TextFormat::simple(font_id_cl.clone(), Color32::BLACK));
                return;
            }
            // syntect 的 chunk 边界与缓冲区行尾不完全对齐：按比例覆盖
            let mut consumed = String::new();
            for (i, span) in spans.iter().enumerate() {
                let is_last = i == spans.len() - 1;
                let chunk = if is_last {
                    if span.text.ends_with('\n') {
                        span.text.clone()
                    } else {
                        // 该行末尾（含 \n）合到本 span
                        format!("{}{}", span.text, last)
                    }
                } else {
                    span.text.clone()
                };
                let _ = &mut consumed;
                job.append(
                    &chunk,
                    0.0,
                    TextFormat {
                        font_id: font_id_cl.clone(),
                        color: span.fg,
                        italics: span.italic,
                        underline: egui::Stroke::NONE,
                        ..Default::default()
                    },
                );
                consumed.push_str(&chunk);
            }
            if consumed.is_empty() {
                job.append(last, 0.0, TextFormat::simple(font_id_cl.clone(), Color32::BLACK));
            }
        };
        let mut rest = buf.as_str();
        while let Some(nl) = rest.find('\n') {
            let (line, tail) = rest.split_at(nl + 1);
            let spans = cache_cl.get(line_no).map(|v| v.as_slice()).unwrap_or(&[]);
            flush(&mut job, spans, line);
            line_no += 1;
            rest = tail;
        }
        let spans = cache_cl.get(line_no).map(|v| v.as_slice()).unwrap_or(&[]);
        flush(&mut job, spans, rest);
        job.wrap.max_width = if wrap_cl { _wrap_width } else { f32::INFINITY };
        ui.fonts(|f| f.layout_job(job))
    };

    let mut output = TextEdit::multiline(params.text)
        .id_source(params.id)
        .font(font_id.clone())
        .desired_width(ui.available_width() - gutter_width)
        .layouter(&mut layouter)
        .frame(false)
        .show(ui);

    // ---- 滚动偏移推导（0.32 无公开偏移 API；galley_pos = 顶部 - scroll）----
    let view_rect = output.response.rect;
    let scroll_y = (view_rect.top() - output.galley_pos.y).max(0.0);
    let offset = Vec2::new(0.0, scroll_y);
    let cursor_line = output
        .cursor_range
        .map(|r| {
            let idx = r.primary.index;
            params.text[..idx].matches('\n').count()
        })
        .unwrap_or(0);

    // ---- 行号列（叠加绘制在 TextEdit 左缘留白上）----
    let painter = ui.painter_at(view_rect);
    // 行号底
    let gutter_rect = Rect::from_min_size(
        view_rect.min,
        Vec2::new(gutter_width, view_rect.height()),
    );
    painter.rect_filled(gutter_rect, 0.0, params.theme.editor_bg);
    // 当前行高亮（横向通栏，先画行号底再画行高亮会盖行号——行高亮画在行号右侧区域）
    let line_y = |line: usize| view_rect.top() + line as f32 * line_height - offset.y;
    let current_rect = Rect::from_min_max(
        Pos2::new(view_rect.left(), line_y(cursor_line)),
        Pos2::new(
            view_rect.right(),
            line_y(cursor_line) + line_height,
        ),
    );
    if current_rect.bottom() > view_rect.top() && current_rect.top() < view_rect.bottom() {
        painter.rect_filled(
            current_rect.intersect(view_rect),
            0.0,
            params.theme.current_line_bg,
        );
    }

    let first_visible = ((offset.y / line_height).floor() as usize).saturating_sub(1);
    let visible_count = (view_rect.height() / line_height).ceil() as usize + 2;
    for i in 0..visible_count {
        let line = first_visible + i;
        if line >= line_count {
            break;
        }
        let y = view_rect.top() + line as f32 * line_height - offset.y + line_height * 0.5;
        let color = if line == cursor_line {
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

    EditorOutput {
        changed: output.response.changed(),
        cursor_line,
    }
}


