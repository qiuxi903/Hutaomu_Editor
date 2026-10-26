// Hutaomu Editor (Rust) - 主应用：窗口镶边、活动栏、标签、面板、状态栏、快捷键。
use egui::{
    Align, Color32, Context, FontId, Id, Key, Label, Pos2, Rect, ScrollArea, Sense, TextEdit,
    TopBottomPanel, Vec2, ViewportCommand,
};

use crate::icons::{self, Icon};
use crate::markdown;
use crate::pickers::{OverlayKind, PickerResult};
use crate::plugins;
use crate::theme::Palette;
use crate::pickers;

// ---------------- 数据 ----------------

#[derive(Clone)]
pub struct Tab {
    pub title_base: String,   // 文件名或"未命名 N"
    pub path: Option<String>, // None = 未命名
    pub code: String,
    pub language: syntect::highlighting::Theme, // 占位：真实语言用 name 匹配 syntect
    pub language_name: String,                  // syntect 名称（如 "Rust"）
    pub language_id: String,                    // 小写标识
    pub dirty: bool,
    pub is_md: bool,
    pub untitled: bool,
}

impl Tab {
    pub fn display_name(&self) -> String {
        if self.untitled && !self.language_id.is_empty() && self.language_id != "plain text"
        {
            format!("{} · {}", self.title_base, self.language_name)
        } else {
            self.title_base.clone()
        }
    }
    pub fn eol(&self) -> &'static str {
        "LF"
    }
}

pub struct SidebarState {
    pub open_dirs: std::collections::BTreeSet<String>,
    pub entries: Vec<FileEntry>,
    pub search_query: String,
    pub search_results: Vec<(String, usize, String)>,
    pub search_done: bool,
}

#[derive(Clone)]
pub struct FileEntry {
    pub name: String,
    pub path: String,
    pub is_dir: bool,
}

// ---------------- 应用 ----------------

pub struct HutaomuApp {
    pub palette: Palette,
    pub tabs: Vec<Tab>,
    pub active: usize,
    pub untitled_counter: usize,
    pub workspace: Option<String>,
    pub sidebar: SidebarState,
    pub sidebar_page: SidebarPage,
    pub sidebar_visible: bool,
    pub plugins: Vec<plugins::InstalledPlugin>,
    pub plugin_enabled: std::collections::BTreeMap<String, bool>,
    pub overlay: Option<OverlayKind>,
    pub font_size: f32,
    pub word_wrap: bool,
    pub md_mode: MdMode,
    pub status_msg: Option<(String, std::time::Instant)>,
    pub syntax_set: syntect::parsing::SyntaxSet,
    pub syntax_theme: syntect::highlighting::Theme,
    pub highlight_caches: Vec<Option<crate::editor::HighlightCache>>,
}

#[derive(Clone, Copy, PartialEq)]
pub enum SidebarPage {
    Explorer,
    Search,
    Outline,
    Plugins,
}

#[derive(Clone, Copy, PartialEq)]
pub enum MdMode {
    Source,
    Preview,
}

impl HutaomuApp {
    pub fn new(cc: &eframe::CreationContext<'_>) -> Self {
        let palette = Palette::load_or_default(std::path::Path::new("themes/paper.json"));
        crate::theme::apply_visuals(&cc.egui_ctx, &palette);
        crate::theme::install_fonts(&cc.egui_ctx);

        let syntax_set = syntect::parsing::SyntaxSet::load_defaults_newlines();
        let mut theme_set = syntect::highlighting::ThemeSet::load_defaults();
        let syntax_theme = theme_set
            .themes
            .remove("InspiredGitHub")
            .unwrap_or_else(syntect::highlighting::Theme::default);

        let mut app = Self {
            palette,
            tabs: Vec::new(),
            highlight_caches: Vec::new(),
            active: 0,
            untitled_counter: 0,
            workspace: None,
            sidebar: SidebarState {
                open_dirs: Default::default(),
                entries: Vec::new(),
                search_query: String::new(),
                search_results: Vec::new(),
                search_done: false,
            },
            sidebar_page: SidebarPage::Explorer,
            sidebar_visible: true,
            plugins: plugins::scan(),
            plugin_enabled: Default::default(),
            overlay: None,
            font_size: 14.0,
            word_wrap: false,
            md_mode: MdMode::Source,
            status_msg: None,
            syntax_set,
            syntax_theme,
        };
        app.new_document();
        app
    }

    pub fn active_tab(&mut self) -> Option<&mut Tab> {
        self.tabs.get_mut(self.active)
    }

    pub fn new_document(&mut self) {
        self.untitled_counter += 1;
        self.tabs.push(Tab {
            title_base: format!("未命名 {}", self.untitled_counter),
            path: None,
            code: String::new(),
            language: self.syntax_theme.clone(),
            language_name: String::new(),
            language_id: String::new(),
            dirty: false,
            is_md: false,
            untitled: true,
        });
        self.highlight_caches.push(None);
        self.active = self.tabs.len() - 1;
    }

    pub fn open_file_dialog(&mut self) {
        if let Some(path) = rfd::FileDialog::new()
            .add_filter("所有文件", &["*"])
            .pick_file()
        {
            self.open_path(&path.to_string_lossy());
        }
    }

    pub fn open_path(&mut self, path: &str) {
        let Ok(text) = std::fs::read_to_string(path) else {
            self.set_status(format!("无法读取 {}", path));
            return;
        };
        let name = std::path::Path::new(path)
            .file_name()
            .map(|s| s.to_string_lossy().to_string())
            .unwrap_or_default();
        let (lang_name, lang_id) = detect_language(&self.syntax_set, path, &text);
        let is_md = lang_id == "markdown";
        // 已打开则聚焦
        if let Some(i) = self
            .tabs
            .iter()
            .position(|t| t.path.as_deref() == Some(path))
        {
            self.active = i;
            return;
        }
        self.tabs.push(Tab {
            title_base: name,
            path: Some(path.to_string()),
            code: text,
            language: self.syntax_theme.clone(),
            language_name: lang_name,
            language_id: lang_id,
            dirty: false,
            is_md,
            untitled: false,
        });
        self.highlight_caches.push(None);
        self.active = self.tabs.len() - 1;
        if let Some(dir) = std::path::Path::new(path).parent() {
            self.workspace = Some(dir.to_string_lossy().to_string());
            self.refresh_entries();
        }
    }

