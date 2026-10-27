// Hutaomu Editor (Rust) - 主应用：窗口镶边、活动栏、标签、面板、状态栏、快捷键。
use egui::{
    Align, Color32, Context, FontId, Id, Key, Label, Pos2, Rect, ScrollArea, Sense, TextEdit,
    TopBottomPanel, Vec2, ViewportCommand,
};

use crate::icons::{self, Icon};
use crate::markdown;
use crate::pickers::{self, OverlayKind};
use crate::plugins;
use crate::settings::Settings;
use crate::theme::Palette;

// ---------------- 数据 ----------------

#[derive(Clone)]
pub struct Tab {
    pub title_base: String,
    pub path: Option<String>,
    pub code: String,
    pub language: syntect::highlighting::Theme,
    pub language_name: String,
    pub language_id: String,
    pub dirty: bool,
    pub is_md: bool,
    pub untitled: bool,
    pub encoding: crate::encoding::TextEncoding,
    pub eol: crate::encoding::Eol,
    pub image: Option<egui::TextureHandle>,
}

impl Tab {
    pub fn is_image(&self) -> bool {
        self.image.is_some()
    }
    pub fn display_name(&self) -> String {
        if self.untitled && !self.language_id.is_empty() && self.language_id != "plain text" {
            format!("{} · {}", self.title_base, self.language_name)
        } else {
            self.title_base.clone()
        }
    }
}

#[derive(Clone)]
pub struct FileEntry {
    pub name: String,
    pub path: String,
    pub is_dir: bool,
}

pub struct SidebarState {
    pub entries: Vec<FileEntry>,
    pub search_query: String,
    pub search_results: Vec<(String, usize, String)>,
    pub search_done: bool,
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

#[derive(Clone, PartialEq)]
pub enum Modal {
    ConfirmCloseTab(usize),
    ConfirmExit,
}

#[derive(Clone, Default)]
pub struct FindState {
    pub open: bool,
    pub replace_mode: bool,
    pub query: String,
    pub replacement: String,
    pub count: usize,
}

pub struct HutaomuApp {
    pub ctx: egui::Context,
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
    pub settings: Settings,
    pub modal: Option<Modal>,
    pub find: FindState,
    pub themes: Vec<(String, Palette)>,
}

fn ctx_of(app: &HutaomuApp) -> egui::Context {
    app.ctx.clone()
}

impl HutaomuApp {
    pub fn new(cc: &eframe::CreationContext<'_>) -> Self {
        let settings = Settings::load();
        let palette_path =
            std::path::Path::new("themes").join(format!("{}.json", settings.theme_id));
        let palette = Palette::load_or_default(&palette_path);
        crate::theme::apply_visuals(&cc.egui_ctx, &palette);
        crate::theme::install_fonts(&cc.egui_ctx);
        let ctx = cc.egui_ctx.clone();

        let syntax_set = syntect::parsing::SyntaxSet::load_defaults_newlines();
        let mut theme_set = syntect::highlighting::ThemeSet::load_defaults();
        let syntax_theme = theme_set
            .themes
            .remove("InspiredGitHub")
            .unwrap_or_else(syntect::highlighting::Theme::default);

        let mut themes: Vec<(String, Palette)> = Vec::new();
        for id in ["paper", "greenwood-dark", "graphite"] {
            let path = std::path::Path::new("themes").join(format!("{}.json", id));
            themes.push((id.to_string(), Palette::load_or_default(&path)));
        }

        let mut app = Self {
            ctx,
            palette,
            tabs: Vec::new(),
            highlight_caches: Vec::new(),
            active: 0,
            untitled_counter: 0,
            workspace: None,
            sidebar: SidebarState {
                entries: Vec::new(),
                search_query: String::new(),
                search_results: Vec::new(),
                search_done: false,
            },
            sidebar_page: SidebarPage::Explorer,
            sidebar_visible: settings.sidebar_visible,
            plugins: plugins::scan(),
            plugin_enabled: Default::default(),
            overlay: None,
            font_size: settings.font_size,
            word_wrap: settings.word_wrap,
            md_mode: if settings.md_preview {
                MdMode::Preview
            } else {
                MdMode::Source
            },
            status_msg: None,
            syntax_set,
            syntax_theme,
            settings,
            modal: None,
            find: FindState::default(),
            themes,
        };
        if let Some(ws) = app.settings.workspace.clone() {
            if std::path::Path::new(&ws).is_dir() {
                app.workspace = Some(ws);
                app.refresh_entries();
            }
        }
        app.new_document();
        app
    }

    pub fn save_settings(&mut self) {
        let s = &mut self.settings;
        s.sidebar_visible = self.sidebar_visible;
        s.font_size = self.font_size;
        s.word_wrap = self.word_wrap;
        s.md_preview = self.md_mode == MdMode::Preview;
        s.workspace = self.workspace.clone();
        s.save();
    }

    pub fn switch_theme(&mut self, id: &str) {
        if let Some((_, pal)) = self.themes.iter().find(|(id2, _)| id2 == id).cloned() {
            self.palette = pal;
            crate::theme::apply_visuals(&ctx_of(self), &self.palette);
            self.settings.theme_id = id.to_string();
            self.settings.save();
            self.set_status(format!("主题已切换：{}", id));
        }
    }

