// Hutaomu Editor (Rust) - 插件系统：声明式 manifest + .htmed 安装包（ZIP）。
// 对标 Qt 版 PluginManager：peek→校验→确认→解压安装，64MB/4000 条目/路径安全校验。
// SPDX-License-Identifier: AGPL-3.0-only
use serde::Deserialize;
use std::io::Read;

#[derive(Deserialize, Clone, Default)]
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
    pub license: String,
    #[serde(default)]
    pub permissions: Vec<String>,
    #[serde(default)]
    pub builtin: bool,
    /// 贡献点原始 JSON（安装确认摘要展示用）
    #[serde(default)]
    pub contributes: serde_json::Value,
}

#[derive(Clone)]
pub struct InstalledPlugin {
    pub manifest: PluginManifest,
    pub root_path: String,
    pub builtin: bool,
}

/// 待确认安装的插件包
pub struct PendingInstall {
    pub path: String,
    pub manifest: PluginManifest,
    pub overwrite: bool,
}

/// 已知权限（未知权限丢弃，与 Qt 版一致）
pub const KNOWN_PERMISSIONS: [&str; 4] = ["fs.read", "fs.write", "net", "shell"];

/// 包大小上限 64MB / 条目上限 4000（与 Qt 版一致）
pub const MAX_PACKAGE_BYTES: u64 = 64 * 1024 * 1024;
pub const MAX_PACKAGE_ENTRIES: usize = 4000;

/// 用户插件根目录
pub fn user_plugins_dir() -> std::path::PathBuf {
    dirs::data_dir()
        .unwrap_or_else(std::env::temp_dir)
        .join("HutaomuEditorRust")
        .join("plugins")
}

/// 扫描 <程序目录>/plugins 与 <AppData>/plugins 下的目录式插件
pub fn scan() -> Vec<InstalledPlugin> {
    let mut out = Vec::new();
    let mut roots = Vec::new();
    if let Ok(exe) = std::env::current_exe() {
        roots.push((exe.parent().unwrap().join("plugins"), true));
    }
    roots.push((user_plugins_dir(), false));
    for (root, builtin_root) in roots {
        if let Ok(rd) = std::fs::read_dir(&root) {
            for entry in rd.flatten() {
                let manifest_path = entry.path().join("plugin.json");
                let Ok(text) = std::fs::read_to_string(&manifest_path) else {
                    continue;
                };
                let Ok(mut manifest) = serde_json::from_str::<PluginManifest>(&text) else {
                    continue;
                };
                manifest.builtin = builtin_root;
                out.push(InstalledPlugin {
                    manifest,
                    root_path: entry.path().to_string_lossy().to_string(),
                    builtin: builtin_root,
                });
            }
        }
    }
    out
}

/// 贡献点摘要（安装确认/详情页用）
pub struct ContributionSummary {
    pub commands: usize,
    pub formats: Vec<String>,
    pub themes: usize,
    pub pages: usize,
    pub status_bar: usize,
}

impl PluginManifest {
    pub fn contributions_summary(&self) -> ContributionSummary {
        let c = &self.contributes;
        let arr_len = |key: &str| -> usize {
            c.get(key).and_then(|v| v.as_array()).map(|a| a.len()).unwrap_or(0)
        };
        let formats = c
            .get("formats")
            .and_then(|v| v.as_array())
            .map(|a| {
                a.iter()
                    .filter_map(|f| {
                        let ext = f.get("extension")?.as_str()?;
                        Some(format!("*.{}", ext.trim_start_matches('*').trim_start_matches('.')))
                    })
                    .collect()
            })
            .unwrap_or_default();
        ContributionSummary {
            commands: arr_len("commands"),
            formats,
            themes: arr_len("themes"),
            pages: arr_len("pages"),
            status_bar: arr_len("statusBar"),
        }
    }

    pub fn valid_permissions(&self) -> Vec<String> {
        self.permissions
            .iter()
            .filter(|p| KNOWN_PERMISSIONS.contains(&p.as_str()))
            .cloned()
            .collect()
    }
}