    pub fn save_active(&mut self, save_as: bool) {
        let Some(tab) = self.tabs.get(self.active).cloned() else {
            return;
        };
        let target = if tab.untitled || save_as {
            rfd::FileDialog::new()
                .set_file_name(&tab.title_base)
                .save_file()
                .map(|p| p.to_string_lossy().to_string())
        } else {
            tab.path.clone()
        };
        let Some(target) = target else { return };
        match std::fs::write(&target, &tab.code) {
            Ok(_) => {
                let name = std::path::Path::new(&target)
                    .file_name()
                    .map(|s| s.to_string_lossy().to_string())
                    .unwrap_or_default();
                let (lang_name, lang_id) =
                    detect_language(&self.syntax_set, &target, &tab.code);
                let t = &mut self.tabs[self.active];
                t.path = Some(target.clone());
                t.title_base = name;
                t.language_name = lang_name;
                t.language_id = lang_id.clone();
                t.is_md = lang_id == "markdown";
                t.dirty = false;
                t.untitled = false;
                self.set_status(format!("已保存 {}", target));
            }
            Err(e) => self.set_status(format!("保存失败：{}", e)),
        }
    }

    pub fn close_active(&mut self) {
        if self.active < self.tabs.len() {
            self.tabs.remove(self.active);
            self.highlight_caches.remove(self.active);
        }
        if self.tabs.is_empty() {
            self.new_document();
        } else {
            self.active = self.active.saturating_sub(1).min(self.tabs.len() - 1);
        }
    }

    pub fn set_status(&mut self, msg: String) {
        self.status_msg = Some((msg, std::time::Instant::now()));
    }

    pub fn refresh_entries(&mut self) {
        self.sidebar.entries.clear();
        let Some(ws) = self.workspace.clone() else {
            return;
        };
        if let Ok(rd) = std::fs::read_dir(&ws) {
            for e in rd.flatten() {
                let name = e.file_name().to_string_lossy().to_string();
                if name.starts_with('.') {
                    continue;
                }
                self.sidebar.entries.push(FileEntry {
                    name: name.clone(),
                    path: e.path().to_string_lossy().to_string(),
                    is_dir: e.path().is_dir(),
                });
            }
        }
        self.sidebar
            .entries
            .sort_by(|a, b| b.is_dir.cmp(&a.is_dir).then(a.name.cmp(&b.name)));
    }

    pub fn run_search(&mut self) {
        self.sidebar.search_results.clear();
        self.sidebar.search_done = false;
        let Some(ws) = self.workspace.clone() else {
            return;
        };
        let needle = self.sidebar.search_query.trim().to_lowercase();
        if needle.is_empty() {
            return;
        }
        let mut results = Vec::new();
        walk_files(std::path::Path::new(&ws), &mut |path: &String| {
            if results.len() >= 300 {
                return;
            }
            let Ok(text) = std::fs::read_to_string(path) else {
                return;
            };
            for (i, line) in text.lines().enumerate() {
                if line.to_lowercase().contains(&needle) {
                    results.push((path.clone(), i + 1, line.trim().to_string()));
                    let _ = &path;
                    if results.len() >= 300 {
                        return;
                    }
                }
            }
        });
        self.sidebar.search_results = results;
        self.sidebar.search_done = true;
    }

    pub fn open_folder_dialog(&mut self) {
        if let Some(dir) = rfd::FileDialog::new().pick_folder() {
            self.workspace = Some(dir.to_string_lossy().to_string());
            self.refresh_entries();
            self.sidebar_page = SidebarPage::Explorer;
            self.sidebar_visible = true;
        }
    }

    pub fn open_folder_at(&mut self, path: &str) {
        let _ = open::that(path);
    }
}

fn walk_files(dir: &std::path::Path, f: &mut impl FnMut(&String)) {
    let Ok(rd) = std::fs::read_dir(dir) else {
        return;
    };
    for e in rd.flatten() {
        let path = e.path();
        let name = e.file_name().to_string_lossy().to_string();
        if name.starts_with('.') || name == "target" || name == "node_modules" {
            continue;
        }
        if path.is_dir() {
            walk_files(&path, f);
        } else if path.is_file() {
            f(&path.to_string_lossy().to_string());
        }
    }
}

fn detect_language(
    set: &syntect::parsing::SyntaxSet,
    path: &str,
    _content: &str,
) -> (String, String) {
    let ext = std::path::Path::new(path)
        .extension()
        .map(|s| s.to_string_lossy().to_lowercase())
        .unwrap_or_default();
    let syntax = set
        .find_syntax_by_extension(&ext)
        .or_else(|| set.find_syntax_by_extension("txt"))
        .unwrap_or_else(|| set.find_syntax_plain_text());
    (
        syntax.name.clone(),
        syntax.name.to_lowercase().replace(' ', "-"),
    )
}

// ---------------- UI ----------------

