# Hutaomu Editor

原生跨平台（当前仅 Windows）的文本 / 代码 / Markdown 编辑器，VS Code 式布局 + 自有“青木”视觉主题，**中文界面优先**。

- **无 Web 套壳**：Qt 6 Widgets 原生渲染，冷启动 ~330ms（热）
- **技术栈**：C++20 + Qt 6.10 + tree-sitter（语法高亮）+ md4c（Markdown）
- **界面语言**：中文（基于 Qt tr() 体系，后续可扩展多语言）
**默认主题**：简约白（白色极简 + 青绿强调），内置 3 套可切换
- **许可证**：当前专有（闭源）；计划开源时采用 GPL-3.0-or-later，见 [LICENSE](LICENSE)

## 当前进度

完整计划见 [PLAN.md](PLAN.md)。

| 里程碑 | 状态 |
|---|---|
| M0 骨架：主窗口、多标签、文件读写、编码/换行检测、主题、设置持久化 | ✅ 完成 |
| M1 代码编辑：tree-sitter 高亮（17 语言）、语言识别、注释切换、查找替换 | ✅ 完成 |
| M2 MD 预览：md4c 解析、分栏实时预览、预览内代码高亮、大纲面板 | ✅ 完成 |
| M3 Live Preview：实时预览装饰 + 三视图模式 | ✅ 完成 |
| M4 外壳：文件树、全局搜索、Ctrl+P 快速打开、设置对话框 | ✅ 完成 |
| M5 打包：Windows 便携包已验证 + Inno Setup 安装包脚本就绪 | ✅ Windows 完成 |

> macOS / Linux 打包需要对应平台构建环境，脚本与文档后续补充。

## M1 代码编辑

- **语法高亮**（tree-sitter + 官方高亮 query，配色适配青木主题）：
  C/C++、C#、Java、JavaScript、TypeScript、Python、Rust、Go、JSON、HTML、CSS、Shell、YAML、TOML、PHP、XML
- **语言识别**：扩展名 → shebang → 内容嗅探三级判断，结果显示在状态栏
- **注释切换**：Ctrl+/ 行注释（各语言符号自动区分），Ctrl+Shift+/ 块注释
- **查找替换**：编辑器内浮层，支持正则 / 大小写 / 全词匹配、全文档匹配计数与高亮、全部替换（$1..$9 反向引用）

## M2 Markdown 预览

- md4c（CommonMark + GFM：表格、删除线、任务列表）解析，QTextBrowser 原生渲染，无 Web 引擎
- **代码块使用 tree-sitter 着色**，与编辑器同一套配色体系
- 图片按文档所在目录解析相对路径；标题提取到大纲面板，点击跳转

## 主题系统

- 主题 = **JSON 色板**（约 90 个颜色键）+ 全参数化 QSS 模板（@token 渲染），新增主题只需一个 JSON 文件
- 内置三套：**简约白**（默认，GitHub Light 风语法色）、**青木深色**（品牌深色）、**石墨**（中性深灰 + 雾蓝）
- 设置对话框动态列出全部主题；视图菜单保留深/浅快速切换
- QPalette、语法高亮、大纲、预览、图标全部随主题联动

## 预览面板与编辑区分界

- 预览面板有独立的**"预览"标题栏**（显示当前文档名），与编辑区一目了然
- 分隔条默认可见（@sashColor），悬停变青绿提示可拖拽

## 触屏与导航体验

- **触屏**：编辑器与预览页均支持单指拖动滚动（带惯性衰减）、轻点定位/点击链接；拖动不再误触发文本选择，鼠标操作不受影响
- **捏合缩放**：触控板/触屏双指捏合直接缩放编辑器字号
- **章节导航**：大纲面板高亮光标所在章节并自动滚动定位；预览中点击标题跳回源码对应行；编辑 ↔ 预览滚动比例同步
- **MD 渲染修复**：围栏状态跨编辑正确传播（开/闭围栏字符必须匹配，`` ``` `` 内的 `~~~` 不再误判）；任务列表 ☑/☐ 勾选状态；表格带边框与列对齐；`***粗斜体***` 嵌套格式

## 界面动效

- 标签页切换、侧边栏页面切换、预览面板显示均有 160~200ms 淡入动效（OutCubic），动画结束即移除效果不留渲染开销

## 实时模式表格渲染

- 实时预览下表格**原位渲染为带边框的原生表格**（与分栏预览同一渲染管线），列对齐生效
- 光标移入表格区域自动展开原始 Markdown 便于编辑，移出后恢复渲染（Obsidian 同款交互）
- 折叠预留高度按**渲染后实测高度**两遍收敛回填，底边框完整不被裁

## 大纲面板（完整功能）

- 层级树结构（H1→H6 按级别嵌套），带"大纲"标题栏，替代原列表样式
- 跟踪光标所在章节（加粗 + 选中高亮 + 自动滚动定位）
- 单击跳转源码对应行；代码围栏内的 `#` 不计入

