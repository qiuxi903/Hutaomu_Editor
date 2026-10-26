// Hutaomu Editor (Rust) - 插件扫描（声明式 manifest 读取，v1：展示与启停）。
use serde::Deserialize;

#[derive(Deserialize, Clone)]
pub struct PluginManifest {
    #[serde(default)]
    pub id: String,
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub version: String,
    #[serde(default)]
    pub publisher: String,
    #[serde(default)]
    pub description: String,
    #[serde(default)]
    pub builtin: bool,
}

#[derive(Clone)]
pub struct InstalledPlugin {
    pub manifest: PluginManifest,
    pub root_path: String,
    pub builtin: bool,
}

/// 扫描 <程序目录>/plugins 与 <AppData>/plugins 下的目录式插件
pub fn scan() -> Vec<InstalledPlugin> {
    let mut out = Vec::new();
    let mut roots = Vec::new();
    if let Ok(exe) = std::env::current_exe() {
        roots.push(exe.parent().unwrap().join("plugins"));
    }
    if let Some(base) = dirs::data_dir() {
        roots.push(base.join("HutaomuEditorRust").join("plugins"));
    }
    for root in roots {
        if let Ok(rd) = std::fs::read_dir(&root) {
            for entry in rd.flatten() {
                let manifest_path = entry.path().join("plugin.json");
                let Ok(text) = std::fs::read_to_string(&manifest_path) else {
                    continue;
                };
                let Ok(mut manifest) = serde_json::from_str::<PluginManifest>(&text) else {
                    continue;
                };
                manifest.builtin = root.join("plugins").join(&manifest.id) == entry.path()
                    && !entry.path().starts_with(dirs::data_dir().unwrap_or_default());
                let builtin = manifest.builtin;
                out.push(InstalledPlugin {
                    manifest,
                    root_path: entry.path().to_string_lossy().to_string(),
                    builtin,
                });
            }
        }
    }
    out
}