impl eframe::App for HutaomuApp {
    fn update(&mut self, ctx: &Context, _frame: &mut eframe::Frame) {
        handle_shortcuts(self, ctx);

        // 框体：标题栏 → [活动栏|侧栏|中央] → 状态栏
        top_title_bar(self, ctx);
        bottom_status_bar(self, ctx);

        egui::SidePanel::left("activity_bar")
            .exact_width(48.0)
            .frame(egui::Frame::none().fill(self.palette.c(&self.palette.activity_bar_bg)))
            .show(ctx, |ui| {
                activity_bar_ui(self, ui);
            });

        if self.sidebar_visible {
            egui::SidePanel::left("side_area")
                .exact_width(260.0)
                .frame(egui::Frame::none().fill(self.palette.c(&self.palette.sidebar_bg)))
                .show(ctx, |ui| {
                    match self.sidebar_page {
                        SidebarPage::Explorer => explorer_ui(self, ui),
                        SidebarPage::Search => search_ui(self, ui),
                        SidebarPage::Outline => outline_ui(self, ui),
                        SidebarPage::Plugins => plugins_ui(self, ui),
                    }
                    // 底部弹性
                    ui.with_layout(egui::Layout::bottom_up(Align::Center), |_| {});
                });
        }

        egui::CentralPanel::default()
            .frame(egui::Frame::none().fill(self.palette.c(&self.palette.editor_bg)))
            .show(ctx, |ui| {
                if self.tabs.is_empty() {
                    return;
                }
                tab_bar_ui(self, ui);
                // 编辑区 / 预览
                if self.md_mode == MdMode::Preview && self.active_tab().map(|t| t.is_md).unwrap_or(false)
                {
                    ui.columns(2, |cols| {
                        editor_area_ui(self, &mut cols[0]);
                        markdown_preview_ui(self, &mut cols[1]);
                    });
                } else {
                    editor_area_ui(self, ui);
                }
            });

        if let Some(kind) = self.overlay.clone() {
            pickers::draw_overlay(self, ctx, kind);
        }

        // 状态消息 6 秒后消失
        if let Some((_, t)) = &self.status_msg {
            if t.elapsed() > std::time::Duration::from_secs(6) {
                self.status_msg = None;
            }
        }
    }
}

fn handle_shortcuts(app: &mut HutaomuApp, ctx: &Context) {
    if app.overlay.is_some() {
        // 覆盖层自己处理按键
        return;
    }
    let (ctrl, shift) = ctx.input(|i| (i.modifiers.ctrl, i.modifiers.shift));
    let pressed = |k: Key| ctx.input(|i| i.key_pressed(k));
    let save = ctrl && pressed(Key::S);
    let save_as = ctrl && shift && pressed(Key::S);
    let new_doc = ctrl && pressed(Key::N);
    let close_tab = ctrl && pressed(Key::W);
    let open = ctrl && pressed(Key::O);
    let open_folder = ctrl && shift && pressed(Key::O);
    let palette_cmd = ctrl && shift && pressed(Key::P);
    let quick_open = ctrl && !shift && pressed(Key::P);
    let toggle_sidebar = ctrl && pressed(Key::B);
    let zoom_in = ctrl && (pressed(Key::Equals) || pressed(Key::Plus));
    let zoom_out = ctrl && pressed(Key::Minus);
    let zoom_reset = ctrl && pressed(Key::Num0);

    if save {
        app.save_active(false);
    } else if save_as {
        app.save_active(true);
    } else if new_doc {
        app.new_document();
    } else if close_tab {
        app.close_active();
    } else if open {
        app.open_file_dialog();
    } else if open_folder {
        app.open_folder_dialog();
    } else if palette_cmd {
        app.overlay = Some(OverlayKind::Commands);
    } else if quick_open && app.workspace.is_some() {
        app.overlay = Some(OverlayKind::QuickOpen);
    } else if toggle_sidebar {
        app.sidebar_visible = !app.sidebar_visible;
    } else if zoom_in {
        app.font_size = (app.font_size + 1.0).min(30.0);
    } else if zoom_out {
        app.font_size = (app.font_size - 1.0).max(9.0);
    } else if zoom_reset {
        app.font_size = 14.0;
    }
}

// ---------------- 状态栏 ----------------

fn bottom_status_bar(app: &mut HutaomuApp, ctx: &Context) {
    let p = app.palette.clone();
    egui::TopBottomPanel::bottom("status_bar")
        .exact_height(26.0)
        .frame(egui::Frame::none().fill(p.c(&p.status_bar_bg)))
        .show(ctx, |ui| {
            ui.horizontal_centered(|ui| {
                ui.add_space(12.0);
                // 状态消息
                if let Some((msg, t)) = &app.status_msg {
                    if t.elapsed().as_secs() < 6 {
                        ui.label(
                            egui::RichText::new(msg)
                                .size(11.5)
                                .color(p.c(&p.status_bar_fg)),
                        );
                    }
                }
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    ui.add_space(12.0);
                    let tab = app.tabs.get(app.active);
                    let (cursor_line, cursor_col) = (1usize, 1usize);
                    ui.label(
                        egui::RichText::new(format!("行 {}, 列 {}", cursor_line, cursor_col))
                            .size(11.5)
                            .color(p.c(&p.status_bar_fg)),
                    );
                    ui.label(
                        egui::RichText::new("LF")
                            .size(11.5)
                            .color(p.c(&p.status_bar_fg)),
                    );
                    ui.label(
                        egui::RichText::new("UTF-8")
                            .size(11.5)
                            .color(p.c(&p.status_bar_fg)),
                    );
                    // 语言项：可点击 → 语言选择器
                    let lang = tab
                        .map(|t| {
                            if t.language_name.is_empty() {
                                "纯文本".to_string()
                            } else {
                                t.language_name.clone()
                            }
                        })
                        .unwrap_or_else(|| "纯文本".to_string());
                    let lang_text = egui::RichText::new(lang)
                        .size(11.5)
                        .color(p.c(&p.status_bar_fg))
                        .underline();
                    let resp = ui.add(egui::Button::new(lang_text)
                        .fill(Color32::TRANSPARENT)
                        .rounding(4.0));
                    if resp.clicked() {
                        app.overlay = Some(crate::pickers::OverlayKind::LanguagePicker);
                    }
                });
            });
        });
}

// ---------------- 标题栏 ----------------