    pub fn set_status(&mut self, msg: String) {
        self.status_msg = Some((msg, std::time::Instant::now()));
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
            encoding: crate::encoding::TextEncoding::Utf8,
            eol: crate::encoding::Eol::Lf,
            image: None,
        });
        self.highlight_caches.push(None);
        self.active = self.tabs.len() - 1;
    }

    pub fn open_path(&mut self, path: &str) {
        self.open_path_inner(path, None);
    }

    pub fn open_path_with_ctx(&mut self, path: &str, ctx: &egui::Context) {
        self.open_path_inner(path, Some(ctx));
    }

    fn open_path_inner(&mut self, path: &str, ctx: Option<&egui::Context>) {
        if let Some(i) = self
            .tabs
            .iter()
            .position(|t| t.path.as_deref() == Some(path))
        {
            self.active = i;
            return;
        }
        let ext = std::path::Path::new(path)
            .extension()
            .map(|s| s.to_string_lossy().to_lowercase())
            .unwrap_or_default();
        if ["png", "jpg", "jpeg", "gif", "bmp", "webp", "ico"].contains(&ext.as_str()) {
            if let Ok(bytes) = std::fs::read(path) {
                if let Ok(img) = image::load_from_memory(&bytes) {
                    let size = [img.width() as usize, img.height() as usize];
                    let color_image = egui::ColorImage::from_rgba_unmultiplied(
                        [size[0], size[1]],
                        &img.to_rgba8(),
                    );
                    let name = std::path::Path::new(path)
                        .file_name()
                        .map(|s| s.to_string_lossy().to_string())
                        .unwrap_or_default();
                    let Some(handle) = ctx.map(|c| {
                        c.load_texture(
                            &format!("img:{}", path),
                            color_image,
                            Default::default(),
                        )
                    }) else {
                        return;
                    };
                    self.tabs.push(Tab {
                        title_base: name,
                        path: Some(path.to_string()),
                        code: String::new(),
                        language: self.syntax_theme.clone(),
                        language_name: String::new(),
                        language_id: String::new(),
                        dirty: false,
                        is_md: false,
                        untitled: false,
                        encoding: crate::encoding::TextEncoding::Utf8,
                        eol: crate::encoding::Eol::Lf,
                        image: Some(handle),
                    });
                    self.highlight_caches.push(None);
                    self.active = self.tabs.len() - 1;
                    self.push_recent(path);
                    return;
                }
            }
        }
        let Ok(bytes) = std::fs::read(path) else {
            self.set_status(format!("无法读取 {}", path));
            return;
        };
        let (text, enc) = crate::encoding::decode(&bytes);
        let eol = crate::encoding::detect_eol(&text);
        let name = std::path::Path::new(path)
            .file_name()
            .map(|s| s.to_string_lossy().to_string())
            .unwrap_or_default();
        let (lang_name, lang_id) = detect_language(&self.syntax_set, path);
        let is_md = lang_id == "markdown";
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
            encoding: enc,
            eol,
            image: None,
        });
        self.highlight_caches.push(None);
        self.active = self.tabs.len() - 1;
        self.push_recent(path);
        if let Some(dir) = std::path::Path::new(path).parent() {
            self.workspace = Some(dir.to_string_lossy().to_string());
            self.refresh_entries();
        }
    }

    pub fn push_recent(&mut self, path: &str) {
        self.settings.push_recent(path);
        self.settings.save();
    }

    pub fn save_active(&mut self, save_as: bool) {
        let Some(tab) = self.tabs.get(self.active).cloned() else {
            return;
        };
        if tab.is_image() {
            return;
        }
        let target = if tab.untitled || save_as {
            rfd::FileDialog::new()
                .set_file_name(&tab.title_base)
                .save_file()
                .map(|p| p.to_string_lossy().to_string())
        } else {
            tab.path.clone()
        };
        let Some(target) = target else { return };
        let bytes = crate::encoding::encode(&tab.code, tab.encoding);
        match crate::encoding::atomic_write(&target, &bytes) {
            Ok(_) => {
                let name = std::path::Path::new(&target)
                    .file_name()
                    .map(|s| s.to_string_lossy().to_string())
                    .unwrap_or_default();
                let (lang_name, lang_id) = detect_language(&self.syntax_set, &target);
                let t = &mut self.tabs[self.active];
                t.path = Some(target.clone());
                t.title_base = name;
                t.language_name = lang_name;
                t.language_id = lang_id.clone();
                t.is_md = lang_id == "markdown";
                t.dirty = false;
                t.untitled = false;
                self.push_recent(&target);
                self.set_status(format!("已保存 {}（{}）", target, tab.encoding.display()));
            }
            Err(e) => self.set_status(format!("保存失败：{}", e)),
        }
    }

    pub fn close_active(&mut self) {
        if self.active >= self.tabs.len() {
            return;
        }
        if self.tabs[self.active].dirty {
            self.modal = Some(Modal::ConfirmCloseTab(self.active));
            return;
        }
        self.force_close(self.active);
    }

    pub fn force_close(&mut self, index: usize) {
        if index < self.tabs.len() {
            self.tabs.remove(index);
            self.highlight_caches.remove(index);
        }
        if self.tabs.is_empty() {
            self.new_document();
        } else {
            self.active = self.active.saturating_sub(1).min(self.tabs.len() - 1);
        }
    }

    pub fn open_file_dialog(&mut self) {
        if let Some(path) = rfd::FileDialog::new()
            .add_filter("所有文件", &["*"])
            .pick_file()
        {
            self.open_path(&path.to_string_lossy());
        }
    }

    pub fn open_folder_dialog(&mut self) {
        if let Some(dir) = rfd::FileDialog::new().pick_folder() {
            self.workspace = Some(dir.to_string_lossy().to_string());
            self.refresh_entries();
            self.sidebar_page = SidebarPage::Explorer;
            self.sidebar_visible = true;
        }
    }

    pub fn refresh_entries(&mut self) {
        self.sidebar.entries.clear();
        let Some(ws) = self.workspace.clone() else { return };
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
        let Some(ws) = self.workspace.clone() else { return };
        let needle = self.sidebar.search_query.trim().to_lowercase();
        if needle.is_empty() {
            return;
        }
        let mut results = Vec::new();
        walk_files(std::path::Path::new(&ws), &mut |path: &String| {
            if results.len() >= 300 {
                return;
            }
            let Ok(text) = std::fs::read_to_string(path) else { return };
            for (i, line) in text.lines().enumerate() {
                if line.to_lowercase().contains(&needle) {
                    results.push((path.clone(), i + 1, line.trim().to_string()));
                    if results.len() >= 300 {
                        return;
                    }
                }
            }
        });
        self.sidebar.search_results = results;
        self.sidebar.search_done = true;
    }

    
    pub fn toggle_comment(&mut self) {
        let comment = {
            let Some(tab) = self.tabs.get(self.active) else { return };
            match tab.language_id.as_str() {
                "rust" | "c" | "c++" | "java" | "javascript" | "typescript" | "go"
                | "json" | "php" | "dart" | "kotlin" | "swift" | "scss" | "less" => "//",
                "python" | "yaml" | "sh" | "bash" | "shell" | "ruby" | "toml" | "r"
                | "julia" | "perl" => "#",
                "sql" | "lua" | "haskell" => "--",
                _ => return,
            }
        };
        let Some(tab) = self.tabs.get_mut(self.active) else { return };
        let mut lines: Vec<String> = tab.code.lines().map(|l| l.to_string()).collect();
        let all_commented = lines
            .iter()
            .all(|l| l.trim_start().starts_with(&comment) || l.trim().is_empty());
        if all_commented {
            for l in &mut lines {
                if let Some(idx) = l.find(comment) {
                    *l = l[..idx].to_string() + &l[idx + comment.len()..];
                }
            }
        } else {
            for l in &mut lines {
                if !l.trim().is_empty() {
                    *l = format!("{} {}", comment, l);
                }
            }
        }
        let nl = if tab.eol == crate::encoding::Eol::Crlf { "\r\n" } else { "\n" };
        tab.code = lines.join(nl) + if tab.code.ends_with('\n') { nl } else { "" };
        tab.dirty = true;
    }
}

