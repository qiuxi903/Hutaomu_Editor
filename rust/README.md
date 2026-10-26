# Hutaomu Editor — Rust 版

> 主仓库（Qt/C++ 版）的 Rust 重构，位于 `rust/` 目录，**不依赖 Qt**，UI 全部由 egui 自绘。

## 技术栈

| 组件 | 选型 |
|---|---|
| GUI 框架 | egui 0.32 / eframe（纯 Rust 自绘，无系统控件风格） |
| 语法高亮 | syntect 5（Sublime Text 语法定义，数十种语言） |
| Markdown 预览 | pulldown-cmark（自绘块渲染） |
| 主题 | 与主仓库同源的 JSON 调色板（`themes/paper.json`），serde 加载 |
| 文件对话框 | rfd（原生对话框） |

## 构建

```bash
cd rust
cargo build --release
# 产物：target/release/hutaomu-editor.exe
```

依赖 rustc ≥ 1.95（MSVC 工具链）。

## 已实现（对标 Qt 版）

- 无边框自绘标题栏（拖拽/双击最大化/最小化/关闭，悬停圆角钮）
- 活动栏（资源管理器/搜索/大纲/插件，底部关于）+ 可收起侧栏
- Chrome 风标签：圆角、按内容宽度、关闭钮、语言徽章、未命名语言后缀
- 编辑器：syntect 高亮、行号列、当前行高亮、占位提示（未命名空文件）
- 语言模式切换（syntect 全部语法，状态栏点击/命令面板入口）
- Markdown 预览（右侧分栏）、大纲面板（标题提取）
- 全局搜索（工作区递归，300 条上限）
- 命令面板（Ctrl+Shift+P）/ 快速打开（Ctrl+P）
- 插件面板（声明式 manifest 扫描与启停）
- 状态栏（光标/编码/语言项可点击）
- 快捷键：Ctrl+N/O/S/Shift+S/W/B/P、Ctrl+=/-/0、Esc

## 暂未迁移（后续迭代）

Office 查看器（docx/xlsx/pptx）、PDF 查看器、.htmtpi 主题包/.htmed 插件包安装、
触屏手势、文件关联与安装器——这些需要逐个用 Rust 生态替代方案重写。