fn top_title_bar(app: &mut HutaomuApp, ctx: &Context) {
    let p = app.palette.clone();
    TopBottomPanel::top("title_bar")
        .exact_height(40.0)
        .frame(egui::Frame::none().fill(p.c(&p.title_bar_bg)))
        .show(ctx, |ui| {
            let h = ui.available_height();
            let (rect, response) = ui.allocate_exact_size(Vec2::new(ui.available_width(), h), Sense::drag());

            // 品牌区
            let icon_rect = Rect::from_min_size(Pos2::new(rect.left() + 12.0, rect.center().y - 9.0), Vec2::splat(18.0));
            icons::draw(ui.painter(), icon_rect, Icon::File, p.accent());
            ui.painter().text(
                Pos2::new(rect.left() + 38.0, rect.center().y),
                egui::Align2::LEFT_CENTER,
                "Hutaomu",
                FontId::proportional(13.0),
                p.accent(),
            );
            ui.painter().text(
                Pos2::new(rect.left() + 96.0, rect.center().y),
                egui::Align2::LEFT_CENTER,
                "Editor",
                FontId::proportional(13.0),
                p.c(&p.title_bar_muted_fg),
            );

            // 菜单（紧跟品牌）
            let mut menu_x = rect.left() + 150.0;
            for (label, items) in main_menus(app) {
                let text_w = 14.0 * label.chars().count() as f32 + 24.0;
                let menu_rect = Rect::from_min_size(
                    Pos2::new(menu_x, rect.top() + 4.0),
                    Vec2::new(text_w, h - 8.0),
                );
                let resp = ui.allocate_rect(menu_rect, Sense::hover());
                let hovered = resp.hovered();
                if hovered {
                    ui.painter().rect_filled(menu_rect, 6.0, p.c(&p.menu_hover_bg));
                }
                ui.painter().text(
                    menu_rect.center(),
                    egui::Align2::CENTER_CENTER,
                    label,
                    FontId::proportional(12.5),
                    p.c(&p.menu_fg),
                );
                menu_popup(ui, app, &label, menu_rect, &items);
                menu_x += text_w;
            }

            // 居中标题
            let title = match app.tabs.get(app.active) {
                Some(t) => format!("{}{} - Hutaomu Editor", t.display_name(),
                    if t.dirty { " •" } else { "" }),
                None => "Hutaomu Editor".into(),
            };
            ui.painter().text(
                Pos2::new(rect.center().x, rect.center().y),
                egui::Align2::CENTER_CENTER,
                title,
                FontId::proportional(12.5),
                p.c(&p.title_bar_muted_fg),
            );

            // 窗口控制钮
            let maximized = ctx.input(|i| i.viewport().maximized.unwrap_or(false));
            let btns = [
                (Icon::Minimize, ViewportCommand::Minimized(true), false),
                (
                    if maximized { Icon::Restore } else { Icon::Maximize },
                    ViewportCommand::Maximized(!maximized),
                    false,
                ),
                (Icon::Close, ViewportCommand::Close, true),
            ];
            let bw = 44.0;
            let mut x = rect.right() - bw * 3.0;
            for (icon, cmd, danger) in btns {
                let br = Rect::from_min_size(Pos2::new(x, rect.top()), Vec2::new(bw, h));
                let resp = ui.allocate_rect(br, Sense::click());
                let hover = resp.hovered();
                if hover {
                    let hover_color = if danger {
                        p.c(&p.win_close_hover_bg)
                    } else {
                        p.c(&p.win_button_hover_bg)
                    };
                    ui.painter().rect_filled(br, 0.0, hover_color);
                }
                let fg = if danger && hover {
                    Color32::WHITE
                } else {
                    p.c(&p.title_bar_text)
                };
                let icon_rect = Rect::from_center_size(br.center(), Vec2::splat(16.0));
                icons::draw(ui.painter(), icon_rect, icon, fg);
                if resp.clicked() {
                    ctx.send_viewport_cmd(cmd);
                }
                x += bw;
            }

            // 拖动/双击最大化（空白区域）
            let interact_rect = Rect::from_min_max(
                Pos2::new(rect.left(), rect.top()),
                Pos2::new(x, rect.bottom()),
            );
            let drag = ui.allocate_rect(interact_rect, Sense::click_and_drag());
            if drag.double_clicked() {
                ctx.send_viewport_cmd(ViewportCommand::Maximized(!maximized));
            } else if drag.drag_started() {
                ctx.send_viewport_cmd(ViewportCommand::StartDrag);
            }
            let _ = response;
        });
}

