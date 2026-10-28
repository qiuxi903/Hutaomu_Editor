// Hutaomu Editor (Rust) - 主题调色板与 egui 视觉映射。
// 兼容 Qt 版 theme.json 格式：colors{}/metrics{} 嵌套 + camelCase 键，
// 语法高亮走 colors["syntax.<role>"]，随主题整体切换。
// SPDX-License-Identifier: AGPL-3.0-only
use serde::Deserialize;
use std::collections::HashMap;

#[derive(Deserialize, Clone)]
#[serde(from = "ThemeFile")]
pub struct Palette {
    pub accent: String,
    pub accent_hover: String,
    pub window_bg: String,
    pub title_bar_bg: String,
    pub title_bar_text: String,
    pub title_bar_muted_fg: String,
    pub separator_color: String,
    pub menu_fg: String,
    pub menu_hover_bg: String,
    pub win_button_hover_bg: String,
    pub win_close_hover_bg: String,
    pub activity_bar_bg: String,
    pub activity_icon_fg: String,
    pub activity_icon_active_fg: String,
    pub activity_icon_disabled_fg: String,
    pub sidebar_bg: String,
    pub sidebar_title_fg: String,
    pub sidebar_text: String,
    pub sidebar_hover_bg: String,
    pub sidebar_selected_bg: String,
    pub sidebar_selected_fg: String,
    pub welcome_hint_fg: String,
    pub status_bar_bg: String,
    pub status_bar_fg: String,
    pub tab_bar_bg: String,
    pub tab_fg: String,
    pub tab_active_bg: String,
    pub tab_active_fg: String,
    pub tab_hover_bg: String,
    pub editor_bg: String,
    pub editor_fg: String,
    pub current_line_bg: String,
    pub selection_bg: String,
    pub line_number_fg: String,
    pub line_number_active_fg: String,
    pub scrollbar_handle: String,
    pub scrollbar_handle_hover: String,
    pub menu_bg: String,
    pub menu_border: String,
    pub menu_item_hover_bg: String,
    pub menu_disabled_fg: String,
    pub tooltip_bg: String,
    pub tooltip_fg: String,
    pub tooltip_border: String,
    pub button_bg: String,
    pub button_hover: String,
    pub button_fg: String,
    pub sash_color: String,
    pub corner_radius: u8,
    pub corner_radius_large: u8,
    /// 主题标识与元信息（来自 JSON 顶层）
    pub theme_id: String,
    pub theme_name: String,
    pub dark: bool,
    /// 界面形态：标签栏样式（default/chrome/underline/pill，Qt layout.json tabs.style）
    pub tabs_style: String,
    /// 主题自带背景图（相对主题目录）与模式/透明度
    pub bg_image: String,
    pub bg_mode: String,
    pub bg_opacity: f32,
    /// 主题文件所在目录（加载后填充，背景图相对路径解析用；非序列化字段）
    pub dir: String,
    /// 完整 colors 映射（供 syntax.<role> / searchHighlight 等动态键查询）
    pub extra: HashMap<String, String>,
}

/// Qt 版 theme.json 中间结构（嵌套 colors/metrics，camelCase）
#[derive(Deserialize, Default)]
struct ThemeFile {
    #[serde(default)]
    id: String,
    #[serde(default)]
    name: String,
    #[serde(default)]
    dark: bool,
    #[serde(default)]
    colors: HashMap<String, String>,
    #[serde(default)]
    metrics: HashMap<String, serde_json::Value>,
    #[serde(default)]
    layout: HashMap<String, serde_json::Value>,
    #[serde(default)]
    background: HashMap<String, serde_json::Value>,
}