/// 清单校验（Qt 规则：id=publisher.name 且 publisher 为前缀；版本宽松语义化）
pub fn validate_manifest(m: &PluginManifest) -> Result<(), String> {
    if m.id.is_empty() || m.name.is_empty() || m.version.is_empty() || m.publisher.is_empty() {
        return Err("清单缺少必填字段（id/name/version/publisher）".into());
    }
    let expected_prefix = format!("{}.", m.publisher);
    if !m.id.starts_with(&expected_prefix) || m.id == expected_prefix {
        return Err(format!("插件 id 必须为 publisher.name 格式且与发布者一致（应为 {}.*）", m.publisher));
    }
    let mut parts = 0;
    let mut ok = !m.version.is_empty();
    for seg in m.version.split('.') {
        parts += 1;
        if seg.is_empty() || !seg.chars().all(|c| c.is_ascii_digit()) || parts > 4 {
            ok = false;
            break;
        }
    }
    if !ok {
        return Err("版本号格式应为 1 / 1.2 / 1.2.3 / 1.2.3.4".into());
    }
    Ok(())
}

/// 从 .htmed（ZIP）或目录读取清单（peek，不落盘）
pub fn peek_manifest(path: &str) -> Result<PluginManifest, String> {
    let p = std::path::Path::new(path);
    if p.is_dir() {
        let text = std::fs::read_to_string(p.join("plugin.json"))
            .map_err(|_| "目录中未找到 plugin.json".to_string())?;
        return serde_json::from_str(&text).map_err(|e| format!("清单解析失败：{}", e));
    }
    let file = std::fs::File::open(p).map_err(|e| e.to_string())?;
    let mut zip = zip::ZipArchive::new(std::io::BufReader::new(file))
        .map_err(|_| "无法读取插件包（需要 .htmed 或 .zip）".to_string())?;
    // 清单位置：包根或唯一的一级子目录
    let manifest_idx = (0..zip.len()).find(|&i| {
        zip.by_index(i).ok().map(|f| {
            let n = f.name().to_string();
            n == "plugin.json" || n.ends_with("/plugin.json") && n.matches('/').count() == 1
        }) == Some(true)
    });
    let Some(idx) = manifest_idx else {
        return Err("插件包中未找到 plugin.json".into());
    };
    let mut text = String::new();
    zip.by_index(idx)
        .map_err(|e| e.to_string())?
        .read_to_string(&mut text)
        .map_err(|e| e.to_string())?;
    serde_json::from_str(&text).map_err(|e| format!("清单解析失败：{}", e))
}

/// 安装 .htmed 包到用户插件目录（含安全校验，失败回滚）
pub fn install_package(path: &str, manifest: &PluginManifest, overwrite: bool) -> Result<String, String> {
    let file = std::fs::File::open(path).map_err(|e| e.to_string())?;
    let mut zip = zip::ZipArchive::new(std::io::BufReader::new(file))
        .map_err(|_| "无法读取插件包".to_string())?;
    if zip.len() > MAX_PACKAGE_ENTRIES {
        return Err(format!("插件包条目过多（上限 {}）", MAX_PACKAGE_ENTRIES));
    }
    // 包内相对插件根：清单在根或唯一一级子目录
    let prefix = (0..zip.len())
        .filter_map(|i| {
            let name = zip.by_index(i).ok()?.name().to_string();
            if name.ends_with("/plugin.json") && name.matches('/').count() == 1 {
                Some(name.split("/plugin.json").next().unwrap().to_string() + "/")
            } else {
                None
            }
        })
        .next()
        .unwrap_or_default();

    let total: u64 = (0..zip.len())
        .filter_map(|i| zip.by_index(i).ok().map(|f| f.size()))
        .sum();
    if total > MAX_PACKAGE_BYTES {
        return Err("插件包超过 64MB 上限".into());
    }

    let target = user_plugins_dir().join(&manifest.id);
    if target.exists() {
        if !overwrite {
            return Err("目标已存在且未允许覆盖".into());
        }
        std::fs::remove_dir_all(&target).map_err(|e| e.to_string())?;
    }
    std::fs::create_dir_all(&target).map_err(|e| e.to_string())?;

    for i in 0..zip.len() {
        let mut entry = zip.by_index(i).map_err(|e| e.to_string())?;
        let name = entry.name().to_string();
        if name.starts_with("__MACOSX") || name.ends_with('/') {
            continue;
        }
        let rel = name.strip_prefix(&prefix).unwrap_or(&name);
        if rel.is_empty() {
            continue;
        }
        // 路径安全：禁止绝对路径/盘符/..
        let rel_path = std::path::Path::new(rel);
        if rel_path.is_absolute()
            || rel.contains("..")
            || rel.contains(':')
            || rel.starts_with('/')
            || rel.starts_with('\\')
        {
            let _ = std::fs::remove_dir_all(&target);
            return Err(format!("插件包内含非法路径：{}", rel));
        }
        let out_path = target.join(rel_path);
        if let Some(parent) = out_path.parent() {
            std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        }
        let mut out = std::fs::File::create(&out_path).map_err(|e| e.to_string())?;
        std::io::copy(&mut entry, &mut out).map_err(|e| e.to_string())?;
    }
    Ok(target.to_string_lossy().to_string())
}