type MenuItem = (&'static str, Option<MenuAction>);
type MenuAction = fn(&mut HutaomuApp);

fn main_menus(app: &HutaomuApp) -> Vec<(&'static str, Vec<MenuItem>)> {
    let _ = app;
    vec![
        (
            "文件",
            vec![
                ("新建文件", Some(HutaomuApp::new_document)),
                ("打开文件...", Some(HutaomuApp::open_file_dialog)),
                ("打开文件夹...", Some(HutaomuApp::open_folder_dialog)),
                ("保存", Some(|a: &mut HutaomuApp| a.save_active(false))),
                ("另存为...", Some(|a: &mut HutaomuApp| a.save_active(true))),
                ("关闭标签", Some(HutaomuApp::close_active)),
            ],
        ),
        (
            "编辑",
            vec![
                ("撤销", None),
                ("重做", None),
                ("剪切", None),
                ("复制", None),
                ("粘贴", None),
            ],
        ),
        (
            "视图",
            vec![
                ("切换侧边栏", Some(|a: &mut HutaomuApp| {
                    a.sidebar_visible = !a.sidebar_visible;
                })),
                ("Markdown：预览", Some(|a: &mut HutaomuApp| {
                    a.md_mode = MdMode::Preview;
                })),
                ("Markdown：源码", Some(|a: &mut HutaomuApp| {
                    a.md_mode = MdMode::Source;
                })),
            ],
        ),
        ("帮助", vec![("关于 Hutaomu Editor", None)]),
    ]
}

fn menu_popup(ui: &mut egui::Ui, app: &mut HutaomuApp, label: &str, anchor: Rect, items: &[MenuItem]) {
    let id = Id::new(("menu", label));
    let resp = ui.interact(anchor, id, Sense::click());
    // 简化：点击展开，点击外部关闭（egui 的 close-on-click-outside 由 menu_button 承担，
    // 这里手写：记录展开状态于 memory）
    let is_open = ui.memory(|m| {
        m.data
            .get_temp::<bool>(id)
            .unwrap_or(false)
    });
    if resp.clicked() {
        ui.memory_mut(|m| m.data.insert_temp(id, !is_open));
    }
    if is_open {
        let mut close = false;
        egui::Area::new(id.with("popup"))
            .order(egui::Order::Foreground)
            .fixed_pos(egui::Pos2::new(anchor.left(), anchor.bottom() + 2.0))
            .show(ui.ctx(), |ui| {
                egui::Frame::popup(ui.style())
                    .fill(app.palette.c(&app.palette.menu_bg))
                    .stroke(egui::Stroke::new(1.0, app.palette.c(&app.palette.menu_border)))
                    .rounding(app.palette.corner_radius_large as f32)
                    .inner_margin(egui::Margin::symmetric(6, 6))
                    .show(ui, |ui| {
                        ui.set_min_width(180.0);
                        for (text, action) in items {
                            let btn = ui.add_sized(
                                [ui.available_width(), 26.0],
                                egui::Button::new(*text)
                                    .fill(Color32::TRANSPARENT)
                                    .rounding(app.palette.corner_radius as f32),
                            );
                            if btn.clicked() {
                                close = true;
                                if let Some(action) = action {
                                    action(app);
                                }
                            }
                            if btn.hovered() {
                                ui.painter().rect_filled(
                                    btn.rect,
                                    app.palette.corner_radius as f32,
                                    app.palette.c(&app.palette.menu_item_hover_bg),
                                );
                            }
                        }
                    });
            });
        if close || ui.input(|i| i.key_pressed(Key::Escape)) {
            ui.memory_mut(|m| m.data.insert_temp(id, false));
        }
        // 点击面板外关闭
        if ui.input(|i| i.pointer.any_click())
            && !ui.input(|i| i.pointer.button_clicked(egui::PointerButton::Primary))
        {
            let _ = close;
        }
    }
}

// ---------------- 活动栏 ----------------

fn activity_bar_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    ui.add_space(8.0);
    let items = [
        (Icon::Explorer, "资源管理器", SidebarPage::Explorer),
        (Icon::Search, "全局搜索", SidebarPage::Search),
        (Icon::Outline, "大纲", SidebarPage::Outline),
        (Icon::Plugins, "插件", SidebarPage::Plugins),
    ];
    for (icon, tip, page) in items {
        let active = app.sidebar_visible && app.sidebar_page == page;
        let avail = ui.available_rect_before_wrap();
        let rect = Rect::from_center_size(
            Pos2::new(avail.center().x, avail.top() + 10.0 + 24.0),
            Vec2::splat(40.0),
        );
        let _ = avail;
        let resp = ui.allocate_rect(rect, Sense::click());
        if active {
            // 左侧 accent 竖条
            ui.painter().rect_filled(
                Rect::from_min_size(Pos2::new(rect.left() - 2.0, rect.top()), Vec2::new(3.0, rect.height())),
                2.0,
                app.palette.accent(),
            );
        }
        if resp.hovered() {
            ui.painter()
                .rect_filled(rect, 8.0, app.palette.c(&app.palette.sidebar_hover_bg));
        }
        let color = if active {
            app.palette.c(&app.palette.activity_icon_active_fg)
        } else {
            app.palette.c(&app.palette.activity_icon_fg)
        };
        icons::draw(ui.painter(), rect.shrink(10.0), icon, color);
        let _ = tip;
        if resp.clicked() {
            if active {
                app.sidebar_visible = false;
            } else {
                app.sidebar_visible = true;
                app.sidebar_page = page;
            }
        }
        ui.add_space(6.0);
    }
    // 底部关于
    ui.with_layout(egui::Layout::bottom_up(Align::Center), |ui| {
        let avail = ui.available_rect_before_wrap();
        let rect = Rect::from_center_size(
            Pos2::new(avail.center().x, avail.bottom() - 34.0),
            Vec2::splat(40.0),
        );
        let resp = ui.allocate_rect(rect, Sense::click());
        if resp.hovered() {
            ui.painter()
                .rect_filled(rect, 8.0, app.palette.c(&app.palette.sidebar_hover_bg));
        }
        icons::draw(
            ui.painter(),
            rect.shrink(10.0),
            Icon::Info,
            app.palette.c(&app.palette.activity_icon_fg),
        );
        if resp.clicked() {
            app.set_status("Hutaomu Editor (Rust) 0.1.0 — AGPL-3.0 永久开源".into());
        }
        ui.add_space(16.0);
    });
}

// ---------------- 侧栏面板 ----------------

fn panel_header(ui: &mut egui::Ui, title: &str, subtitle: &str) {
    ui.add_space(8.0);
    ui.horizontal(|ui| {
        ui.add_space(14.0);
        ui.vertical(|ui| {
            ui.label(
                egui::RichText::new(title)
                    .size(12.0)
                    .strong()
                    .color(egui::Color32::GRAY),
            );
            if !subtitle.is_empty() {
                ui.label(
                    egui::RichText::new(subtitle)
                        .size(11.0)
                        .color(egui::Color32::GRAY),
                );
            }
        });
    });
    ui.add_space(6.0);
}