## M3 实时预览（三视图模式）

- **实时预览**（默认）：标题按级别放大着色、粗斜体/删除线/行内代码/链接就地渲染、语法标记在非光标行隐藏（Obsidian 风格）
- **分栏预览**：左侧源码 + 右侧渲染，编辑实时刷新（300ms 防抖）
- **源码模式**：纯 Markdown 编辑
- 视图菜单切换，选择持久化；编辑菜单含 Markdown 加粗/斜体/行内代码快捷包裹

## M4 外壳完善

- 文件树侧边栏（目录优先、单击打开）+ 大纲面板
- **全局搜索**（Ctrl+Shift+F）：全工作区文本搜索，跳过构建目录与二进制文件，结果点击直达行列
- **快速打开**（Ctrl+P）：按路径过滤工作区文件，回车直达
- **设置对话框**（Ctrl+,）：主题 / 编辑器字体字号 / 制表符宽度 / 自动换行 / Markdown 视图模式

## M5 打包

- `dist/HutaomuEditor-0.1.0-win64.zip`：自包含便携包（解压后 66MB），已验证脱离 Qt 环境直接运行
- `packaging/windows/installer.iss`：Inno Setup 安装包脚本（安装到 Program Files、开始菜单/桌面快捷方式、卸载器）
- `packaging/windows/build-installer.bat`：一键构建安装包（构建 → windeployqt → ISCC 出包）
- `packaging/windows/innosetup.exe`：Inno Setup 安装器（首次使用双击安装，之后运行 bat 即可）
- 启动速度：热启动 ~330ms（冷 ~500ms），后续可通过精简部署继续优化

## M0 已实现

**VS Code 式布局 + 自有“青木”视觉**：

- 自定义标题栏：青木叶 Logo + 品牌名 + 中文菜单内嵌 + 居中窗口标题 + 自绘窗控按钮
- 原生窗口行为保留：拖拽移动、边缘缩放、Win11 圆角、DWM 阴影、任务栏贴靠（WM_NCCALCSIZE + WM_NCHITTEST 方案）
- “青木”配色：中性深灰底 + 青绿强调色（状态栏、标签指示条、选中色），深/浅双主题
- 左侧活动栏（资源管理器 + 全局搜索）+ 资源管理器侧边栏（打开文件夹为工作区、目录优先文件树、单击快速打开、欢迎页）
- 标签栏：“+”按钮直接新建标签、可关闭、拖动排序、脏标记（•）、Ctrl+Tab 切换
- 全中文界面（36 处界面文案），UI 字体微软雅黑

**编辑与文件**：

- 文件打开/保存/另存为、拖拽打开、命令行参数打开、最近文件
- 编码自动检测：UTF-8 / UTF-8 BOM / UTF-16 / UTF-32 / **GB18030** / Latin-1，保存保持原编码
- 换行符 LF / CRLF / CR 自动检测并保持，保存时还原
- 文件外部修改监听：缓冲干净时自动重载，有未保存修改时询问
- 原子保存（QSaveFile），崩溃不损坏文件
- 深色 / 浅色主题，菜单 视图 → 切换深色/浅色主题
- 行号、当前行高亮、字号缩放（Ctrl+= / Ctrl+- / Ctrl+0）、自动换行开关
- 状态栏：行列号 / 换行符 / 编码 / 语言
- 设置 JSON 持久化（窗口状态、字号、主题、最近文件、工作区、视图模式）

## 构建（Windows）

依赖：Qt 6.10.3 (MinGW) 装于 `G:\Qt`，CMake + Ninja 来自 pip。

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

```bat
build\Release\EncodingTest.exe    :: 编码检测（15 项）
build\Release\TreeSitterTest.exe  :: 语法解析/高亮/识别（23 项）
build\Release\CommentTest.exe     :: 注释切换（8 项）
```

## 目录结构

```
src/
  main.cpp          入口
  app/              外壳（主窗口、标题栏、活动栏、快速打开、设置）
  core/             文档模型（IO、编码、换行、外部修改监听）
  editor/           编辑器组件（行号、高亮接入、查找条、注释命令）
  markdown/         Markdown 渲染（md4c→HTML）与实时预览装饰层
  panels/           面板（资源管理器、全局搜索、大纲、预览）
  settings/         设置（JSON 持久化）
  syntax/           语法引擎（tree-sitter 集成、语言注册表、代码转 HTML）
  themes/           主题（QSS、颜色 token、图标加载）
tests/              单元测试（编码/语法/注释）
packaging/          构建与打包脚本（含 Inno Setup）
third_party/        tree-sitter 运行时与 17 语言语法、md4c 源码
```