fn walk_files(dir: &std::path::Path, f: &mut impl FnMut(&String)) {
    let Ok(rd) = std::fs::read_dir(dir) else { return };
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

fn detect_language(set: &syntect::parsing::SyntaxSet, path: &str) -> (String, String) {
    let ext = std::path::Path::new(path)
        .extension()
        .map(|s| s.to_string_lossy().to_lowercase())
        .unwrap_or_default();
    let syntax = set
        .find_syntax_by_extension(&ext)
        .unwrap_or_else(|| set.find_syntax_plain_text());
    (
        syntax.name.clone(),
        syntax.name.to_lowercase().replace(' ', "-"),
    )
}

// ---------------- eframe App ----------------

impl eframe::App for HutaomuApp {
    fn update(&mut self, ctx: &Context, _frame: &mut eframe::Frame) {
        handle_shortcuts(self, ctx);
        self.handle_dropped_files(ctx);
        self.draw_modal(ctx);

        let close_requested = ctx.input(|i| i.viewport().close_requested());
        if close_requested {
            let any_dirty = self.tabs.iter().any(|t| t.dirty);
            if any_dirty {
                ctx.send_viewport_cmd(ViewportCommand::CancelClose);
                if self.modal.is_none() {
                    self.modal = Some(Modal::ConfirmExit);
                }
            } else {
                self.save_settings();
            }
        }

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
                .show(ctx, |ui| match self.sidebar_page {
                    SidebarPage::Explorer => explorer_ui(self, ui),
                    SidebarPage::Search => search_ui(self, ui),
                    SidebarPage::Outline => outline_ui(self, ui),
                    SidebarPage::Plugins => plugins_ui(self, ui),
                });
        }

        egui::CentralPanel::default()
            .frame(egui::Frame::none().fill(self.palette.c(&self.palette.editor_bg)))
            .show(ctx, |ui| {
                if self.tabs.is_empty() {
                    return;
                }
                tab_bar_ui(self, ui);
                let is_md = self.tabs[self.active].is_md;
                if self.md_mode == MdMode::Preview && is_md {
                    ui.columns(2, |cols| {
                        editor_area_ui(self, &mut cols[0]);
                        markdown_preview_ui(self, &mut cols[1]);
                    });
                } else {
                    editor_area_ui(self, ui);
                }
            });

        if let Some((_, t)) = &self.status_msg {
            if t.elapsed() > std::time::Duration::from_secs(6) {
                self.status_msg = None;
            }
        }
    }
}