fn explorer_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    panel_header(ui, "资源管理器", "");
    if app.workspace.is_none() {
        // 欢迎页
        ui.add_space(40.0);
        ui.vertical_centered(|ui| {
            ui.label(
                egui::RichText::new("未打开文件夹")
                    .size(13.0)
                    .color(app.palette.c(&app.palette.welcome_hint_fg)),
            );
            ui.add_space(14.0);
            let btn = ui.add_sized(
                [150.0, 34.0],
                egui::Button::new(
                    egui::RichText::new("打开文件夹")
                        .size(13.0)
                        .color(app.palette.c(&app.palette.button_fg)),
                )
                .fill(app.palette.c(&app.palette.button_bg))
                .rounding(app.palette.corner_radius as f32),
            );
            if btn.clicked() {
                app.open_folder_dialog();
            }
        });
        return;
    }
    // 工具行
    ui.horizontal(|ui| {
        ui.add_space(10.0);
        if small_icon_button(ui, app, Icon::Folder, "打开其他文件夹").clicked() {
            app.open_folder_dialog();
        }
        if small_icon_button(ui, app, Icon::Plus, "新建文件").clicked() {
            app.new_document();
        }
    });
    ui.add_space(4.0);
    ScrollArea::vertical().show(ui, |ui| {
        let entries = app.sidebar.entries.clone();
        for entry in entries {
            let indent = 14.0;
            let (rect, resp) = ui.allocate_exact_size(
                Vec2::new(ui.available_width(), 24.0),
                Sense::click(),
            );
            let hover = resp.hovered();
            let selected = false;
            if hover || selected {
                ui.painter().rect_filled(
                    rect,
                    4.0,
                    app.palette.c(&app.palette.sidebar_hover_bg),
                );
            }
            let x0 = rect.left() + indent;
            let icon_rect = Rect::from_center_size(
                Pos2::new(x0 + 8.0, rect.center().y),
                Vec2::splat(14.0),
            );
            let (icon, color) = if entry.is_dir {
                (Icon::Folder, app.palette.c(&app.palette.sidebar_title_fg))
            } else {
                (Icon::File, app.palette.c(&app.palette.sidebar_title_fg))
            };
            icons::draw(ui.painter(), icon_rect, icon, color);
            ui.painter().text(
                Pos2::new(x0 + 24.0, rect.center().y),
                egui::Align2::LEFT_CENTER,
                &entry.name,
                FontId::proportional(12.5),
                app.palette.c(&app.palette.sidebar_text),
            );
            if resp.clicked() {
                if entry.is_dir {
                    // v1：目录展开按需扫描
                } else {
                    app.open_path(&entry.path);
                }
            }
        }
    });
}

fn small_icon_button(ui: &mut egui::Ui, app: &HutaomuApp, icon: Icon, tip: &str) -> egui::Response {
    let (rect, resp) = ui.allocate_exact_size(Vec2::splat(26.0), Sense::click());
    if resp.hovered() {
        ui.painter()
            .rect_filled(rect, 6.0, app.palette.c(&app.palette.sidebar_hover_bg));
    }
    icons::draw(
        ui.painter(),
        rect.shrink(6.0),
        icon,
        app.palette.c(&app.palette.sidebar_title_fg),
    );
    resp.on_hover_text(tip)
}

fn search_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    panel_header(ui, "全局搜索", "");
    ui.horizontal(|ui| {
        ui.add_space(10.0);
        let query = app.sidebar.search_query.clone();
        let edit = TextEdit::singleline(&mut app.sidebar.search_query)
            .desired_width(150.0)
            .font(FontId::proportional(12.5));
        let resp = ui.add(edit);
        if resp.changed() || (resp.lost_focus() && ui.input(|i| i.key_pressed(Key::Enter))) {
            app.run_search();
        }
        let _ = query;
        if ui
            .add_sized(
                [52.0, 28.0],
                egui::Button::new(
                    egui::RichText::new("搜索")
                        .size(12.5)
                        .color(app.palette.c(&app.palette.button_fg)),
                )
                .fill(app.palette.c(&app.palette.button_bg))
                .rounding(app.palette.corner_radius as f32),
            )
            .clicked()
        {
            app.run_search();
        }
    });
    ui.add_space(6.0);
    if !app.sidebar.search_done {
        return;
    }
    ScrollArea::vertical().show(ui, |ui| {
        for (path, line, text) in app.sidebar.search_results.clone() {
            let (rect, resp) = ui.allocate_exact_size(
                Vec2::new(ui.available_width(), 34.0),
                Sense::click(),
            );
            if resp.hovered() {
                ui.painter().rect_filled(
                    rect,
                    4.0,
                    app.palette.c(&app.palette.sidebar_hover_bg),
                );
            }
            let file_name = std::path::Path::new(&path)
                .file_name()
                .map(|s| s.to_string_lossy().to_string())
                .unwrap_or_default();
            ui.painter().text(
                Pos2::new(rect.left() + 12.0, rect.top() + 9.0),
                egui::Align2::LEFT_CENTER,
                format!("{}:{}", file_name, line),
                FontId::proportional(11.0),
                app.palette.accent(),
            );
            ui.painter().text(
                Pos2::new(rect.left() + 12.0, rect.bottom() - 9.0),
                egui::Align2::LEFT_CENTER,
                text,
                FontId::proportional(11.5),
                app.palette.c(&app.palette.sidebar_text),
            );
            if resp.clicked() {
                app.open_path(&path);
            }
        }
    });
}

fn outline_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    panel_header(ui, "大纲", "");
    let Some(tab) = app.tabs.get(app.active) else {
        return;
    };
    let headings = markdown::headings(&tab.code);
    if headings.is_empty() {
        ui.add_space(20.0);
        ui.vertical_centered(|ui| {
            ui.label(
                egui::RichText::new("（无标题结构）")
                    .size(12.5)
                    .color(app.palette.c(&app.palette.welcome_hint_fg)),
            );
        });
        return;
    }
    ScrollArea::vertical().show(ui, |ui| {
        for (level, text, line) in headings {
            let indent = 12.0 + (level as f32 - 1.0) * 14.0;
            let (rect, resp) = ui.allocate_exact_size(
                Vec2::new(ui.available_width(), 26.0),
                Sense::click(),
            );
            if resp.hovered() {
                ui.painter().rect_filled(
                    rect,
                    4.0,
                    app.palette.c(&app.palette.sidebar_hover_bg),
                );
            }
            ui.painter().text(
                Pos2::new(rect.left() + indent, rect.center().y),
                egui::Align2::LEFT_CENTER,
                text,
                FontId::proportional(if level <= 2 { 13.0 } else { 12.0 }),
                app.palette.c(&app.palette.sidebar_text),
            );
            if resp.clicked() {
                app.set_status(format!("跳转到第 {} 行（v1 滚动定位待补）", line));
            }
        }
    });
}

