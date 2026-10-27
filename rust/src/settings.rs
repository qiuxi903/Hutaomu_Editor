// Hutaomu Editor (Rust) - 设置持久化（%APPDATA%/HutaomuEditorRust/settings.json）。
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone)]
pub struct Settings {
    #[serde(default = "d_true")]
    pub sidebar_visible: bool,
    #[serde(default = "d_font")]
    pub font_size: f32,
    #[serde(default)]
    pub word_wrap: bool,
    #[serde(default = "d_preview")]
    pub md_preview: bool,
    #[serde(default = "d_theme")]
    pub theme_id: String,
    #[serde(default)]
    pub maximized: bool,
    #[serde(default)]
    pub workspace: Option<String>,
    #[serde(default)]
    pub recent: Vec<String>,
}

fn d_true() -> bool { true }
fn d_font() -> f32 { 14.0 }
fn d_preview() -> bool { false }
fn d_theme() -> String { "paper".into() }

impl Default for Settings {
    fn default() -> Self {
        serde_json::from_str("{}").unwrap()
    }
}

pub fn settings_path() -> Option<std::path::PathBuf> {
    dirs::data_dir().map(|d| d.join("HutaomuEditorRust").join("settings.json"))
}

impl Settings {
    pub fn load() -> Self {
        settings_path()
            .and_then(|p| std::fs::read_to_string(p).ok())
            .and_then(|s| serde_json::from_str(&s).ok())
            .unwrap_or_default()
    }

    pub fn load_from(path: &std::path::Path) -> Self {
        std::fs::read_to_string(path)
            .ok()
            .and_then(|s| serde_json::from_str(&s).ok())
            .unwrap_or_default()
    }

    pub fn save(&self) {
        if let Some(path) = settings_path() {
            if let Some(dir) = path.parent() {
                let _ = std::fs::create_dir_all(dir);
            }
            if let Ok(json) = serde_json::to_string_pretty(self) {
                let _ = std::fs::write(path, json);
            }
        }
    }

    pub fn push_recent(&mut self, path: &str) {
        self.recent.retain(|p| p != path);
        self.recent.insert(0, path.to_string());
        self.recent.truncate(10);
    }
}