fn handle_shortcuts(app: &mut HutaomuApp, ctx: &Context) {
    if app.overlay.is_some() {
        return;
    }
    let (ctrl, shift) = ctx.input(|i| (i.modifiers.ctrl, i.modifiers.shift));
    let pressed = |k: Key| ctx.input(|i| i.key_pressed(k));
    if ctrl && pressed(Key::S) {
        app.save_active(shift);
    } else if ctrl && pressed(Key::N) {
        app.new_document();
    } else if ctrl && pressed(Key::W) {
        app.close_active();
    } else if ctrl && pressed(Key::O) {
        app.open_file_dialog();
    } else if ctrl && shift && pressed(Key::O) {
        app.open_folder_dialog();
    } else if ctrl && shift && pressed(Key::P) {
        app.overlay = Some(OverlayKind::Commands);
    } else if ctrl && pressed(Key::P) && app.workspace.is_some() {
        app.overlay = Some(OverlayKind::QuickOpen);
    } else if ctrl && pressed(Key::B) {
        app.sidebar_visible = !app.sidebar_visible;
    } else if ctrl && pressed(Key::F) {
        app.find.open = true;
        app.find.replace_mode = false;
    } else if ctrl && pressed(Key::H) {
        app.find.open = true;
        app.find.replace_mode = true;
    } else if ctrl && pressed(Key::Slash) {
        app.toggle_comment();
    } else if ctrl && (pressed(Key::Equals) || pressed(Key::Plus)) {
        app.font_size = (app.font_size + 1.0).min(30.0);
    } else if ctrl && pressed(Key::Minus) {
        app.font_size = (app.font_size - 1.0).max(9.0);
    } else if ctrl && pressed(Key::Num0) {
        app.font_size = 14.0;
    }
}

impl HutaomuApp {
    fn handle_dropped_files(&mut self, ctx: &Context) {
        let dropped = ctx.input(|i| i.raw.dropped_files.clone());
        for f in dropped {
            if let Some(path) = f.path {
                self.open_path_with_ctx(&path.to_string_lossy(), ctx);
            }
        }
    }

    pub fn draw_modal(&mut self, ctx: &Context) {
        let Some(modal) = self.modal.clone() else { return };
        let p = self.palette.clone();
        egui::Area::new(Id::new("modal_mask"))
            .order(egui::Order::Foreground)
            .fixed_pos(ctx.screen_rect().left_top())
            .show(ctx, |ui| {
                let (rect, _) =
                    ui.allocate_exact_size(ctx.screen_rect().size(), egui::Sense::click());
                ui.painter()
                    .rect_filled(rect, 0.0, Color32::from_rgba_unmultiplied(0, 0, 0, 60));
            });
        egui::Area::new(Id::new("modal_box"))
            .order(egui::Order::Tooltip)
            .anchor(egui::Align2::CENTER_CENTER, [0.0, 0.0])
            .show(ctx, |ui| {
                egui::Frame::default()
                    .fill(p.c(&p.menu_bg))
                    .stroke(egui::Stroke::new(1.0, p.c(&p.menu_border)))
                    .rounding(p.corner_radius_large as f32)
                    .inner_margin(egui::Margin::same(18))
                    .show(ui, |ui| {
                        ui.set_width(420.0);
                        let (title, body) = match &modal {
                            Modal::ConfirmCloseTab(_) | Modal::ConfirmExit => (
                                "未保存的更改",
                                "当前文件有未保存的修改。保存更改吗？",
                            ),
                        };
                        ui.label(egui::RichText::new(title).size(15.0).strong());
                        ui.add_space(8.0);
                        ui.label(egui::RichText::new(body).size(13.0));
                        ui.add_space(14.0);
                        ui.horizontal(|ui| {
                            let btn = |ui: &mut egui::Ui, text: &str, primary: bool| {
                                let fill = if primary {
                                    p.c(&p.button_bg)
                                } else {
                                    Color32::TRANSPARENT
                                };
                                let stroke = if primary {
                                    egui::Stroke::NONE
                                } else {
                                    egui::Stroke::new(1.0, p.c(&p.separator_color))
                                };
                                ui.add_sized(
                                    [96.0, 32.0],
                                    egui::Button::new(
                                        egui::RichText::new(text).size(13.0).color(if primary {
                                            p.c(&p.button_fg)
                                        } else {
                                            p.c(&p.editor_fg)
                                        }),
                                    )
                                    .fill(fill)
                                    .stroke(stroke)
                                    .rounding(p.corner_radius as f32),
                                )
                            };
                            match &modal {
                                Modal::ConfirmCloseTab(_) => {
                                    if btn(ui, "保存", true).clicked() {
                                        self.save_active(false);
                                        if let Some(Modal::ConfirmCloseTab(i)) = self.modal.take() {
                                            self.force_close(i);
                                        }
                                    }
                                    if btn(ui, "不保存", false).clicked() {
                                        if let Some(Modal::ConfirmCloseTab(i)) = self.modal.take() {
                                            self.force_close(i);
                                        }
                                    }
                                    if btn(ui, "取消", false).clicked() {
                                        self.modal = None;
                                    }
                                }
                                Modal::ConfirmExit => {
                                    if btn(ui, "保存并退出", true).clicked() {
                                        for i in 0..self.tabs.len() {
                                            let dirty = self.tabs[i].dirty;
                                            let path = self.tabs[i].path.clone();
                                            if dirty {
                                                if let Some(path) = path {
                                                    let code = self.tabs[i].code.clone();
                                                    let enc = self.tabs[i].encoding;
                                                    let _ = crate::encoding::atomic_write(
                                                        &path,
                                                        &crate::encoding::encode(&code, enc),
                                                    );
                                                }
                                            }
                                        }
                                        self.save_settings();
                                        ctx.send_viewport_cmd(ViewportCommand::Close);
                                    }
                                    if btn(ui, "不保存退出", false).clicked() {
                                        self.save_settings();
                                        ctx.send_viewport_cmd(ViewportCommand::Close);
                                    }
                                    if btn(ui, "取消", false).clicked() {
                                        self.modal = None;
                                    }
                                }
                            }
                        });
                    });
            });
    }
}