fn plugins_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    let enabled_count = app.plugins.iter().filter(|p| app.plugin_enabled.get(&p.manifest.id).copied().unwrap_or(true)).count();
    panel_header(
        ui,
        "插件",
        &if app.plugins.is_empty() {
            "尚未安装插件".to_string()
        } else {
            format!("{} 个插件 · {} 已启用", app.plugins.len(), enabled_count)
        },
    );
    ScrollArea::vertical().show(ui, |ui| {
        let plugins = app.plugins.clone();
        for plugin in plugins {
            let (rect, resp) = ui.allocate_exact_size(
                Vec2::new(ui.available_width(), 64.0),
                Sense::click(),
            );
            if resp.hovered() {
                ui.painter().rect_filled(
                    rect,
                    8.0,
                    app.palette.c(&app.palette.sidebar_hover_bg),
                );
            }
            let enabled = app
                .plugin_enabled
                .get(&plugin.manifest.id)
                .copied()
                .unwrap_or(true);
            // 头像
            let avatar_rect =
                Rect::from_center_size(Pos2::new(rect.left() + 30.0, rect.center().y), Vec2::splat(38.0));
            ui.painter().rect_filled(
                avatar_rect,
                10.0,
                Color32::from_rgba_unmultiplied(74, 137, 220, 30),
            );
            ui.painter().text(
                avatar_rect.center(),
                egui::Align2::CENTER_CENTER,
                plugin.manifest.name.chars().next().unwrap_or('P').to_uppercase().to_string(),
                FontId::proportional(16.0),
                app.palette.accent(),
            );
            // 名称 + 徽章
            ui.painter().text(
                Pos2::new(rect.left() + 58.0, rect.top() + 16.0),
                egui::Align2::LEFT_CENTER,
                &plugin.manifest.name,
                FontId::proportional(12.5),
                Color32::BLACK,
            );
            if plugin.builtin {
                ui.painter().text(
                    Pos2::new(rect.left() + 58.0 + 12.0 * plugin.manifest.name.chars().count() as f32 + 12.0,
                              rect.top() + 16.0),
                    egui::Align2::LEFT_CENTER,
                    "预装",
                    FontId::proportional(10.0),
                    app.palette.accent(),
                );
            }
            ui.painter().text(
                Pos2::new(rect.left() + 58.0, rect.bottom() - 16.0),
                egui::Align2::LEFT_CENTER,
                if plugin.manifest.description.is_empty() {
                    format!("v{}", plugin.manifest.version)
                } else {
                    plugin.manifest.description.clone()
                },
                FontId::proportional(11.0),
                egui::Color32::GRAY,
            );
            // 启用开关
            let toggle_rect = Rect::from_center_size(
                Pos2::new(rect.right() - 26.0, rect.center().y),
                Vec2::new(36.0, 20.0),
            );
            let toggle_resp = ui.allocate_rect(toggle_rect, Sense::click());
            let track = if enabled {
                app.palette.accent()
            } else {
                Color32::from_gray(200)
            };
            ui.painter().rect_filled(toggle_rect, 10.0, track);
            let knob_x = if enabled {
                toggle_rect.right() - 11.0
            } else {
                toggle_rect.left() + 11.0
            };
            ui.painter().circle_filled(
                Pos2::new(knob_x, toggle_rect.center().y),
                8.0,
                Color32::WHITE,
            );
            if toggle_resp.clicked() {
                let v = app
                    .plugin_enabled
                    .entry(plugin.manifest.id.clone())
                    .or_insert(true);
                *v = !*v;
            }
        }
    });
}

// ---------------- 标签栏 + 编辑区 ----------------

