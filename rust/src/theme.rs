// Hutaomu Editor (Rust) - 主题调色板与 egui 视觉映射。
// SPDX-License-Identifier: AGPL-3.0-only
use serde::Deserialize;

#[derive(Deserialize, Clone)]
pub struct Palette {
    #[serde(default = "d_accent")]
    pub accent: String,
    #[serde(default = "d_window")]
    pub window_bg: String,
    #[serde(default = "d_titlebar")]
    pub title_bar_bg: String,
    #[serde(default = "d_title_text")]
    pub title_bar_text: String,
    #[serde(default = "d_muted")]
    pub title_bar_muted_fg: String,
    #[serde(default = "d_sep")]
    pub separator_color: String,
    #[serde(default = "d_menu_fg")]
    pub menu_fg: String,
    #[serde(default = "d_menu_hover")]
    pub menu_hover_bg: String,
    #[serde(default = "d_win_hover")]
    pub win_button_hover_bg: String,
    #[serde(default = "d_win_close")]
    pub win_close_hover_bg: String,
    #[serde(default = "d_activity")]
    pub activity_bar_bg: String,
    #[serde(default = "d_activity_icon")]
    pub activity_icon_fg: String,
    #[serde(default = "d_activity_active")]
    pub activity_icon_active_fg: String,
    #[serde(default = "d_activity_disabled")]
    pub activity_icon_disabled_fg: String,
    #[serde(default = "d_sidebar")]
    pub sidebar_bg: String,
    #[serde(default = "d_sidebar_title")]
    pub sidebar_title_fg: String,
    #[serde(default = "d_sidebar_text")]
    pub sidebar_text: String,
    #[serde(default = "d_sidebar_hover")]
    pub sidebar_hover_bg: String,
    #[serde(default = "d_sidebar_sel")]
    pub sidebar_selected_bg: String,
    #[serde(default = "d_sidebar_sel_fg")]
    pub sidebar_selected_fg: String,
    #[serde(default = "d_welcome")]
    pub welcome_hint_fg: String,
    #[serde(default = "d_status")]
    pub status_bar_bg: String,
    #[serde(default = "d_status_fg")]
    pub status_bar_fg: String,
    #[serde(default = "d_tabbar")]
    pub tab_bar_bg: String,
    #[serde(default = "d_tab_fg")]
    pub tab_fg: String,
    #[serde(default = "d_tab_active")]
    pub tab_active_bg: String,
    #[serde(default = "d_tab_active_fg")]
    pub tab_active_fg: String,
    #[serde(default = "d_tab_hover")]
    pub tab_hover_bg: String,
    #[serde(default = "d_editor")]
    pub editor_bg: String,
    #[serde(default = "d_editor_fg")]
    pub editor_fg: String,
    #[serde(default = "d_current_line")]
    pub current_line_bg: String,
    #[serde(default = "d_selection")]
    pub selection_bg: String,
    #[serde(default = "d_lnum")]
    pub line_number_fg: String,
    #[serde(default = "d_lnum_active")]
    pub line_number_active_fg: String,
    #[serde(default = "d_scroll")]
    pub scrollbar_handle: String,
    #[serde(default = "d_scroll_hover")]
    pub scrollbar_handle_hover: String,
    #[serde(default = "d_menu_bg")]
    pub menu_bg: String,
    #[serde(default = "d_menu_border")]
    pub menu_border: String,
    #[serde(default = "d_menu_item_hover")]
    pub menu_item_hover_bg: String,
    #[serde(default = "d_menu_disabled")]
    pub menu_disabled_fg: String,
    #[serde(default = "d_tooltip")]
    pub tooltip_bg: String,
    #[serde(default = "d_tooltip_fg")]
    pub tooltip_fg: String,
    #[serde(default = "d_tooltip_border")]
    pub tooltip_border: String,
    #[serde(default = "d_button")]
    pub button_bg: String,
    #[serde(default = "d_button_hover")]
    pub button_hover: String,
    #[serde(default = "d_button_fg")]
    pub button_fg: String,
    #[serde(default = "d_sash")]
    pub sash_color: String,
    #[serde(default = "d_radius")]
    pub corner_radius: u8,
    #[serde(default = "d_radius_large")]
    pub corner_radius_large: u8,
}