/// 导出插件目录为 .htmed 包
pub fn export_package(root_path: &str, out_path: &str) -> Result<(), String> {
    let file = std::fs::File::create(out_path).map_err(|e| e.to_string())?;
    let mut zip = zip::ZipWriter::new(file);
    let options: zip::write::SimpleFileOptions = zip::write::SimpleFileOptions::default()
        .compression_method(zip::CompressionMethod::Deflated);
    let root = std::path::Path::new(root_path);
    let mut stack = vec![root.to_path_buf()];
    while let Some(dir) = stack.pop() {
        for entry in std::fs::read_dir(&dir).map_err(|e| e.to_string())?.flatten() {
            let p = entry.path();
            let rel = p.strip_prefix(root).unwrap_or(&p).to_string_lossy().replace('\\', "/");
            if p.is_dir() {
                zip.add_directory(rel.clone(), options)
                    .map_err(|e| e.to_string())?;
                stack.push(p);
            } else {
                zip.start_file(rel, options).map_err(|e| e.to_string())?;
                std::io::copy(&mut std::fs::File::open(&p).map_err(|e| e.to_string())?, &mut zip)
                    .map_err(|e| e.to_string())?;
            }
        }
    }
    zip.finish().map_err(|e| e.to_string())?;
    Ok(())
}

// ---------------- .htmtpi 主题包（Qt ThemeManager 对齐） ----------------

/// 主题目录（<程序目录>/themes 与 <AppData>/themes，用户侧可写）
pub fn user_themes_dir() -> std::path::PathBuf {
    dirs::data_dir()
        .unwrap_or_else(std::env::temp_dir)
        .join("HutaomuEditorRust")
        .join("themes")
}

/// 列出全部主题 JSON 路径：扁平 themes/*.json + 目录 themes/*/theme.json
pub fn all_theme_paths() -> Vec<std::path::PathBuf> {
    let mut out = Vec::new();
    let mut roots = Vec::new();
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            // 开发环境：rust/target/{debug,release} → 仓库 themes/
            for up in ["../../..", "../../../.."] {
                roots.push(dir.join(up).join("themes"));
            }
            roots.push(dir.join("themes"));
        }
    }
    roots.push(std::path::PathBuf::from("themes"));
    roots.push(user_themes_dir());
    for root in roots {
        let Ok(rd) = std::fs::read_dir(&root) else { continue };
        for entry in rd.flatten() {
            let p = entry.path();
            if p.is_dir() {
                let t = p.join("theme.json");
                if t.is_file() {
                    out.push(t);
                }
            } else if p.extension().map(|e| e == "json").unwrap_or(false) {
                out.push(p);
            }
        }
    }
    out
}

