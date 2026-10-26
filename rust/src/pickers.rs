// Hutaomu Editor (Rust) - 覆盖层选择器：命令面板 / 快速打开 / 语言模式。
use egui::{Align2, Color32, Context, FontId, Id, Key, Pos2, Rect, Vec2};

use crate::app::{HutaomuApp, SidebarPage};


#[derive(Clone, PartialEq)]
pub enum OverlayKind {
    Commands,
    QuickOpen,
    LanguagePicker,
}

pub struct PickerResult {
    pub language: Option<String>,
}

struct Entry {
    id: String,
    title: String,
    subtitle: String,
}

impl HutaomuApp {
    fn command_entries(&self) -> Vec<Entry> {
        vec![
            Entry { id: "new".into(), title: "新建文件".into(), subtitle: "Ctrl+N".into() },
            Entry { id: "open".into(), title: "打开文件…".into(), subtitle: "Ctrl+O".into() },
            Entry { id: "open_folder".into(), title: "打开文件夹…".into(), subtitle: "Ctrl+Shift+O".into() },
            Entry { id: "save".into(), title: "保存".into(), subtitle: "Ctrl+S".into() },
            Entry { id: "save_as".into(), title: "另存为…".into(), subtitle: "Ctrl+Shift+S".into() },
            Entry { id: "close".into(), title: "关闭标签".into(), subtitle: "Ctrl+W".into() },
            Entry { id: "sidebar".into(), title: "切换侧边栏".into(), subtitle: "Ctrl+B".into() },
            Entry { id: "lang".into(), title: "更改语言模式…".into(), subtitle: String::new() },
            Entry { id: "preview".into(), title: "Markdown：预览".into(), subtitle: String::new() },
            Entry { id: "source".into(), title: "Markdown：源码".into(), subtitle: String::new() },
        ]
    }

    fn execute_command(&mut self, id: &str) {
        match id {
            "new" => self.new_document(),
            "open" => self.open_file_dialog(),
            "open_folder" => self.open_folder_dialog(),
            "save" => self.save_active(false),
            "save_as" => self.save_active(true),
            "close" => self.close_active(),
            "sidebar" => self.sidebar_visible = !self.sidebar_visible,
            "lang" => self.overlay = Some(OverlayKind::LanguagePicker),
            "preview" => self.md_mode = crate::app::MdMode::Preview,
            "source" => self.md_mode = crate::app::MdMode::Source,
            _ => {}
        }
    }
}

const OVERLAY_ID: &str = "hutaomu_overlay";