fn d_accent() -> String { "#4a89dc".into() }
fn d_window() -> String { "#ffffff".into() }
fn d_titlebar() -> String { "#ffffff".into() }
fn d_title_text() -> String { "#24292e".into() }
fn d_muted() -> String { "#9a9a9a".into() }
fn d_sep() -> String { "#f0f0f0".into() }
fn d_menu_fg() -> String { "#24292e".into() }
fn d_menu_hover() -> String { "#f5f5f5".into() }
fn d_win_hover() -> String { "#f0f0f0".into() }
fn d_win_close() -> String { "#e56c6c".into() }
fn d_activity() -> String { "#fafafa".into() }
fn d_activity_icon() -> String { "#8f8f8f".into() }
fn d_activity_active() -> String { "#4a89dc".into() }
fn d_activity_disabled() -> String { "#c9c9c9".into() }
fn d_sidebar() -> String { "#fafafa".into() }
fn d_sidebar_title() -> String { "#9a9a9a".into() }
fn d_sidebar_text() -> String { "#3b3b3b".into() }
fn d_sidebar_hover() -> String { "#f0f0f0".into() }
fn d_sidebar_sel() -> String { "#eaeaea".into() }
fn d_sidebar_sel_fg() -> String { "#24292e".into() }
fn d_welcome() -> String { "#b0b0b0".into() }
fn d_status() -> String { "#ffffff".into() }
fn d_status_fg() -> String { "#9a9a9a".into() }
fn d_tabbar() -> String { "#fafafa".into() }
fn d_tab_fg() -> String { "#9a9a9a".into() }
fn d_tab_active() -> String { "#ffffff".into() }
fn d_tab_active_fg() -> String { "#24292e".into() }
fn d_tab_hover() -> String { "#f3f3f3".into() }
fn d_editor() -> String { "#ffffff".into() }
fn d_editor_fg() -> String { "#24292e".into() }
fn d_current_line() -> String { "#f7f9fb".into() }
fn d_selection() -> String { "#cfe2f3".into() }
fn d_lnum() -> String { "#c9c9c9".into() }
fn d_lnum_active() -> String { "#6a6a6a".into() }
fn d_scroll() -> String { "#e0e0e0".into() }
fn d_scroll_hover() -> String { "#cccccc".into() }
fn d_menu_bg() -> String { "#ffffff".into() }
fn d_menu_border() -> String { "#f0f0f0".into() }
fn d_menu_item_hover() -> String { "#f5f5f5".into() }
fn d_menu_disabled() -> String { "#c9c9c9".into() }
fn d_tooltip() -> String { "#ffffff".into() }
fn d_tooltip_fg() -> String { "#5a5a5a".into() }
fn d_tooltip_border() -> String { "#f0f0f0".into() }
fn d_button() -> String { "#4a89dc".into() }
fn d_button_hover() -> String { "#3a76c4".into() }
fn d_button_fg() -> String { "#ffffff".into() }
fn d_sash() -> String { "#eeeeee".into() }
fn d_radius() -> u8 { 8 }
fn d_radius_large() -> u8 { 12 }

impl Palette {
    /// 从 JSON 文件加载（失败回退内置默认）
    pub fn load_or_default(path: &std::path::Path) -> Self {
        std::fs::read_to_string(path)
            .ok()
            .and_then(|s| serde_json::from_str(&s).ok())
            .unwrap_or_default()
    }

    pub fn accent(&self) -> egui::Color32 {
        hex(&self.accent)
    }
    pub fn c(&self, s: &str) -> egui::Color32 {
        hex(s)
    }
}

fn hex(s: &str) -> egui::Color32 {
    let s = s.trim_start_matches('#');
    let v = u32::from_str_radix(s, 16).unwrap_or(0x000000);
    egui::Color32::from_rgb((v >> 16) as u8, (v >> 8) as u8, v as u8)
}

impl Default for Palette {
    fn default() -> Self {
        serde_json::from_str("{}").unwrap()
    }
}

/// 应用 egui 视觉：全白基调、圆角、无边框控件、柔和选中色
pub fn apply_visuals(ctx: &egui::Context, p: &Palette) {
    let mut visuals = egui::Visuals::light();
    visuals.window_stroke = egui::Stroke::NONE;
    visuals.widgets.noninteractive.bg_fill = p.c(&p.window_bg);
    visuals.widgets.noninteractive.fg_stroke = egui::Stroke::new(1.0, p.c(&p.editor_fg));
    visuals.widgets.noninteractive.bg_stroke = egui::Stroke::new(1.0, p.c(&p.separator_color));
    visuals.widgets.inactive.bg_fill = egui::Color32::TRANSPARENT;
    visuals.widgets.inactive.fg_stroke = egui::Stroke::new(1.0, p.c(&p.menu_fg));
    visuals.widgets.hovered.bg_fill = p.c(&p.menu_hover_bg);
    visuals.widgets.hovered.weak_bg_fill = p.c(&p.menu_hover_bg);
    visuals.widgets.hovered.fg_stroke = egui::Stroke::new(1.0, p.c(&p.editor_fg));
    visuals.widgets.active.bg_fill = p.menu_pressed_like();
    visuals.selection.bg_fill = p.c(&p.selection_bg);
    visuals.selection.stroke = egui::Stroke::new(1.0, p.c(&p.editor_fg));
    visuals.window_shadow = egui::Shadow::NONE;
    visuals.popup_shadow = egui::Shadow::NONE;

    ctx.set_visuals(visuals);
}

impl Palette {
    fn menu_pressed_like(&self) -> egui::Color32 {
        self.c(&self.menu_hover_bg)
    }
}



/// 注入 Windows 系统字体：微软雅黑（中文 UI 回退）+ Consolas（等宽代码）
pub fn install_fonts(ctx: &egui::Context) {
    let mut fonts = egui::FontDefinitions::default();
    let candidates = [
        ("msyh", "C:/Windows/Fonts/msyh.ttc"),
        ("msyh", "C:/Windows/Fonts/msyh.ttf"),
        ("consolas", "C:/Windows/Fonts/consola.ttf"),
    ];
    for (name, path) in candidates {
        if let Ok(bytes) = std::fs::read(path) {
            fonts
                .font_data
                .insert(name.to_string(), egui::FontData::from_owned(bytes).into());
        }
    }
    if fonts.font_data.contains_key("msyh") {
        fonts
            .families
            .entry(egui::FontFamily::Proportional)
            .or_default()
            .push("msyh".into());
        fonts
            .families
            .entry(egui::FontFamily::Monospace)
            .or_default()
            .push("msyh".into());
    }
    ctx.set_fonts(fonts);
}