// ---------------- 标题栏 ----------------

fn top_title_bar(app: &mut HutaomuApp, ctx: &Context) {
    let p = app.palette.clone();
    egui::TopBottomPanel::top("title_bar")
        .exact_height(40.0)
        .frame(egui::Frame::none().fill(p.c(&p.title_bar_bg)))
        .show(ctx, |ui| {
            let h = ui.available_height();
            let (rect, _drag_response) =
                ui.allocate_exact_size(Vec2::new(ui.available_width(), h), Sense::drag());

            let icon_rect = Rect::from_min_size(
                Pos2::new(rect.left() + 12.0, rect.center().y - 9.0),
                Vec2::splat(18.0),
            );
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

            let mut menu_x = rect.left() + 150.0;
            let menus: Vec<(&str, Vec<(String, MenuAction)>)> = vec![
                (
                    "文件",
                    vec![
                        ("新建文件".to_string(), Some(HutaomuApp::new_document)),
                        ("打开文件...".to_string(), Some(HutaomuApp::open_file_dialog)),
                        ("打开文件夹...".to_string(), Some(HutaomuApp::open_folder_dialog)),
                        ("保存".to_string(), Some(|a: &mut HutaomuApp| a.save_active(false))),
                        ("另存为...".to_string(), Some(|a: &mut HutaomuApp| a.save_active(true))),
                        ("关闭标签".to_string(), Some(HutaomuApp::close_active)),
                        ("退出".to_string(), Some(|a: &mut HutaomuApp| {
                            a.ctx.send_viewport_cmd(ViewportCommand::Close);
                        })),
                    ],
                ),
                (
                    "视图",
                    vec![
                        ("切换侧边栏".to_string(), Some(|a: &mut HutaomuApp| {
                            a.sidebar_visible = !a.sidebar_visible;
                        })),
                        ("Markdown：预览".to_string(), Some(|a: &mut HutaomuApp| {
                            a.md_mode = MdMode::Preview;
                        })),
                        ("Markdown：源码".to_string(), Some(|a: &mut HutaomuApp| {
                            a.md_mode = MdMode::Source;
                        })),
                        ("主题：简约白".to_string(), Some(|a: &mut HutaomuApp| a.switch_theme("paper"))),
                        ("主题：青木深色".to_string(), Some(|a: &mut HutaomuApp| a.switch_theme("greenwood-dark"))),
                        ("主题：石墨".to_string(), Some(|a: &mut HutaomuApp| a.switch_theme("graphite"))),
                    ],
                ),
                (
                    "帮助",
                    vec![("关于 Hutaomu Editor".to_string(), Some(|a: &mut HutaomuApp| {
                        a.set_status("Hutaomu Editor (Rust) 0.1.0 — AGPL-3.0 永久开源".into());
                    }))],
                ),
            ];
            for (label, items) in menus {
                let text_w = 13.0 * label.chars().count() as f32 + 24.0;
                let menu_rect = Rect::from_min_size(
                    Pos2::new(menu_x, rect.top() + 4.0),
                    Vec2::new(text_w, h - 8.0),
                );
                let resp = ui.allocate_rect(menu_rect, Sense::click());
                if resp.hovered() {
                    ui.painter().rect_filled(menu_rect, 6.0, p.c(&p.menu_hover_bg));
                }
                ui.painter().text(
                    menu_rect.center(),
                    egui::Align2::CENTER_CENTER,
                    label,
                    FontId::proportional(12.5),
                    p.c(&p.menu_fg),
                );
                let menu_id = Id::new(("title_menu", label));
                let is_open = ui.memory(|m| m.data.get_temp::<bool>(menu_id).unwrap_or(false));
                if resp.clicked() {
                    ui.memory_mut(|m| m.data.insert_temp(menu_id, !is_open));
                }
                if is_open {
                    let mut close_after = false;
                    egui::Area::new(menu_id.with("popup"))
                        .order(egui::Order::Foreground)
                        .fixed_pos(egui::Pos2::new(menu_rect.left(), menu_rect.bottom() + 2.0))
                        .show(ctx, |ui| {
                            egui::Frame::default()
                                .fill(p.c(&p.menu_bg))
                                .stroke(egui::Stroke::new(1.0, p.c(&p.menu_border)))
                                .rounding(p.corner_radius_large as f32)
                                .inner_margin(egui::Margin::symmetric(6, 6))
                                .show(ui, |ui| {
                                    ui.set_min_width(190.0);
                                    for (text, action) in items {
                                        let btn = ui.add_sized(
                                            [ui.available_width(), 26.0],
                                            egui::Button::new(text.clone())
                                                .fill(Color32::TRANSPARENT)
                                                .rounding(p.corner_radius as f32),
                                        );
                                        if btn.clicked() {
                                            close_after = true;
                                            if let Some(action) = action {
                                                action(app);
                                            }
                                        }
                                        if btn.hovered() {
                                            ui.painter().rect_filled(
                                                btn.rect,
                                                p.corner_radius as f32,
                                                p.c(&p.menu_item_hover_bg),
                                            );
                                        }
                                    }
                                });
                        });
                    if close_after || ctx.input(|i| i.key_pressed(Key::Escape)) {
                        ui.memory_mut(|m| m.data.insert_temp(menu_id, false));
                    }
                }
                menu_x += text_w;
            }

            let title = match app.tabs.get(app.active) {
                Some(t) => format!(
                    "{}{} - Hutaomu Editor",
                    t.display_name(),
                    if t.dirty { " •" } else { "" }
                ),
                None => "Hutaomu Editor".into(),
            };
            ui.painter().text(
                Pos2::new(rect.center().x, rect.center().y),
                egui::Align2::CENTER_CENTER,
                title,
                FontId::proportional(12.5),
                p.c(&p.title_bar_muted_fg),
            );

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
                if resp.hovered() {
                    let hover_color = if danger {
                        p.c(&p.win_close_hover_bg)
                    } else {
                        p.c(&p.win_button_hover_bg)
                    };
                    ui.painter().rect_filled(br, 0.0, hover_color);
                }
                let fg = if danger && resp.hovered() {
                    Color32::WHITE
                } else {
                    p.c(&p.title_bar_text)
                };
                icons::draw(
                    ui.painter(),
                    Rect::from_center_size(br.center(), Vec2::splat(16.0)),
                    icon,
                    fg,
                );
                if resp.clicked() {
                    ctx.send_viewport_cmd(cmd);
                }
                x += bw;
            }

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
        });
}