pub fn draw_overlay(app: &mut HutaomuApp, ctx: &Context, kind: OverlayKind) {
    let screen = ctx.screen_rect();
    // 遮罩
    egui::Area::new(Id::new(OVERLAY_ID).with("mask"))
        .order(egui::Order::Foreground)
        .fixed_pos(screen.left_top())
        .show(ctx, |ui| {
            let (rect, resp) =
                ui.allocate_exact_size(screen.size(), egui::Sense::click());
            ui.painter()
                .rect_filled(rect, 0.0, Color32::from_rgba_unmultiplied(0, 0, 0, 40));
            if resp.clicked() {
                app.overlay = None;
            }
        });

    let (title, placeholder): (&str, &str) = match kind {
        OverlayKind::Commands => ("命令面板", "输入命令名…"),
        OverlayKind::QuickOpen => ("快速打开", "输入文件名…（工作区内）"),
        OverlayKind::LanguagePicker => ("选择语言模式", "输入语言名…（如 C++ / python）"),
    };

    let box_w = 560.0;
    let box_h = 420.0;
    let box_rect = Rect::from_center_size(
        Pos2::new(screen.center().x, screen.top() + 120.0 + box_h / 2.0),
        Vec2::new(box_w, box_h),
    );

    egui::Area::new(Id::new(OVERLAY_ID).with("box"))
        .order(egui::Order::Tooltip)
        .fixed_pos(box_rect.left_top())
        .show(ctx, |ui| {
            let p = app.palette.clone();
            egui::Frame::default()
                .fill(p.c(&p.menu_bg))
                .stroke(egui::Stroke::new(1.0, p.c(&p.menu_border)))
                .rounding(p.corner_radius_large as f32)
                .inner_margin(egui::Margin::symmetric(10, 10))
                .show(ui, |ui| {
                    ui.set_width(box_w - 20.0);
                    ui.set_height(box_h - 20.0);

                    let mut query = ui
                        .memory(|m| {
                            m.data
                                .get_temp::<String>(Id::new(OVERLAY_ID).with("query"))
                                .unwrap_or_default()
                        })
                        .clone();
                    let mut committed: Option<String> = None;
                    let mut closed = false;

                    egui::TextEdit::singleline(&mut query)
                        .id(Id::new(OVERLAY_ID).with("input"))
                        .hint_text(placeholder)
                        .font(FontId::proportional(14.0))
                        .desired_width(f32::INFINITY)
                        .show(ui);

                    ui.add_space(6.0);

                    // 条目
                    let entries: Vec<Entry> = match kind {
                        OverlayKind::Commands => app.command_entries(),
                        OverlayKind::QuickOpen => app
                            .sidebar
                            .entries
                            .iter()
                            .map(|f| Entry {
                                id: f.path.clone(),
                                title: f.name.clone(),
                                subtitle: f.path.clone(),
                            })
                            .collect(),
                        OverlayKind::LanguagePicker => app
                            .syntax_set
                            .syntaxes()
                            .iter()
                            .map(|s| Entry {
                                id: s.name.clone(),
                                title: s.name.clone(),
                                subtitle: s.file_extensions.join(" ."),
                            })
                            .collect(),
                    };
                    let needle = query.trim().to_lowercase();
                    let filtered: Vec<&Entry> = entries
                        .iter()
                        .filter(|e| {
                            needle.is_empty()
                                || e.title.to_lowercase().contains(&needle)
                                || e.id.to_lowercase().contains(&needle)
                        })
                        .take(60)
                        .collect();

                    egui::ScrollArea::vertical().show(ui, |ui| {
                        for (i, entry) in filtered.iter().enumerate() {
                            let (rect, resp) = ui.allocate_exact_size(
                                Vec2::new(ui.available_width(), 30.0),
                                egui::Sense::click(),
                            );
                            if i == 0 {
                                ui.painter().rect_filled(
                                    rect,
                                    6.0,
                                    p.c(&p.menu_item_hover_bg),
                                );
                            }
                            if resp.hovered() {
                                ui.painter().rect_filled(
                                    rect,
                                    6.0,
                                    p.c(&p.menu_item_hover_bg),
                                );
                            }
                            ui.painter().text(
                                Pos2::new(rect.left() + 10.0, rect.center().y),
                                Align2::LEFT_CENTER,
                                &entry.title,
                                FontId::proportional(13.0),
                                p.c(&p.editor_fg),
                            );
                            if !entry.subtitle.is_empty() {
                                ui.painter().text(
                                    Pos2::new(rect.right() - 10.0, rect.center().y),
                                    Align2::RIGHT_CENTER,
                                    &entry.subtitle,
                                    FontId::proportional(11.0),
                                    p.c(&p.title_bar_muted_fg),
                                );
                            }
                            if resp.clicked() {
                                committed = Some(entry.id.clone());
                            }
                        }
                    });

                    // 输入状态回写与按键处理
                    ui.memory_mut(|m| {
                        m.data
                            .insert_temp(Id::new(OVERLAY_ID).with("query"), query.clone())
                    });

                    let enter = ui.input(|i| i.key_pressed(Key::Enter));
                    let escape = ui.input(|i| i.key_pressed(Key::Escape));
                    let down = ui.input(|i| i.key_pressed(Key::ArrowDown));
                    let _ = down;
                    if escape {
                        closed = true;
                    }
                    if enter {
                        if let Some(first) = filtered.first() {
                            committed = Some(first.id.clone());
                        }
                    }
                    if let Some(id) = committed {
                        match kind {
                            OverlayKind::Commands => app.execute_command(&id),
                            OverlayKind::QuickOpen => app.open_path(&id),
                            OverlayKind::LanguagePicker => {
                                if let Some(tab) = app.tabs.get_mut(app.active) {
                                    if let Some(syntax) =
                                        app.syntax_set.find_syntax_by_name(&id)
                                    {
                                        tab.language_name = syntax.name.clone();
                                        tab.language_id = syntax
                                            .name
                                            .to_lowercase()
                                            .replace(' ', "-");
                                    }
                                }
                                app.set_status("语言模式已切换。".into());
                            }
                        }
                        closed = true;
                    }
                    if closed {
                        app.overlay = None;
                        ui.memory_mut(|m| {
                            m.data
                                .remove::<String>(Id::new(OVERLAY_ID).with("query"))
                        });
                        ctx.request_repaint();
                    }
                });
        });

}