impl From<ThemeFile> for Palette {
    fn from(f: ThemeFile) -> Self {
        let g = |key: &str, d: fn() -> String| -> String {
            f.colors.get(key).cloned().unwrap_or_else(d)
        };
        let m_u8 = |key: &str, d: u8| -> u8 {
            f.metrics
                .get(key)
                .and_then(|v| v.as_u64())
                .map(|v| v as u8)
                .unwrap_or(d)
        };
        Palette {
            accent: g("accent", d_accent),
            accent_hover: g("accentHover", d_button_hover),
            window_bg: g("windowBg", d_window),
            title_bar_bg: g("titleBarBg", d_titlebar),
            title_bar_text: g("titleBarText", d_title_text),
            title_bar_muted_fg: g("titleBarMutedFg", d_muted),
            separator_color: g("separatorColor", d_sep),
            menu_fg: g("menuFg", d_menu_fg),
            menu_hover_bg: g("menuHoverBg", d_menu_hover),
            win_button_hover_bg: g("winButtonHoverBg", d_win_hover),
            win_close_hover_bg: g("winCloseHoverBg", d_win_close),
            activity_bar_bg: g("activityBarBg", d_activity),
            activity_icon_fg: g("activityIconFg", d_activity_icon),
            activity_icon_active_fg: g("activityIconActiveFg", d_activity_active),
            activity_icon_disabled_fg: g("activityIconDisabledFg", d_activity_disabled),
            sidebar_bg: g("sidebarBg", d_sidebar),
            sidebar_title_fg: g("sidebarTitleFg", d_sidebar_title),
            sidebar_text: g("sidebarText", d_sidebar_text),
            sidebar_hover_bg: g("sidebarHoverBg", d_sidebar_hover),
            sidebar_selected_bg: g("sidebarSelectedBg", d_sidebar_sel),
            sidebar_selected_fg: g("sidebarSelectedFg", d_sidebar_sel_fg),
            welcome_hint_fg: g("welcomeHintFg", d_welcome),
            status_bar_bg: g("statusBarBg", d_status),
            status_bar_fg: g("statusBarFg", d_status_fg),
            tab_bar_bg: g("tabBarBg", d_tabbar),
            tab_fg: g("tabFg", d_tab_fg),
            tab_active_bg: g("tabActiveBg", d_tab_active),
            tab_active_fg: g("tabActiveFg", d_tab_active_fg),
            tab_hover_bg: g("tabHoverBg", d_tab_hover),
            editor_bg: g("editorBg", d_editor),
            editor_fg: g("editorFg", d_editor_fg),
            current_line_bg: g("currentLineBg", d_current_line),
            selection_bg: g("selectionBg", d_selection),
            line_number_fg: g("lineNumberFg", d_lnum),
            line_number_active_fg: g("lineNumberActiveFg", d_lnum_active),
            scrollbar_handle: g("scrollbarHandle", d_scroll),
            scrollbar_handle_hover: g("scrollbarHandleHover", d_scroll_hover),
            menu_bg: g("menuBg", d_menu_bg),
            menu_border: g("menuBorder", d_menu_border),
            menu_item_hover_bg: g("menuItemHoverBg", d_menu_item_hover),
            menu_disabled_fg: g("menuDisabledFg", d_menu_disabled),
            tooltip_bg: g("tooltipBg", d_tooltip),
            tooltip_fg: g("tooltipFg", d_tooltip_fg),
            tooltip_border: g("tooltipBorder", d_tooltip_border),
            button_bg: g("buttonBg", d_button),
            button_hover: g("buttonHover", d_button_hover),
            button_fg: g("buttonFg", d_button_fg),
            sash_color: g("sashColor", d_sash),
            corner_radius: m_u8("cornerRadius", 8),
            corner_radius_large: m_u8("cornerRadiusLarge", 12),
            tabs_style: f
                .layout
                .get("tabs")
                .and_then(|t| t.get("style"))
                .and_then(|s| s.as_str())
                .unwrap_or("default")
                .to_string(),
            bg_image: f
                .background
                .get("image")
                .and_then(|s| s.as_str())
                .unwrap_or("")
                .to_string(),
            bg_mode: f
                .background
                .get("mode")
                .and_then(|s| s.as_str())
                .unwrap_or("auto")
                .to_string(),
            bg_opacity: f
                .background
                .get("opacity")
                .and_then(|s| s.as_f64())
                .map(|v| v as f32)
                .unwrap_or(0.15),
            theme_id: f.id,
            theme_name: f.name,
            dark: f.dark,
            dir: String::new(),
            extra: f.colors,
        }
    }
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

impl Palette {
    /// 从 JSON 文件加载（失败回退内置默认）；记录主题目录供背景图相对路径解析
    pub fn load_or_default(path: &std::path::Path) -> Self {
        let mut p: Palette = std::fs::read_to_string(path)
            .ok()
            .and_then(|s| serde_json::from_str(&s).ok())
            .unwrap_or_default();
        if let Some(dir) = path.parent() {
            p.dir = dir.to_string_lossy().to_string();
        }
        p
    }

    pub fn accent(&self) -> egui::Color32 {
        hex(&self.accent)
    }
    pub fn c(&self, s: &str) -> egui::Color32 {
        hex(s)
    }
    /// 动态 colors 键查询（syntax.<role>、searchHighlight 等）
    pub fn color_key(&self, key: &str) -> Option<egui::Color32> {
        self.extra.get(key).map(|s| hex(s))
    }

    /// 查找高亮底色（Qt token: searchHighlight）
    pub fn search_highlight(&self) -> egui::Color32 {
        self.color_key("searchHighlight")
            .unwrap_or_else(|| hex(if self.dark { "#3a4652" } else { "#fff3c4" }))
    }