/// 导入 .htmtpi 主题包：清单在包根或唯一一级子目录，解压到用户主题目录
pub fn import_theme_package(path: &str) -> Result<String, String> {
    let file = std::fs::File::open(path).map_err(|e| e.to_string())?;
    let mut zip = zip::ZipArchive::new(std::io::BufReader::new(file))
        .map_err(|_| "无法读取主题包（需要 .htmtpi 或 .zip）".to_string())?;
    if zip.len() > MAX_PACKAGE_ENTRIES {
        return Err("主题包条目过多".into());
    }
    let prefix = (0..zip.len())
        .filter_map(|i| {
            let name = zip.by_index(i).ok()?.name().to_string();
            if name.ends_with("theme.json") && name.matches('/').count() <= 1 {
                Some(name.split("theme.json").next().unwrap_or("").to_string())
            } else {
                None
            }
        })
        .next()
        .unwrap_or_default();
    let Some(idx) = (0..zip.len()).find(|&i| {
        zip.by_index(i)
            .ok()
            .map(|f| f.name().to_string())
            .map(|n| format!("{}theme.json", prefix) == n)
            == Some(true)
    }) else {
        return Err("主题包中未找到 theme.json".into());
    };
    let mut text = String::new();
    zip.by_index(idx)
        .map_err(|e| e.to_string())?
        .read_to_string(&mut text)
        .map_err(|e| e.to_string())?;
    let id = serde_json::from_str::<serde_json::Value>(&text)
        .ok()
        .and_then(|v| v.get("id").and_then(|i| i.as_str()).map(|s| s.to_string()))
        .ok_or("theme.json 缺少 id 字段")?;
    if id.contains("..") || id.contains('/') || id.contains('\\') || id.is_empty() {
        return Err("theme id 非法".into());
    }
    let target = user_themes_dir().join(&id);
    if target.exists() {
        std::fs::remove_dir_all(&target).map_err(|e| e.to_string())?;
    }
    std::fs::create_dir_all(&target).map_err(|e| e.to_string())?;
    let total: u64 = (0..zip.len()).filter_map(|i| zip.by_index(i).ok().map(|f| f.size())).sum();
    if total > MAX_PACKAGE_BYTES {
        let _ = std::fs::remove_dir_all(&target);
        return Err("主题包超过 64MB 上限".into());
    }
    for i in 0..zip.len() {
        let mut entry = zip.by_index(i).map_err(|e| e.to_string())?;
        let name = entry.name().to_string();
        if name.starts_with("__MACOSX") || name.ends_with('/') {
            continue;
        }
        let rel = name.strip_prefix(&prefix).unwrap_or(&name);
        if rel.is_empty() || rel.contains("..") || rel.contains(':') || rel.starts_with('/') {
            let _ = std::fs::remove_dir_all(&target);
            return Err(format!("主题包内含非法路径：{}", rel));
        }
        let out_path = target.join(rel);
        if let Some(parent) = out_path.parent() {
            std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        }
        let mut out = std::fs::File::create(&out_path).map_err(|e| e.to_string())?;
        std::io::copy(&mut entry, &mut out).map_err(|e| e.to_string())?;
    }
    Ok(id)
}

/// 导出主题（扁平 json 或目录形态）为 .htmtpi
pub fn export_theme(theme_json_path: &str, out_path: &str) -> Result<(), String> {
    let src = std::path::Path::new(theme_json_path);
    let base = src.parent().unwrap_or(std::path::Path::new("."));
    let file = std::fs::File::create(out_path).map_err(|e| e.to_string())?;
    let mut zip = zip::ZipWriter::new(file);
    let options: zip::write::SimpleFileOptions = zip::write::SimpleFileOptions::default()
        .compression_method(zip::CompressionMethod::Deflated);
    // theme.json 本体
    zip.start_file("theme.json", options).map_err(|e| e.to_string())?;
    std::io::copy(
        &mut std::fs::File::open(src).map_err(|e| e.to_string())?,
        &mut zip,
    )
    .map_err(|e| e.to_string())?;
    // 同目录资源（背景图/preview 等，扁平主题仅一层）
    let is_dir_theme = src.file_name().map(|f| f == "theme.json").unwrap_or(false);
    if is_dir_theme {
        if let Ok(rd) = std::fs::read_dir(base) {
            for entry in rd.flatten() {
                let p = entry.path();
                if p.is_file() {
                    if let Some(name) = p.file_name().and_then(|n| n.to_str()) {
                        if name != "theme.json" {
                            zip.start_file(name, options).map_err(|e| e.to_string())?;
                            std::io::copy(
                                &mut std::fs::File::open(&p).map_err(|e| e.to_string())?,
                                &mut zip,
                            )
                            .map_err(|e| e.to_string())?;
                        }
                    }
                }
            }
        }
    }
    zip.finish().map_err(|e| e.to_string())?;
    Ok(())
}