type MenuAction = Option<fn(&mut HutaomuApp)>;

// ---------------- 活动栏 ----------------

fn activity_bar_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    ui.add_space(8.0);
    let items = [
        (Icon::Explorer, SidebarPage::Explorer),
        (Icon::Search, SidebarPage::Search),
        (Icon::Outline, SidebarPage::Outline),
        (Icon::Plugins, SidebarPage::Plugins),
    ];
    for (icon, page) in items {
        let active = app.sidebar_visible && app.sidebar_page == page;
        let avail = ui.available_rect_before_wrap();
        let rect = Rect::from_center_size(
            Pos2::new(avail.center().x, avail.top() + 34.0),
            Vec2::splat(40.0),
        );
        let resp = ui.allocate_rect(rect, Sense::click());
        if active {
            ui.painter().rect_filled(
                Rect::from_min_size(
                    Pos2::new(rect.left() - 2.0, rect.top()),
                    Vec2::new(3.0, rect.height()),
                ),
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
    ui.with_layout(egui::Layout::bottom_up(egui::Align::Center), |ui| {
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
            ui.label(egui::RichText::new(title).size(12.0).strong().color(egui::Color32::GRAY));
            if !subtitle.is_empty() {
                ui.label(egui::RichText::new(subtitle).size(11.0).color(egui::Color32::GRAY));
            }
        });
    });
    ui.add_space(6.0);
}

fn small_icon_button(ui: &mut egui::Ui, app: &HutaomuApp, icon: Icon) -> egui::Response {
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
    resp
}

fn explorer_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    panel_header(ui, "资源管理器", "");
    if app.workspace.is_none() {
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
    ui.horizontal(|ui| {
        ui.add_space(10.0);
        if small_icon_button(ui, app, Icon::Folder).clicked() {
            app.open_folder_dialog();
        }
        if small_icon_button(ui, app, Icon::Plus).clicked() {
            app.new_document();
        }
    });
    ui.add_space(4.0);
    let entries = app.sidebar.entries.clone();
    ScrollArea::vertical().show(ui, |ui| {
        for entry in entries {
            let (rect, resp) =
                ui.allocate_exact_size(Vec2::new(ui.available_width(), 24.0), Sense::click());
            if resp.hovered() {
                ui.painter()
                    .rect_filled(rect, 4.0, app.palette.c(&app.palette.sidebar_hover_bg));
            }
            let x0 = rect.left() + 14.0;
            let icon_rect =
                Rect::from_center_size(Pos2::new(x0 + 8.0, rect.center().y), Vec2::splat(14.0));
            let icon = if entry.is_dir { Icon::Folder } else { Icon::File };
            icons::draw(
                ui.painter(),
                icon_rect,
                icon,
                app.palette.c(&app.palette.sidebar_title_fg),
            );
            ui.painter().text(
                Pos2::new(x0 + 24.0, rect.center().y),
                egui::Align2::LEFT_CENTER,
                &entry.name,
                FontId::proportional(12.5),
                app.palette.c(&app.palette.sidebar_text),
            );
            if resp.clicked() && !entry.is_dir {
                app.open_path(&entry.path);
            }
        }
    });
}