    /// 语法角色颜色（Qt 约定：colors["syntax.<role>"]），缺失时按深浅回退
    pub fn syntax_color(&self, role: &str) -> egui::Color32 {
        self.color_key(&format!("syntax.{}", role)).unwrap_or_else(|| {
            let (k, s, n, f, t, c, o, v, g, a, p) = if self.dark {
                (
                    "#76807f", "#7aa2c2", "#d0a8a8", "#9fb8cd", "#8fb8c8", "#c8a2c8",
                    "#8b939a", "#d5dade", "#b8c0a8", "#c8b88a", "#8b939a",
                )
            } else {
                (
                    "#6a9955", "#7c4dcf", "#b76b01", "#4a89dc", "#2980b9", "#8e44ad",
                    "#5a5a5a", "#24292e", "#c0392b", "#b76b01", "#5a5a5a",
                )
            };
            hex(match role {
                "comment" => k,
                "keyword" | "storage" | "include" | "macro" | "annotation" | "label" | "escape" => s,
                "string" | "regexp" => g,
                "number" => n,
                "function" | "method" | "constructor" => f,
                "type" | "class" | "struct" | "builtin" | "namespace" | "module" => t,
                "constant" | "boolean" => c,
                "operator" => o,
                "variable" | "parameter" | "property" | "field" => v,
                "tag" => g,
                "attribute" => a,
                _ => p,
            })
        })
    }

    /// 由主题语法色构建 syntect 高亮主题（随编辑器主题联动）
    pub fn syntect_theme(&self) -> syntect::highlighting::Theme {
        use syntect::highlighting::{Color, FontStyle, StyleModifier, Theme, ThemeItem, ThemeSettings};
        let fg = |s: egui::Color32| Color {
            r: s.r(),
            g: s.g(),
            b: s.b(),
            a: 255,
        };
        let item = |scope: &str, color: egui::Color32, italic: bool, bold: bool| ThemeItem {
            scope: scope.parse().unwrap_or_default(),
            style: StyleModifier {
                foreground: Some(fg(color)),
                background: None,
                font_style: Some({
                    let mut fs = FontStyle::empty();
                    if italic {
                        fs.insert(FontStyle::ITALIC);
                    }
                    if bold {
                        fs.insert(FontStyle::BOLD);
                    }
                    fs
                }),
            },
        };
        let ed_fg = self.c(&self.editor_fg);
        let mut scopes = Vec::new();
        // 顺序即优先级：后命中覆盖先命中（通用在前，具体在后）
        scopes.push(item("punctuation", self.syntax_color("punctuation"), false, false));
        scopes.push(item("variable", self.syntax_color("variable"), false, false));
        scopes.push(item("operator", self.syntax_color("operator"), false, false));
        scopes.push(item("keyword", self.syntax_color("keyword"), false, false));
        scopes.push(item("storage", self.syntax_color("keyword"), false, false));
        scopes.push(item("string", self.syntax_color("string"), false, false));
        scopes.push(item("comment", self.syntax_color("comment"), true, false));
        scopes.push(item("entity.name.tag", self.syntax_color("tag"), false, false));
        scopes.push(item(
            "entity.other.attribute-name",
            self.syntax_color("attribute"),
            false,
            false,
        ));
        scopes.push(item("support.function", self.syntax_color("function"), false, false));
        scopes.push(item("entity.name.function", self.syntax_color("function"), false, false));
        scopes.push(item("entity.name.type", self.syntax_color("type"), false, false));
        scopes.push(item("entity.name.class", self.syntax_color("type"), false, false));
        scopes.push(item("entity.name.struct", self.syntax_color("type"), false, false));
        scopes.push(item("entity.name.enum", self.syntax_color("type"), false, false));
        scopes.push(item("support.type", self.syntax_color("type"), false, false));
        scopes.push(item("support.class", self.syntax_color("type"), false, false));
        scopes.push(item("constant", self.syntax_color("constant"), false, false));
        scopes.push(item("constant.numeric", self.syntax_color("number"), false, false));
        scopes.push(item("markup.heading", self.accent(), true, true));
        scopes.push(item("markup.bold", ed_fg, false, true));
        scopes.push(item("markup.italic", ed_fg, true, false));
        let mut settings = ThemeSettings::default();
        settings.foreground = Some(fg(ed_fg));
        settings.background = Some(fg(self.c(&self.editor_bg)));
        settings.selection = Some(fg(self.c(&self.selection_bg)));
        settings.line_highlight = Some(fg(self.c(&self.current_line_bg)));
        Theme {
            name: Some("hutaomu".into()),
            author: Some("hutaomu".into()),
            settings,
            scopes,
        }
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
