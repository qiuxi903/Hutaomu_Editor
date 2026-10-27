// Hutaomu Editor (Rust) - 入口。
// SPDX-License-Identifier: AGPL-3.0-only
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use hutaomu_editor::app;

fn main() {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_decorations(false)
            .with_min_inner_size([960.0, 600.0])
            .with_inner_size([1440.0, 900.0]),
        ..Default::default()
    };
    eframe::run_native(
        "Hutaomu Editor",
        options,
        Box::new(|cc| Ok(Box::new(app::HutaomuApp::new(cc)))),
    )
    .expect("eframe 启动失败");
}