fn search_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    panel_header(ui, "全局搜索", "");
    ui.horizontal(|ui| {
        ui.add_space(10.0);
        let resp = TextEdit::singleline(&mut app.sidebar.search_query)
            .desired_width(150.0)
            .font(egui::FontId::proportional(12.5))
            .show(ui)
            .response;
        if resp.lost_focus() && ui.input(|i| i.key_pressed(Key::Enter)) {
            app.run_search();
        }
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
    let results = app.sidebar.search_results.clone();
    ScrollArea::vertical().show(ui, |ui| {
        for (path, line, text) in results {
            let (rect, resp) =
                ui.allocate_exact_size(Vec2::new(ui.available_width(), 34.0), Sense::click());
            if resp.hovered() {
                ui.painter()
                    .rect_filled(rect, 4.0, app.palette.c(&app.palette.sidebar_hover_bg));
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
    let Some(tab) = app.tabs.get(app.active) else { return };
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
            let (rect, resp) =
                ui.allocate_exact_size(Vec2::new(ui.available_width(), 26.0), Sense::click());
            if resp.hovered() {
                ui.painter()
                    .rect_filled(rect, 4.0, app.palette.c(&app.palette.sidebar_hover_bg));
            }
            ui.painter().text(
                Pos2::new(rect.left() + indent, rect.center().y),
                egui::Align2::LEFT_CENTER,
                &text,
                FontId::proportional(if level <= 2 { 13.0 } else { 12.0 }),
                app.palette.c(&app.palette.sidebar_text),
            );
            if resp.clicked() {
                app.set_status(format!("第 {} 行（{}）", line, text));
            }
        }
    });
}

fn plugins_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    let enabled_count = app
        .plugins
        .iter()
        .filter(|p| {
            app.plugin_enabled
                .get(&p.manifest.id)
                .copied()
                .unwrap_or(true)
        })
        .count();
    panel_header(
        ui,
        "插件",
        &if app.plugins.is_empty() {
            "尚未安装插件".to_string()
        } else {
            format!("{} 个插件 · {} 已启用", app.plugins.len(), enabled_count)
        },
    );
    let plugins = app.plugins.clone();
    ScrollArea::vertical().show(ui, |ui| {
        for plugin in plugins {
            let (rect, resp) =
                ui.allocate_exact_size(Vec2::new(ui.available_width(), 64.0), Sense::click());
            if resp.hovered() {
                ui.painter()
                    .rect_filled(rect, 8.0, app.palette.c(&app.palette.sidebar_hover_bg));
            }
            let enabled = app
                .plugin_enabled
                .get(&plugin.manifest.id)
                .copied()
                .unwrap_or(true);
            let avatar_rect = Rect::from_center_size(
                Pos2::new(rect.left() + 30.0, rect.center().y),
                Vec2::splat(38.0),
            );
            ui.painter().rect_filled(
                avatar_rect,
                10.0,
                Color32::from_rgba_unmultiplied(74, 137, 220, 30),
            );
            ui.painter().text(
                avatar_rect.center(),
                egui::Align2::CENTER_CENTER,
                plugin
                    .manifest
                    .name
                    .chars()
                    .next()
                    .unwrap_or('P')
                    .to_uppercase()
                    .to_string(),
                FontId::proportional(16.0),
                app.palette.accent(),
            );
            ui.painter().text(
                Pos2::new(rect.left() + 58.0, rect.top() + 16.0),
                egui::Align2::LEFT_CENTER,
                &plugin.manifest.name,
                FontId::proportional(12.5),
                Color32::BLACK,
            );
            if plugin.builtin {
                ui.painter().text(
                    Pos2::new(
                        rect.left()
                            + 58.0
                            + 12.0 * plugin.manifest.name.chars().count() as f32
                            + 12.0,
                        rect.top() + 16.0,
                    ),
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
            ui.painter()
                .circle_filled(Pos2::new(knob_x, toggle_rect.center().y), 8.0, Color32::WHITE);
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

fn tab_bar_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    let p = app.palette.clone();
    let h = 38.0;
    let (bar_rect, _) = ui.allocate_exact_size(Vec2::new(ui.available_width(), h), Sense::hover());
    ui.painter().rect_filled(bar_rect, 0.0, p.c(&p.tab_bar_bg));

    let mut x = bar_rect.left();
    let count = app.tabs.len();
    let pointer = ui.input(|i| i.pointer.latest_pos().unwrap_or_default());
    for i in 0..count {
        let tab = &app.tabs[i];
        let active = i == app.active;
        let title = tab.display_name() + if tab.dirty { " •" } else { "" };
        let text_w = 12.5 * title.chars().count() as f32;
        let w = text_w + 44.0;
        let rect = Rect::from_min_size(Pos2::new(x, bar_rect.top()), Vec2::new(w, h));

        if active {
            ui.painter().rect_filled(rect, 0.0, p.c(&p.editor_bg));
        } else if rect.contains(pointer) {
            ui.painter().rect_filled(rect, 0.0, p.c(&p.tab_hover_bg));
        }

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

        let close_rect = Rect::from_center_size(
            Pos2::new(rect.right() - 14.0, rect.center().y),
            Vec2::splat(16.0),
        );
        let close_resp = ui.allocate_rect(close_rect, Sense::click());
        if close_resp.hovered() {
            ui.painter()
                .circle_filled(close_rect.center(), 8.0, Color32::from_gray(90));
            icons::draw(ui.painter(), close_rect, Icon::Close, Color32::WHITE);
        } else {
            icons::draw(ui.painter(), close_rect, Icon::Close, p.c(&p.tab_fg));
        }
        let tab_click = ui.allocate_rect(rect, Sense::click());
        let middle = tab_click.middle_clicked();
        if close_resp.clicked() || middle {
            app.close_active();
            return;
        }

        if tab_click.clicked() {
            app.active = i;
        }

        x += w + 2.0;
    }

    let plus_rect =
        Rect::from_center_size(Pos2::new(x + 14.0, bar_rect.center().y), Vec2::splat(24.0));
    let plus_resp = ui.allocate_rect(plus_rect, Sense::click());
    if plus_resp.hovered() {
        ui.painter()
            .circle_filled(plus_rect.center(), 12.0, p.c(&p.tab_hover_bg));
    }
    icons::draw(ui.painter(), plus_rect, Icon::Plus, p.c(&p.tab_fg));
    if plus_resp.clicked() {
        app.new_document();
    }
}

fn editor_area_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    let Some(tab_index) = (0..app.tabs.len()).find(|&i| i == app.active) else {
        return;
    };
    let p = app.palette.clone();
    let font_size = app.font_size;

    if let Some(texture) = app.tabs[tab_index].image.clone() {
        let size = texture.size_vec2();
        let avail = ui.available_size() - Vec2::new(40.0, 40.0);
        let scale = (avail.x / size.x).min(avail.y / size.y).min(1.0);
        ui.with_layout(egui::Layout::top_down_justified(egui::Align::Center), |ui| {
            ui.add_space(20.0);
            ui.add(
                egui::Image::from_texture(&texture)
                    .fit_to_exact_size(Vec2::new(size.x * scale, size.y * scale)),
            );
        });
        return;
    }

    if app.find.open {
        find_bar_ui(app, ui, tab_index);
    }

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

    let tab = &app.tabs[tab_index];
    if tab.untitled && tab.code.is_empty() {
        ui.painter().text(
            Pos2::new(
                output.rect.left() + 56.0,
                output.rect.top() + 10.0,
            ),
            egui::Align2::LEFT_TOP,
            "开始输入以关闭此提示。点击状态栏的语言可更改语法高亮。",
            egui::FontId::monospace(font_size),
            p.c(&p.title_bar_muted_fg),
        );
    }
}

fn find_bar_ui(app: &mut HutaomuApp, ui: &mut egui::Ui, tab_index: usize) {
    let p = app.palette.clone();
    let (rect, _) = ui.allocate_exact_size(Vec2::new(ui.available_width(), 44.0), Sense::hover());
    ui.painter().rect_filled(rect, 0.0, p.c(&p.title_bar_bg));

    ui.horizontal(|ui| {
        ui.add_space(12.0);
        let find_resp = TextEdit::singleline(&mut app.find.query)
            .desired_width(200.0)
            .font(egui::FontId::monospace(13.0))
            .hint_text("查找…")
            .show(ui)
            .response;
        if find_resp.changed() {
            let needle = app.find.query.to_lowercase();
            app.find.count = if needle.is_empty() {
                0
            } else {
                app.tabs[tab_index]
                    .code
                    .to_lowercase()
                    .matches(&needle)
                    .count()
            };
        }
        ui.label(
            egui::RichText::new(format!("{} 项匹配", app.find.count))
                .size(11.5)
                .color(p.c(&p.title_bar_muted_fg)),
        );
        if app.find.replace_mode {
            ui.add_space(6.0);
            TextEdit::singleline(&mut app.find.replacement)
                .desired_width(200.0)
                .font(egui::FontId::monospace(13.0))
                .hint_text("替换为…")
                .show(ui);
            if ui.button("全部替换").clicked() {
                let needle = app.find.query.clone();
                if !needle.is_empty() {
                    let n = app.tabs[tab_index].code.matches(&needle).count();
                    let replaced =
                        app.tabs[tab_index].code.replace(&needle, &app.find.replacement);
                    app.tabs[tab_index].code = replaced;
                    app.tabs[tab_index].dirty = true;
                    app.find.count = 0;
                    app.set_status(format!("已替换 {} 处", n));
                }
            }
        }
        if ui.button("×").clicked() {
            app.find.open = false;
        }
    });
    if ui.input(|i| i.key_pressed(Key::Escape)) {
        app.find.open = false;
    }
}

fn markdown_preview_ui(app: &mut HutaomuApp, ui: &mut egui::Ui) {
    let Some(tab) = app.tabs.get(app.active) else { return };
    let blocks = markdown::parse(&tab.code);
    let fg = app.palette.c(&app.palette.editor_fg);
    let accent = app.palette.accent();
    let muted = app.palette.c(&app.palette.title_bar_muted_fg);
    ScrollArea::vertical().id_source("md_preview").show(ui, |ui| {
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

// ---------------- 状态栏 ----------------

fn bottom_status_bar(app: &mut HutaomuApp, ctx: &Context) {
    let p = app.palette.clone();
    egui::TopBottomPanel::bottom("status_bar")
        .exact_height(26.0)
        .frame(egui::Frame::none().fill(p.c(&p.status_bar_bg)))
        .show(ctx, |ui| {
            ui.horizontal_centered(|ui| {
                ui.add_space(12.0);
                if let Some((msg, t)) = &app.status_msg {
                    if t.elapsed().as_secs() < 6 {
                        ui.label(
                            egui::RichText::new(msg).size(11.5).color(p.c(&p.status_bar_fg)),
                        );
                    }
                }
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    ui.add_space(12.0);
                    let tab = app.tabs.get(app.active);
                    ui.label(
                        egui::RichText::new("行 1, 列 1")
                            .size(11.5)
                            .color(p.c(&p.status_bar_fg)),
                    );
                    let eol = tab.map(|t| t.eol.display()).unwrap_or("LF");
                    ui.label(
                        egui::RichText::new(eol).size(11.5).color(p.c(&p.status_bar_fg)),
                    );
                    let enc = tab.map(|t| t.encoding.display()).unwrap_or("UTF-8");
                    ui.label(
                        egui::RichText::new(enc).size(11.5).color(p.c(&p.status_bar_fg)),
                    );
                    let lang = tab
                        .map(|t| {
                            if t.language_name.is_empty() {
                                "纯文本".to_string()
                            } else {
                                t.language_name.clone()
                            }
                        })
                        .unwrap_or_else(|| "纯文本".to_string());
                    let resp = ui.add(
                        egui::Button::new(
                            egui::RichText::new(lang)
                                .size(11.5)
                                .color(p.c(&p.status_bar_fg))
                                .underline(),
                        )
                        .fill(Color32::TRANSPARENT),
                    );
                    if resp.clicked() {
                        app.overlay = Some(OverlayKind::LanguagePicker);
                    }
                });
            });
        });
}
