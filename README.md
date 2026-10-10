# Hutaomu Editor

原生跨平台（当前提供 Windows 版）的文本 / 代码 / Markdown / Office 编辑器，
VS Code 式布局，**中文界面优先**。

- **无 Web 套壳**：Qt 6 Widgets 原生渲染，热启动 ~330ms
- **技术栈**：C++20 + Qt 6.10 + tree-sitter（语法高亮）+ md4c（Markdown）
- **许可证**：[AGPL-3.0-only](LICENSE)（永久开源）
- **默认主题**：简约白，内置主题 + 官方主题包（VS Code / Typora / Obsidian / Half-Life 风格）

## 功能总览

### 编辑器

- **语法高亮**（tree-sitter）：C/C++、C#、Java、JavaScript、TypeScript、Python、Rust、Go、JSON、HTML、CSS、Shell、YAML、TOML、PHP、XML 等
- **Markdown 三视图**：实时预览（所见即所得）、分栏预览、源码模式；表格原位渲染、任务列表、大纲面板联动跳转
- **查找替换**：正则 / 大小写 / 全词、匹配计数与高亮、`$1..$9` 反向引用
- **注释切换**（Ctrl+/ 各语言自动区分）、字号缩放、自动换行、行号、当前行高亮
- **编码**：UTF-8 / BOM / UTF-16 / UTF-32 / GB18030 / Latin-1 自动检测；换行符 LF/CRLF/CR 保持；原子保存；外部修改监听

### 文件查看与编辑（Office 等）

| 类型 | 能力 |
|---|---|
| **.docx** | 富文本渲染（标题/字体/表格/列表/对齐）+ **可视化编辑**（加粗/斜体/字号/颜色工具栏），保存回写 OOXML |
| **.xlsx** | 多工作表网格、合并单元格、列宽行高、单元格样式、数字/日期格式、公式栏与统计 |
| **.pptx** | 幻灯片渲染（形状/图片/文本），翻页与缩放 |
| **.pdf** | pdfium 渲染、翻页缩放 |
| **图片 / 媒体** | 常见图片格式；视频/音频走 Windows Media Foundation |

### 主题系统

主题不只是换色——一个主题（`theme.json` + 可选 `layout.json`）可以带四层能力：

1. **配色**（约 90 个颜色 token，全参数化 QSS）
2. **度量排版**（字号行距、密度、圆角、编辑器字体）
3. **外观资源**（品牌 logo、图标、编辑区背景图 + 不透明度）
4. **界面形态**（面板显隐、标签样式、居中限宽、沉浸写作模式）

- 支持用户**自定义背景**（设置 → 主题 → 背景图）
- 主题包 `.htmtpi`：导入 / 导出 / 删除，即装即用
- 内置：简约白（默认）、青木深色、石墨；官方主题包含 Half-Life 三部曲风格等 7 套

### 插件系统（声明式）

插件为单文件 `.htmed`（ZIP 格式，**不含可执行代码**），可贡献：

- **主题**、**文件格式**（扩展名 → 语言）、**命令**（内置动作 / 需授权的外部命令）、**状态栏部件**、**侧栏页面**
- 命令接入**命令面板**（`Ctrl+Shift+P`）
- 安装时展示贡献点与权限，`shell` 权限需用户确认信任；路径/大小/条目多重安全校验

## 打包分发

- `packaging/windows/build-installer.bat` 一键出包：Inno Setup 安装包（中文向导、文件关联、可选组件）+ 便携 ZIP
- 预装官方插件（主题包 + 配置格式包 + 示例插件），安装即用
- macOS / Linux 打包待后续补充

## 构建（Windows）

依赖：Qt 6.10.3 (MinGW)、CMake、Ninja。

```bat
:: 一键构建（Debug 传参：build.bat Debug）
packaging\windows\build.bat

:: 运行
build\Release\HutaomuEditor.exe [文件路径...]
```

### 从零搭建构建环境

```bat
pip install aqtinstall cmake ninja
python -m aqt install-qt windows desktop 6.10.3 win64_mingw -m qt5compat --outputdir G:\Qt
python -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 --outputdir G:\Qt
```

### 测试

构建后运行（19 个测试套件：编码 / 语法 / 注释 / Markdown / 表格 / 大纲 / 主题 / 插件 / 命令面板 / Office 等）：

```bat
cd build\Release && ctest
```

开发约定见 [CONTRIBUTING.md](CONTRIBUTING.md)；主题格式文档见 [themes/README.md](themes/README.md)；
插件格式文档见 [plugins/README.md](plugins/README.md)。

## 目录结构

```
src/
  main.cpp          入口
  app/              外壳（主窗口、标题栏、设置、命令面板、文件关联）
  core/             文档模型（IO、编码、换行、ZIP 写入）
  editor/           编辑器组件（行号、高亮接入、查找条、注释命令）
  markdown/         Markdown 渲染（md4c→HTML）与实时预览装饰层
  panels/           面板（资源管理器、全局搜索、大纲、预览）
  plugins/          插件管理（清单解析、发现、页面面板）
  settings/         设置（JSON 持久化）
  syntax/           语法引擎（tree-sitter 集成、语言注册表）
  themes/           主题（QSS 模板、色板、主题包、背景管理）
  viewers/          文档查看器（Office/PDF/图片/媒体、ZIP 读取）
tests/              19 个测试套件
packaging/          构建与打包脚本（含 Inno Setup 中文向导）
plugins/            官方插件源码
themes/             官方主题源码
third_party/        tree-sitter、md4c、pdfium、zlib
```

## 许可证

本项目以 [GNU AGPL-3.0](LICENSE)（AGPL-3.0-only）许可开源：
任何修改版（含网络服务形态）都必须继续开放源码。
第三方组件的许可见 [THIRDPARTY.md](THIRDPARTY.md)。