fn tab_bar_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    let p = app.palette.clone();
    let h = 38.0;
    let (bar_rect, _) = ui.allocate_exact_size(Vec2::new(ui.available_width(), h), Sense::hover());
    ui.painter().rect_filled(bar_rect, 0.0, p.c(&p.tab_bar_bg));

    let mut x = bar_rect.left();
    let count = app.tabs.len();
    for i in 0..count {
        let tab = &app.tabs[i];
        let active = i == app.active;
        let title = tab.display_name() + if tab.dirty { " •" } else { "" };
        let text_w = 12.5 * title.chars().count() as f32;
        let w = text_w + 44.0; // padding + close
        let rect = Rect::from_min_size(Pos2::new(x, bar_rect.top()), Vec2::new(w, h));

        if active {
            ui.painter().rect_filled(rect, 0.0, p.c(&p.editor_bg));
            // 顶部圆角：只在活动标签上绘制圆角矩形（手动三层：顶圆角）
            // egui 无独立角半径，此处以背景色连通近似
        } else if rect.contains(ui.input(|i| i.pointer.latest_pos().unwrap_or_default())) {
            ui.painter().rect_filled(rect, 0.0, p.c(&p.tab_hover_bg));
        }

        // 语言徽章
        if !tab.language_id.is_empty() && tab.language_id != "plain text" {
            let badge = Rect::from_center_size(
                Pos2::new(rect.left() + 14.0, rect.center().y),
                Vec2::splat(14.0),
            );
            ui.painter().rect_filled(badge, 3.0, badge_color(&tab.language_id));
            ui.painter().text(
                badge.center(),
                egui::Align2::CENTER_CENTER,
                tab.language_name.chars().next().unwrap_or('L').to_uppercase().to_string(),
                FontId::monospace(9.0),
                Color32::WHITE,
            );
        }

        // 标题
        let title_x = if tab.language_id.is_empty() || tab.language_id == "plain text" {
            rect.left() + 12.0
        } else {
            rect.left() + 26.0
        };
        ui.painter().text(
            Pos2::new(title_x, rect.center().y),
            egui::Align2::LEFT_CENTER,
            &title,
            FontId::proportional(if active { 12.5 } else { 12.0 }),
            if active { p.c(&p.tab_active_fg) } else { p.c(&p.tab_fg) },
        );

        // 关闭 ×
        let close_rect = Rect::from_center_size(
            Pos2::new(rect.right() - 14.0, rect.center().y),
            Vec2::splat(16.0),
        );
        let close_resp = ui.allocate_rect(close_rect, Sense::click());
        if close_resp.hovered() {
            ui.painter()
                .circle_filled(close_rect.center(), 8.0, Color32::from_gray(90));
            icons::draw(
                ui.painter(),
                close_rect,
                Icon::Close,
                Color32::WHITE,
            );
        } else {
            icons::draw(
                ui.painter(),
                close_rect,
                Icon::Close,
                p.c(&p.tab_fg),
            );
        }
        if close_resp.clicked() {
            app.close_active();
            return;
        }

        // 切换
        let tab_resp = ui.allocate_rect(rect, Sense::click());
        if tab_resp.clicked() {
            app.active = i;
        }

        x += w + 2.0;
    }

    // "+" 按钮
    let plus_rect = Rect::from_center_size(
        Pos2::new(x + 14.0, bar_rect.center().y),
        Vec2::splat(24.0),
    );
    let plus_resp = ui.allocate_rect(plus_rect, Sense::click());
    if plus_resp.hovered() {
        ui.painter()
            .circle_filled(plus_rect.center(), 12.0, p.c(&p.tab_hover_bg));
    }
    icons::draw(
        ui.painter(),
        plus_rect,
        Icon::Plus,
        p.c(&p.tab_fg),
    );
    if plus_resp.clicked() {
        app.new_document();
    }
}

fn badge_color(language_id: &str) -> Color32 {
    let mut hash: u32 = 0;
    for c in language_id.chars() {
        hash = hash.wrapping_mul(31).wrapping_add(c as u32);
    }
    let h = hash % 360;
    Color32::from_rgb(
        (128.0 + 100.0 * (h as f32).to_radians().sin()) as u8,
        (128.0 + 100.0 * ((h + 120) as f32).to_radians().sin()) as u8,
        (128.0 + 100.0 * ((h + 240) as f32).to_radians().sin()) as u8,
    )
}

fn editor_area_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    // 只渲染活动标签
    let Some(tab_index) = (0..app.tabs.len()).find(|&i| i == app.active) else {
        return;
    };
    let p = app.palette.clone();
    let font_size = app.font_size;

    let theme = crate::editor::EditorTheme {
        editor_bg: p.c(&p.editor_bg),
        editor_fg: p.c(&p.editor_fg),
        current_line_bg: p.c(&p.current_line_bg),
        selection_bg: p.c(&p.selection_bg),
        line_number_fg: p.c(&p.line_number_fg),
        line_number_active_fg: p.c(&p.line_number_active_fg),
    };
    let syntax_name = if app.tabs[tab_index].language_name.is_empty() {
        "Plain Text".to_string()
    } else {
        app.tabs[tab_index].language_name.clone()
    };
    let syntax = app
        .syntax_set
        .find_syntax_by_name(&syntax_name)
        .unwrap_or_else(|| app.syntax_set.find_syntax_plain_text());

    let cache = &mut app.highlight_caches[tab_index];
    let lang_for_editor = app.tabs[tab_index].language_name.clone();
    let output = crate::editor::show(
        ui,
        crate::editor::EditorParams {
            id: egui::Id::new(("editor", tab_index)),
            text: &mut app.tabs[tab_index].code,
            font_size,
            wrap: app.word_wrap,
            language_name: &lang_for_editor,
            theme: &theme,
            cache,
            syntax,
            syntax_set: &app.syntax_set,
            show: true,
        },
    );
    if output.changed {
        app.tabs[tab_index].dirty = true;
    }

    // 未命名空文件的占位提示覆盖层（含可点击的"选择语言模式"——v1 状态栏入口）
    let tab = &app.tabs[tab_index];
    if tab.untitled && tab.code.is_empty() {
        ui.painter().text(
            Pos2::new(ui.available_rect_before_wrap().left() + 56.0,
                      ui.available_rect_before_wrap().top() + 14.0),
            egui::Align2::LEFT_TOP,
            "开始输入以关闭此提示。点击状态栏的语言可更改语法高亮。",
            egui::FontId::proportional(font_size),
            p.c(&p.title_bar_muted_fg),
        );
    }
}

fn markdown_preview_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    let Some(tab) = app.tabs.get(app.active) else {
        return;
    };
    let blocks = markdown::parse(&tab.code);
    let fg = app.palette.c(&app.palette.editor_fg);
    let accent = app.palette.accent();
    let muted = app.palette.c(&app.palette.title_bar_muted_fg);
    ScrollArea::vertical()
        .id_source("md_preview")
        .show(ui, |ui| {
            ui.add_space(16.0);
            for block in &blocks {
                ui.add_space(4.0);
                let rich = markdown::rich(block, fg, accent, muted);
                if matches!(block.kind, markdown::MdKind::Code) {
                    let (rect, _) = ui.allocate_at_least(
                        Vec2::new(ui.available_width() - 24.0, 8.0),
                        Sense::hover(),
                    );
                    ui.painter().rect_filled(
                        rect.expand2(Vec2::new(12.0, 8.0)),
                        8.0,
                        Color32::from_gray(247),
                    );
                    ui.label(rich);
                } else {
                    ui.add(Label::new(rich).wrap());
                }
                ui.add_space(4.0);
            }
        });
}
