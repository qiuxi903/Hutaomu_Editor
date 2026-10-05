# Hutaomu Editor 插件系统设计（草案 v0.1）

> 目标：对标 VS Code 的插件体系，让第三方可以贡献**主题、命令、新格式查看器、
> 侧边栏页面、状态栏部件**等能力。
> 状态：**待评审**——文末「待确认问题」需要你拍板后才开始实现。

---

## 1. 设计目标与边界

### 目标

1. **主题**：`.htmtpi` 主题包——**不只是配色**：可改度量排版、logo/图标、
   背景图，乃至**布局形态**（隐藏活动栏、居中限宽、沉浸写作模式），
   让编辑器换个样子（VS Code 风 ↔ Typora 风 ↔ Obsidian 风）；安装即生效，无需重启。
2. **新格式支持**：插件可注册文件扩展名 → 查看器/编辑器（例如 `.psd`、`.epub`）。
3. **新功能/命令**：插件可注册命令，绑定到菜单、快捷键、命令面板（`Ctrl+Shift+P`）。
4. **新页面**：插件可注册侧边栏页面（复用现有活动栏按钮 + 卡片列模型）。
5. **可分发**：插件为单文件 `.htmed`，双击安装或从 UI 安装。

### 明确的边界（v1 不做）

- ❌ **不做插件市场**（v1 只支持本地文件安装 + 目录扫描 + 预装）。
- ❌ **不允许插件自带原生二进制**（`.dll`/`.so`）——跨平台与安全代价过高；
  但插件**可以带 JS 与网页 UI**（见下）。
- ⚠️ **插件的能力是分级开启的**：不写 JS 的插件**完全不启动 JS 运行时**。

---

## 2. 能力分级与「启动速度」的平衡（核心设计）

你的要求：**插件可以带 JS、可以套壳网页（像 VS Code）；但没有 JS 的插件
绝不能因此拖慢启动**。这通过**三级插件 + 惰性激活**实现。

### 2.1 三级插件能力

| 级别 | 插件内含 | 能力 | 启动开销 |
|---|---|---|---|
| **L1 声明式** | 仅 JSON/主题/图标 | 主题、格式映射、页面（内置类型）、命令（内置动作/shell）、状态栏 | **0**（不启动任何运行时，只读 JSON） |
| **L2 脚本式** | + `main.js` | 用 JS 写命令逻辑、转换数据、自定义渲染、响应事件 | JS 引擎**按需**启动（有 L2 插件**且被激活**时才启动） |
| **L3 网页式** | + `webview/` 网页 UI | 面板/编辑器标签页用 HTML+CSS+JS 绘制（套壳），经桥接调用宿主能力 | 系统 WebView **按需**创建（首次打开该视图时） |

**典型组合**：主题/格式类插件是 L1（零开销）；复杂 UI 插件（如"Markdown 所见即所得
增强""数据库浏览器"）用 L3；逻辑类插件用 L2。

### 2.2 惰性激活（保证冷启动 < 300ms）

关键规则：**插件只有被"激活事件"触发时才加载运行时**，与 VS Code 的
activation events 同思路。

```
启动流程：
  1. 读所有插件的 manifest.json（纯 JSON 解析，微秒级）        → 零运行时开销
  2. 注册 L1 贡献点（主题/格式/状态栏/内置页面）              → 立即可用
  3. L2/L3 插件只登记"激活条件"，不加载 main.js / 不建 WebView
  4. 触发激活条件时才按需启动：
       onCommand:<id>        执行该命令时
       onView:<id>           打开该视图时
       onLanguage:<lang>     打开该语言文件时
       onFilePattern:<glob>  打开匹配文件时（如 *.psd）
       onStartupFinished     启动完成、UI 空闲后（延迟执行）
  5. JS 引擎实例在"第一个 L2/L3 插件激活"时创建，之后所有插件共享
     （单实例多上下文，按插件隔离全局变量）
```

**验收标准**：全部安装 L1 插件时，冷启动时间与不装插件**无可测差异**；
装有 L2/L3 插件但未触发激活条件时，同样无可测差异。

### 2.3 技术选型（已定稿）

| 用途 | 方案 | 说明 |
|---|---|---|
| JS 引擎（L2） | **QuickJS**（~500KB，MIT） | 比 V8 小两个数量级、无 JIT 依赖、易嵌入、启动 <1ms。牺牲部分性能换取体积与启动速度（符合本项目取向） |
| WebView（L3） | **QtWebView（系统 WebView）** | **已定：走 QtWebView**，不自带内核 |

#### 为什么是 QtWebView（决策记录）

调研结论：Qt 6 官方安装器/仓库**不再提供 QtWebEngine 的二进制包**
（Qt 6.10.3 官方仓库仅有 `qtwebview`/`qtwebchannel`；WebEngine 自 Qt 6 起
只随商业版或需自行构建，且官方推荐 MSVC 工具链、MinGW 支持有限）。
QtWebEngine 的三条获取路线代价都不小：

| 路线 | 做法 | 代价 |
|---|---|---|
| A. 自建 QtWebEngine | 官方源码 + `gn`/`ninja` 构建 | 需 Python、Chromium 依赖、数小时编译、约 20-30GB 磁盘；三端各建一次 |
| B. 迁 MSVC + 官方预编译包 | 项目改用 MSVC 工具链 | 需迁移现有 MinGW 构建与 CI |
| C. **QtWebView（系统 WebView）— 选中** | 不自带内核 | 无构建成本、安装包轻、启动快 |

**决策**：**先走 C（QtWebView）**。理由：

1. **与项目取舍一致**——本项目处处优先"轻、快、三端一致可预期"，
   为 L3 插件引入 20-30GB 自建 Chromium 不划算；主体界面本就是纯 Qt Widgets，
   WebView 只是插件 UI 的可选后端，不是核心路径。
2. **代价可控**——依赖用户机器已有的 WebView2（Win）/ WebKitGTK（Linux）。
   若系统 WebView 缺失，则 L3 插件视图显示明确的降级提示，不影响主体功能。
3. **保留升级路径**——L3 后端通过 `IWebViewBackend` 抽象接口隔离，
   未来若确有需要可换 QtWebEngine 实现，**上层代码与插件桥接 API 不变**。

**已知风险与缓解**：
- 跨平台渲染差异（WebView2 是 Chromium、WebKitGTK 不是）→ 插件 UI 规范
  按"保守 HTML/CSS 子集"编写（文档给出允许特性清单）；
- 系统 WebView 不可用的机器 → 降级提示，不崩溃。

---

## 2.4 安全模型（分级信任）

| 级别 | 风险 | 对策 |
|---|---|---|
| L1 | 低（无代码） | 安装时展示贡献点清单；`shell` 动作首次执行需确认 |
| L2 | 中（JS 可读写文件/网络） | 能力**显式授权**（`permissions: ["fs.read", "fs.write", "net", "shell"]`）；未授权的 API 调用直接抛错；JS 上下文按插件隔离 |
| L3 | 中高（网页 + 桥接） | WebView **默认禁用远程 URL**（只允许插件包内资源 + 白名单域名，需 manifest 声明）；桥接 API 与 L2 同一套权限系统；**禁用远程脚本**（`<script src=http://...>`、内联 eval）；关闭持久化存储/Cookie；仅加载平台自带 WebView（WebView2 / WebKitGTK），不随包分发内核 |

**安装确认页**（对标 VS Code 权限提示）：展示插件 ID、发布者、版本、许可证、
贡献点清单、**申请的权限**（如"读取文件""执行外部程序""访问网络"），
用户确认后解包。权限变更（升级时新增权限）需重新确认。

---

## 3. 插件包格式 `.htmed`

**单文件 + ZIP 容器**（与 `.docx`/`.vsix` 同思路，本仓库已有 `ZipReader`）。
包内可以只有声明（L1），也可以带 JS（L2）和网页 UI（L3）。

```
my-plugin.htmed            (ZIP)
├── manifest.json          插件清单（必需）
├── README.md              说明（可选，插件详情页显示）
├── LICENSE                许可证（可选但推荐）
├── icon.svg               插件图标（可选）
├── main.js                ← L2：插件入口脚本（可选；有它即 L2）
├── webview/               ← L3：网页 UI 资源（可选；有它即 L3）
│   ├── panel.html
│   ├── panel.css
│   └── panel.js
├── themes/
│   └── ocean.json         主题定义（安装后进入主题列表）
├── formats/
│   └── psd.json           格式映射（扩展名 -> 查看器 / 自定义视图）
├── pages/
│   └── notes.json         侧边栏页面定义（内置类型 或 webview 类型）
├── views/
│   └── db.json            编辑器标签页视图（新增：可打开自定义编辑器/视图）
├── commands/
│   └── export-pdf.json    命令定义（内置动作 / shell / **js**）
└── status/
    └── word-count.json    状态栏部件定义
```

### manifest.json（含 L2/L3 的完整示例）

```json
{
  "schemaVersion": 1,
  "id": "com.example.markdown-tools",
  "name": "Markdown 工具集",
  "version": "1.2.0",
  "publisher": "Example",
  "description": "目录生成、导出、字数统计",
  "engines": { "hutaomu": ">=1.0.0" },
  "license": "MIT",

  "main": "main.js",

  "activationEvents": [
    "onCommand:markdownTools.exportPdf",
    "onView:markdownTools.toc",
    "onLanguage:markdown"
  ],

  "permissions": ["fs.read", "workspace", "shell"],

  "contributes": {
    "themes":    ["themes/ocean.json"],
    "formats":   ["formats/psd.json"],
    "pages":     ["pages/notes.json"],
    "views":     ["views/db.json"],
    "commands":  ["commands/export-pdf.json"],
    "statusBar": ["status/word-count.json"]
  }
}
```

**关键字段**：

| 字段 | 级别 | 含义 |
|---|---|---|
| （无 `main`/`webview`） | L1 | 纯声明，零运行时开销 |
| `main` | L2 | JS 入口，按 `activationEvents` 惰性加载 |
| `webview/` + `pages`/`views` 里 `type: "webview"` | L3 | 网页 UI，打开视图时才创建 WebView |
| `activationEvents` | L2/L3 | **惰性激活条件**，见 §2.2 |
| `permissions` | L2/L3 | 能力白名单，未申请的能力调用即报错 |

### 安装目录

```
Windows: %APPDATA%/Hutaomu/Hutaomu Editor/plugins/<plugin-id>/
macOS:   ~/Library/Application Support/Hutaomu/Hutaomu Editor/plugins/
Linux:   ~/.local/share/Hutaomu/Hutaomu Editor/plugins/
```

（与现有 `AppSettings` 的 `QStandardPaths::AppDataLocation` 一致。）

**预装插件**：安装包内 `plugins/` 目录的插件在首次启动时复制到用户目录
（可卸载/可升级），用于官方主题包、中文语言包等。

---

## 4. 主题系统 `.htmtpi`——「主题可以重塑编辑器外观」

### 4.0 设计理念

**主题不是换色，而是"换皮肤"，甚至"换形态"。** 参考对象：
- VS Code 主题：只能改颜色与少量尺寸（现状）；
- **Typora 主题**：能改字体、行距、段落间距、标题装饰、背景图、元素边距，
  同一份 Markdown 文档在不同主题下**看起来像另一个软件**；
- Obsidian 主题：还能改布局结构（侧栏位置/宽度、标签样式、卡片化）。

**Hutaomu 的目标：向 Typora/Obsidian 看齐。** 同一个编辑器，装上
「Typora 风格主题」后应呈现沉浸式单栏写作形态（无侧栏、居中栏宽、
大字号行距、标题下划线）；装上「VS Code 风格主题」则是 IDE 多面板形态。

### 4.1 主题的四层能力（逐层增强）

| 层 | 能改什么 | 技术手段 | 示例 |
|---|---|---|---|
| **T1 配色** | 颜色 token（现有） | `colors` JSON → QSS/调色板 | 深色/浅色、语法高亮配色 |
| **T2 度量与排版** | 字号、行距、内边距、圆角、边框、滚动条、菜单密度 | 扩展 token + QSS 变量 | Typora 式大行距、紧凑 IDE 密度 |
| **T3 外观资源** | logo/图标、背景图片、启动图、空状态插画 | 资源覆盖（`assets/`）+ 自定义绘制读取 | 换品牌 logo、纸张纹理背景 |
| **T4 结构与交互** | 侧栏/活动栏/状态栏的**显隐与位置**、标签样式、沉浸模式、编辑区最大宽度、标题装饰 | **布局清单（layout）** + 结构 token | Typora 单栏沉浸、Obsidian 卡片侧栏 |

T1/T2 纯数据；T3 是资源替换；**T4 是"换形态"的关键**——需要宿主暴露
一组可配置的布局开关（而不是让主题写代码）。

### 4.2 主题包结构

```
typora-github.htmtpi        (ZIP)
├── theme.json              主题定义（必需）
├── layout.json             布局/形态定义（可选 → 有它即 T4 主题）
├── qss/
│   └── overrides.qss       自定义 QSS 片段（可选，追加在内置模板之后）
├── assets/                 T3 资源（可选）
│   ├── logo.svg            品牌 logo（替换标题栏/关于页）
│   ├── activity-icons/     活动栏图标覆盖（同名替换）
│   ├── background.png      编辑区/窗口背景图
│   └── empty-state.svg     空状态插画
├── preview.png             设置页预览图（可选）
└── LICENSE
```

### 4.3 `theme.json`（T1 + T2）

```json
{
  "schemaVersion": 1,
  "id": "typora-github",
  "name": "Typora GitHub",
  "description": "沉浸式单栏写作形态，仿 Typora",
  "dark": false,
  "base": "paper",
  "colors": {
    "accent": "#0969da",
    "editorBg": "#ffffff",
    "editorFg": "#1f2328",
    "syntax.keyword": "#cf222e",
    "syntax.string": "#0a3069"
  },
  "metrics": {
    "editorFontFamily": "Georgia, Noto Serif SC, serif",
    "editorFontSize": 17,
    "uiDensity": "spacious",
    "fontSizeSmall": 13,
    "fontSizeNormal": 15,
    "cornerRadius": 6,
    "scrollbarWidth": 10,
    "tabHeight": 36,
    "treeRowHeight": 28
  }
}
```

**`metrics` token 清单与落地状态**（完整表见 [themes/README.md](themes/README.md)）：

| 分组 | token | 状态 |
|---|---|---|
| 排版 | `editorFontFamily` / `editorFontSize`（**覆盖**用户设置，设置页提示） | **P1 已实现** |
| 密度 | `uiDensity`（`compact`/`comfortable`/`spacious` 预设）+ 各内边距/行高/圆角/滚动条/标签高度 | **P1 已实现** |
| 编辑器排版 | `lineHeight` / `paragraphSpacing` / `contentMaxWidth` | **P3**：QPlainTextEdit 文档布局不支持行高/段距格式（`FixedHeight` 被忽略），需编辑器自绘排版；`contentMaxWidth` 归入 `layout.json` 的 `editor.maxWidth` |

### 4.4 `layout.json`（T4，换形态的核心）

宿主预先实现一组**布局开关**（主题只声明，不写代码）：

```json
{
  "sidebar":   { "visible": true,  "defaultWidth": 260, "style": "card" },
  "activityBar": { "visible": false },
  "statusBar": { "visible": false },
  "tabs":      { "style": "underline", "showCloseButton": "hover" },
  "editor": {
    "maxWidth": 760,
    "centered": true,
    "backgroundImage": "assets/background.png",
    "backgroundOpacity": 0.35,
    "headingUnderline": true,
    "blockSpacing": 14
  },
  "outlinePanel": { "visible": false },
  "previewPane":  { "visible": false },
  "immersive": true
}
```

**可配置项（宿主实现，主题声明）**：

| 区域 | 可选值 |
|---|---|
| `sidebar.visible` / `activityBar.visible` / `statusBar.visible` / `outlinePanel.visible` / `previewPane.visible` | `true`/`false`（配合"主题可隐藏 UI 部件"） |
| `sidebar.style` | `plain` / `card`（卡片化侧栏，Obsidian 风） |
| `tabs.style` | `tab`（VS Code）/ `underline`（Typora/Chrome 风）/ `pill` / `hidden`（单栏沉浸） |
| `editor.centered` + `editor.maxWidth` | 居中限宽（写作模式） |
| `editor.backgroundImage` + `backgroundOpacity` | 背景图（纸张纹理等） |
| `editor.headingUnderline` / `blockSpacing` | 标题装饰、段落间距（Typora 特征） |
| `immersive` | 一键进入"专注写作"（隐藏所有侧栏/状态栏，居中大行距） |

**约束**：布局开关是**宿主定义的有限集合**——主题不能发明新的 UI 结构
（避免主题变成代码）。若某主题需要宿主持不支持的结构，需向宿主提需求扩展开关集。
这样既实现"换个样子"，又保证三端行为一致、不引入执行风险。

### 4.5 `qss/overrides.qss`（T2/T3 的高级逃生舱，**v1 推迟，见 §9 #13**）

在宿主渲染的内置 QSS **之后**追加，可覆盖任意控件样式：

```css
/* Typora 风的标题装饰 */
#editorTabs::tab { padding: 6px 18px; }
#outlineTree { background: transparent; }
#statusBar { font-size: 13px; }
```

**为什么推迟**：QSS 选择器与宿主控件 objectName 强耦合，宿主改一次控件命名
就可能让主题失效（且用户看到的是"主题坏了"）。v1 先只提供受限声明
（`metrics` + `layout`），待控件命名稳定、且积累了真实主题需求后再放开。

**限制（未来启用时生效）**：
- 只允许**样式**（选择器 + 属性），不允许 URL 指向包外资源；
- 资源引用走自定义 scheme（`htheme://typora-github/assets/...`），路径校验防目录穿越；
- 不支持的属性静默忽略（不报错、不影响可用性）。

### 4.6 资源覆盖规则（T3）

| 资源 | 覆盖方式 |
|---|---|
| 品牌 logo | `assets/logo.svg` → 标题栏、"关于"对话框 |
| 活动栏图标 | `assets/activity-icons/*.svg` 按**同名替换**（如 `activity-explorer.svg`） |
| 背景图 | `layout.json` 的 `editor.backgroundImage` |
| 空状态插画 | `assets/empty-state.svg` |
| 窗口图标 | `assets/app-icon.svg`（可选，v1 可只影响标题栏） |

实现：`IconLoader::load()` 增加"**先查当前主题的资源覆盖，再回退内置资源**"。

### 4.7 落地改动清单

| 模块 | 改动 | 状态 |
|---|---|---|
| `ThemeDefinition` | 增加 `metrics`、`description`、`builtin`、`rootPath`、`previewPath`、`capabilities`；`background`（T3，见下） | **P1 完成**（`layout` 字段随 P3） |
| `ThemeBackground`（新） | `image` / `mode`(tile·center·stretch) / `opacity`；路径校验拒绝越界 | **P2 完成** |
| `ThemeManager::assetPath()` | 包内相对路径 → 实际路径（内置资源/安装目录统一），含越界校验 | **P2 完成** |
| `ThemePackage`（新） | `.htmtpi`（ZIP）解包校验安装 + 目录形态直装；路径穿越/符号链接/打包炸弹防护 | **P1 完成** |
| `ThemeManager` | 多来源合并（内置资源 → 用户主题目录）；`metrics` 合并（默认值 → 密度预设 → 主题覆盖）；颜色/度量替换改为**长键优先**（修掉 `@accent` 误伤 `@accentHover` 的偶发 bug） | **P1 完成** |
| `MainWindow` | `editorFont()` 支持主题覆盖字体/字号（含 `setFamilies` 候选回退）；主题切换后重刷字体；深/浅判断改用 `dark` 字段（第三方深色主题 id 未必以 dark 结尾） | **P1 完成** |
| `MainWindow` | `applyThemeLayout()`：按主题声明显隐/样式化各区域 + **形态快照**（撤销主题时原样还回用户界面状态）+ 启动时应用 + 新编辑器继承 | **P3 完成** |
| QSS 模板 | `@m.<metric>` 度量变量（含默认值）+ 颜色变量；设置页主题画廊样式 | **P1 完成**（overrides 追加随 QSS 逃生舱一并推迟） |
| `IconLoader` | 主题资源覆盖：`<主题根>/assets/icons/<name>.svg` 优先，缺失回退内置；缓存键含主题 id | **P2 完成** |
| `IconLoader` | `htheme://` 自定义 scheme（供 QSS url() 引用包内资源） | 随 QSS 逃生舱推迟（§9 #13） |
| `CodeEditor` | 背景图绘制：合成视口纹理 + `palette Base` 画刷（含 viewport 子控件调色板）；行高/段距/居中限宽随 P3 | **P2 完成（背景图部分）** |
| 设置页 | 主题画廊（预览图 + 名称 + 描述/能力提示）、「导入主题包…」「删除主题」、字体覆盖提示 | **P1 完成** |
| 设置页 | 应用含布局变更的主题时提示，带"以后不再提示"与"只应用配色" | **P3 完成**（`MainWindow::maybeConfirmThemeLayout`） |

### 4.8 主题能力声明与用户预期管理

主题包在 manifest/theme.json 里声明能力等级，安装与切换时明确告知用户：

```json
{ "capabilities": ["colors", "metrics", "assets", "layout"] }
```

- 切换**含 `layout` 的主题**时，弹一次提示：「该主题会调整界面布局
  （隐藏活动栏、启用沉浸写作模式），是否应用？」——因为这会改变用户熟悉的操作位置；
- 用户可在设置页**只应用该主题的配色部分**（T1/T2），忽略布局部分（T4），
  即"同一主题，可选是否接受形态变化"。

---

## 5. 各扩展点的实现方式

### 5.1 主题（`contributes.themes`）

- 安装时把 `theme.json` 复制到主题目录；
- `ThemeManager::availableThemes()` 增加目录扫描（见上）；
- 设置页主题下拉自动出现新主题；
- **零代码**，纯数据。

### 5.2 新格式（`contributes.formats`）

`formats/psd.json`：

```json
{
  "viewer": "image",          // 复用内置查看器类型
  "extensions": ["psd", "xcf"],
  "name": "Photoshop 文档"
}
```

可用的内置 `viewer` 类型（v1 直接复用现有查看器）：
`text`（文本编辑器）、`image`、`media`、`pdf`、`docx`、`xlsx`、`pptx`、
`hex`（十六进制，需新增）、`archive`（压缩包列表，需新增）。

`viewerKindForFile()` 改为**先查插件注册表，再回退内置表**。

指向上面的**内置**查看器是 L1（零代码）；若要**自定义渲染**，两种方式：
- L3：`"viewer": "webview"` + `webview/viewer.html`，用网页渲染该格式
  （例如 `.epub` 用 JS 解析后排版显示）；
- L2：`"viewer": "js"`，在 `main.js` 里注册 `registerViewer(ext, {render})`，
  返回 HTML 字符串或直接绘制。

`viewerKindForFile()` 的查找顺序：**插件注册表 → 内置表**。

### 5.3 新页面（`contributes.pages`）

`pages/notes.json`：

```json
{
  "id": "notes",
  "title": "笔记",
  "icon": "icon.svg",
  "type": "webview",          // 内置页面类型
  "defaultSide": "right"
}
```

内置页面类型（v1）：
- `webview`：渲染插件包内的 HTML/图片/PDF（**不执行脚本**，静态内容）；
- `fileList`：显示某个目录的文件列表（如“下载目录”“项目 TODO”）；
- `outline` / `search` 等**别名**（把内置页面暴露给插件重命名/换位）。

实现：复用现有 `registerSidePanel()` + `ActivityBar::addPanelButton()`，
活动栏图标从插件包内 `icon.svg` 经 `IconLoader` 加载。

### 5.4 命令（`contributes.commands`）

`commands/export-pdf.json`：

```json
{
  "id": "export.pdf",
  "title": "导出为 PDF",
  "category": "Markdown",
  "keybinding": "Ctrl+Alt+P",
  "action": {
    "type": "shell",                 // 调用外部程序
    "command": "pandoc",
    "args": ["${file}", "-o", "${fileDir}/${fileBaseName}.pdf"]
  }
}
```

内置 `action.type`（v1）：
- `shell`：调用外部命令（**需用户首次确认**，见 §6）；
- `openExternal`：用系统默认程序打开 URL/文件；
- `builtin`：调用内置动作（如 `editor.save`、`markdown.preview`）。

命令自动出现在「命令面板」（需新增，`Ctrl+Shift+P`，一个带模糊搜索的弹窗）。

### 5.5 状态栏部件（`contributes.statusBar`）

`status/word-count.json`：

```json
{ "id": "wordCount", "type": "wordCount", "alignment": "right",
  "visibleFor": ["markdown", "text"] }
```

内置类型（v1）：`wordCount`、`charCount`、`lineCount`、`indent`、`eol`、`encoding`。

---

### 5.6 L2：JS 插件 API（宿主桥）

`main.js` 以 CommonJS 风格导出激活函数（对标 VS Code 的 `activate`）：

```js
// main.js
exports.activate = function (ctx) {
  ctx.commands.register('markdownTools.exportPdf', async () => {
    const text = ctx.editor.getText();
    const out = ctx.path.join(ctx.editor.dir, ctx.editor.baseName + '.pdf');
    await ctx.shell.run('pandoc', [ctx.editor.path, '-o', out]);
    ctx.ui.showMessage('已导出：' + out);
  });

  ctx.statusBar.register('markdownTools.words', () => ({
    text: '字数 ' + ctx.editor.getText().trim().split(/\s+/).length
  }));
};

exports.deactivate = function () { /* 清理 */ };
```

**宿主 API（`ctx`，按 `permissions` 授权后可用）**：

| 命名空间 | 方法（示例） | 需要的权限 |
|---|---|---|
| `ctx.editor` | `getText()` `setText()` `getSelection()` `replaceSelection()` `getPath()/dir/baseName` `onDidChange(fn)` `open(path)` | `editor`（默认授予） |
| `ctx.workspace` | `rootPath` `findFiles(glob)` `readFile(p)` `writeFile(p, s)` `onDidSave(fn)` | `workspace` / `fs.read` / `fs.write` |
| `ctx.commands` | `register(id, fn)` `execute(id)` `getAll()` | — |
| `ctx.ui` | `showMessage(s)` `showInputBox()` `showQuickPick(items)` `registerView(id, factory)` `openView(id)` | — |
| `ctx.statusBar` | `register(id, provider)` `update(id, item)` | — |
| `ctx.decorations` | `setRangeStyle(from, to, style)` `clear()` | `editor` |
| `ctx.shell` | `run(cmd, args, opts)` | **`shell`**（首次确认） |
| `ctx.net` | `fetch(url, opts)` | **`net`**（默认拒绝） |
| `ctx.theme` | `onDidChange(fn)` `current()` | — |
| `ctx.storage` | `get(k)` `set(k, v)`（插件私有 KV 持久化） | — |

**实现要点**：
- QuickJS 引擎**单实例、多上下文**：每个插件一个 `JSContext`，全局变量隔离；
- 宿主对象以 C 函数绑定（`JS_NewCFunction`），**逐个 API 做权限检查**；
- 所有跨边界调用包 try/catch：JS 异常转为宿主日志，**不允许崩溃波及编辑器**；
- 长任务用 `setTimeout` 等价的原生定时器驱动（QuickJS 需宿主提供事件循环泵）；
- 崩溃/死循环防护：单次调用 CPU 时间上限（可配置，默认 5s），超时中断该上下文。

### 5.7 L3：网页 UI（套壳）

两种形态：

1. **侧边栏页面**（`pages/*.json` 里 `"type": "webview"`）
2. **编辑器标签页视图**（`views/*.json`，可打开为独立标签，如"数据库浏览器"）

```json
// pages/toc.json
{
  "id": "markdownTools.toc",
  "title": "文档目录",
  "type": "webview",
  "entry": "webview/panel.html",
  "icon": "icon.svg",
  "defaultSide": "right"
}
```

网页内通过 **QtWebChannel** 拿到的桥对象与 L2 的 `ctx` **同构**（同一套权限检查）：

```html
<!-- webview/panel.html -->
<script src="qwebchannel.js"></script>
<script>
new QWebChannel(qt.webChannelTransport, function (channel) {
  const h = channel.objects.host;          // 宿主桥
  h.getEditorText(function (text) {        // 回调风格（QtWebChannel 语义）
    document.getElementById('out').textContent = text;
  });
  h.onEditorChanged.connect(function (text) { /* 实时响应编辑 */ });
});
</script>
```

**资源与网络限制**（安全）：
- 只允许加载**插件包内**资源（`qrc`/自定义 scheme + 路径前缀校验）；
- 远程 URL **默认禁止**；manifest 声明 `webview.allowedHosts` 白名单后放行；
- 禁用 `eval`、禁止 `file://` 任意路径（只暴露包内目录）。

---

## 6. 架构落地（代码层面）

新增模块 `src/plugins/`：

| 文件 | 职责 |
|---|---|
| `PluginManifest.h/.cpp` | manifest 解析 + schema 校验 + 版本/权限检查 |
| `PluginManager.h/.cpp` | 扫描目录、安装/卸载 `.htmed`、启用/禁用、**激活事件派发** |
| `PluginRegistry.h/.cpp` | 已注册扩展点查找表（主题/格式/页面/视图/命令/状态栏） |
| `PluginInstaller.h/.cpp` | ZIP 解包 + 路径安全校验 + `.htmtpi` 安装 + 权限确认页 |
| `CommandPalette.h/.cpp` | `Ctrl+Shift+P` 命令面板（新增 UI） |
| `CommandRegistry.h/.cpp` | 命令注册与执行（内置/shell/js 三类动作，含确认流程） |
| `js/QuickJsEngine.h/.cpp` | QuickJS 封装：单实例多上下文、宿主对象绑定、超时中断 |
| `js/HostApi*.cpp` | 按命名空间实现 `ctx.*` API（editor/workspace/ui/shell/net/...） |
| `webview/IWebViewBackend.h` | WebView 后端抽象接口（v1 实现 = QtWebView；未来可加 QtWebEngine 实现） |
| `webview/WebViewHost.h/.cpp` | WebView 容器 + QWebChannel 桥（与 host API 共用权限层） |
| `themes/ThemePackage.h/.cpp` | `.htmtpi` 解包/校验/安装、`metrics`+`layout`+`assets`+`qss` 读取 |
| `themes/ThemeLayoutEngine.h/.cpp` | 把主题 `layout.json` 应用到宿主（显隐区域/标签样式/居中限宽/背景图/沉浸模式） |
| `webview/PluginResourceScheme.cpp` | 自定义 URL scheme：只暴露插件包内资源 |

需要改动的既有模块：

- `ThemeManager`：主题来源从单一路径改为多来源合并（内置资源 → 主题目录 → 插件），
  并支持 T1–T4 四层能力（配色/度量/资源/布局）；
- `MainWindow`：新增 `applyThemeLayout(const ThemeLayoutSpec&)` —— 按主题换形态
  （隐藏活动栏/状态栏、标签下划线风、编辑区居中限宽、背景图、沉浸写作模式）；
- `ViewerFactory::viewerKindForFile`：先查插件格式表，再回退内置；
- `MainWindow`：启动时加载插件（仅 manifest）、注册 L1 贡献点、派发激活事件；
- `CodeEditor`：支持主题的段落间距 / 行高 / 内容居中限宽 / 背景图绘制；
- `ActivityBar`：支持从插件包加载 SVG 图标；
- `SettingsDialog`：新增「插件」页（安装/启用/禁用/卸载/权限查看）与「命令面板」入口。

**加载时机**（严格保证启动速度）：

```
main.cpp
  ├─ AppSettings::load()
  ├─ PluginManager::loadManifests()      ← 只读 JSON，不启动任何运行时
  ├─ PluginManager::registerDeclarative() ← L1 贡献点立即生效（主题等）
  ├─ ThemeManager::applyTheme(...)        ← 主题必须在窗口创建前应用
  ├─ MainWindow window
  │    └─ 触发 onStartupFinished 类激活事件（延迟到 UI 空闲）
  └─ app.exec()  → 首个 L2/L3 激活时才创建 QuickJS / WebView
```

---

## 7. 版本与兼容

- `manifest.schemaVersion`：清单结构版本，不兼容时拒绝安装并提示；
- `manifest.engines.hutaomu`：要求的宿主版本范围（语义化版本）；
- 插件自身 `version`：用于升级判断（同 id 覆盖安装）。

---

## 8. 分阶段实施建议（已按确认结果调整）

| 阶段 | 内容 | 交付 |
|---|---|---|
| **P1 ✅ 已完成** | **`.htmtpi` 主题包 + T1/T2 能力**（配色 + 度量排版）+ 设置页主题画廊（预览图/导入/删除）+ **4 套示范主题**；另修掉颜色替换长键 bug | 主题可换色、可改 UI 度量与编辑器字体字号；第三方主题可导入、可删除 |
| **P2 ✅ 已完成** | **T3 资源覆盖**：`assets/icons/*` 图标与 logo 覆盖、`background` 编辑区背景图（tile/center/stretch + 不透明度）+ 设置页主题**实时预览**（选中即应用、取消回滚） | 主题可换品牌、图标与背景；换主题即时可见 |
| **P3 ✅ 已完成** | **T4 布局能力**：`layout.json` 开关集（活动栏/状态栏/侧栏/大纲/预览显隐、标签 tab·underline·pill·hidden、侧栏 plain·card、编辑区居中限宽、标题下划线、沉浸预设）+ 形态快照回滚 + 应用前提示（"不再提示"/"只应用配色"） | **主题可"换形态"**：Typora 沉浸写作 / Obsidian 卡片侧栏 / 隐藏活动栏；示范主题已带形态 |
| **P4 ✅ 已完成（核心）** | `.htmed` 框架：manifest 校验、安装/覆盖/启停/卸载、**权限确认页**、`state.json` 状态；L1 贡献点已落地 **主题 / 文件格式 / 命令（内置动作 + shell 外部命令 + 首次信任）**；插件菜单、示例插件、`--list-plugins` 排查命令 | 声明式插件可用、可分发（`参考 plugins/README.md`） |
| **P4b ✅ 已完成** | L1 其余贡献点：`statusBar`（内置类型部件）与 `pages`（侧栏页面：markdown/text/fileList，静态渲染不执行脚本，带活动栏图标） | 插件可贡献状态栏与侧栏页面；示例插件已含四类贡献点 |
| **P5 ✅ 已完成（命令面板）** | 命令面板：`Ctrl+Shift+P` 模糊搜索（内置动作 + 插件命令 + 主题切换），中文标题与英文 id 双可搜 | 键盘可触达一切功能；插件命令有了统一入口 |
| **P5b ✅ 已完成** | 预装插件：`<程序目录>/plugins` + `:/plugins` + `$HUTAOMU_PLUGIN_DIRS` 三来源发现；同 id 用户副本覆盖预装；可禁用不可卸载；**官方主题包**（4 套主题 + 2 命令）与**配置文件格式包**（13 个扩展名 + 状态栏段）随包分发；官方主题由资源改为插件数据 | 官方扩展开箱即用，且可禁用/可更新 |
| **P6** | **QuickJS + L2 JS API**（`activationEvents` 惰性激活、`permissions` 校验、超时中断） | 插件可写逻辑 |
| **P7** | **L3 网页 UI**：基于 QtWebView（系统 WebView）实现，后端抽象隔离（见 §2.3） | 插件可做复杂 UI |
| **P8** | 插件详情页、开发者模式（目录加载 + 热重载） | 面向第三方作者 |
| **P9（后续）** | 插件市场（服务器 + 签名 + 审核） | 需要单独立项 |

**顺序理由**：你提出的"主题可以换形态"是**优先级最高的形态差异需求**，
因此把主题从原 P1 拆成 P1–P3 三个子阶段提前做（T1/T2 → T3 → T4），
让编辑器尽早具备"换个样子"的能力；插件框架（P4 起）随后跟进，
运行时（QuickJS/WebView)放在最后，避免早期被复杂度拖住。

**并行建议**：P1–P3（主题）与 P4（声明式框架）可并行开发，互不阻塞。

> P1 起即交付**官方示范主题**（放进仓库 `themes/` 源目录，构建产出 `.htmtpi`），
> 既作为主题格式的活文档，也验证 T1/T2 能力边界。

## 9. 决策记录（已确认）

| # | 决策 | 结论 |
|---|---|---|
| 1 | JS 引擎 | **QuickJS**（~500KB，启动 <1ms） |
| 2 | WebView 路线 | **QtWebView（系统 WebView）**；后端抽象隔离，未来可换（见 §2.3 决策记录） |
| 3 | 网络访问 | **默认拒绝**；首次调用弹窗询问（仅本次 / 始终允许 / 拒绝） |
| 4 | 插件分发 | **v1 仅本地安装 + 预装**；插件市场后续阶段（P9） |
| 5 | `shell` 外部命令 | 允许；首次执行弹窗展示完整命令行 + "信任此插件" |
| 6 | 开发者模式 | **要**（默认关闭，设置页可开；目录加载 + 热重载） |
| 7 | 插件 ID | 强制 `publisher.name` 前缀 |
| 8 | 预装插件 | 支持（官方主题、语言包） |
| 9 | 主题能力 | **不止配色**：可改布局/logo/背景图，实现"换形态"（Typora 风）；见 §4 |
| 10 | 主题与用户预期 | 切换含布局变更的主题时弹窗提示（首页提示，**可勾选"不再提示"**）；可只应用配色忽略布局 |
| 11 | 示范主题 | **做**（P1 起）：Typora 风、Obsidian 风、VS Code Dark+ / Light+ 精修版 |
| 12 | 主题字号覆盖 | **覆盖用户设置 + 设置页提示**（"当前主题指定了编辑器字号/字体"） |
| 13 | `qss/overrides.qss` 逃生舱 | **推迟**：v1 只用受限声明（`metrics`/`layout`），QSS 覆盖待控件命名稳定后再评估（目标 ≥P8），避免主题与宿主版本强耦合 |

### 全部决策已确认（2026-09-28）

四项小问题的结论：① 布局变更提示可勾选"不再提示"；② 做示范主题；
③ 主题字号**覆盖**用户设置 + 设置页提示；④ QSS 逃生舱推迟（见上表 #13）。

### P1 交付记录

| 交付 | 位置 |
|---|---|
| 主题定义/发现/度量合并/样式渲染 | `src/themes/ThemeManager.h/.cpp` |
| `.htmtpi` 安装（ZIP 解包 + 目录形态，安全校验） | `src/themes/ThemePackage.h/.cpp`（复用项目内 `viewers::ZipReader`，不依赖 Qt 私有模块） |
| QSS 度量变量 | `src/themes/template.qss`（`@m.<key>px`） |
| 设置页主题画廊 | `src/app/SettingsDialog.h/.cpp`（预览图/导入/删除/字体覆盖提示） |
| 主题字体覆盖 | `src/app/MainWindow.cpp`（`editorFont()`） |
| 4 套示范主题 + 生成器 | `themes/<id>/` + `themes/tools/make_themes.py` |
| 主题作者文档 | [themes/README.md](themes/README.md) |
| 回归测试（13 项全绿） | `tests/ThemePackageTest.cpp` |

**P1 已知边界**（如实记录）：
- 编辑器**行高/段距**未生效（QPlainTextEdit 文档布局限制）→ P3 自绘排版；
- 主题尚未支持资源覆盖（logo/背景图）与布局形态 → P2/P3；
- 设置页主题画廊当前为图标列表；主题**实时预览**（不点确定就看效果）未做，可作为 P2 增强。

### P2 交付记录

| 交付 | 位置 |
|---|---|
| 资源路径解析（内置/安装统一，越界拒绝） | `ThemeManager::assetPath()` / `currentTheme()` |
| 背景图规格解析（`background`） | `ThemeManager.cpp`（`mode`/`opacity`，非法路径忽略） |
| 图标与 logo 覆盖 | `IconLoader.cpp`（覆盖优先 + 主题感知缓存 + `clearCache`） |
| 背景图绘制 | `CodeEditor::rebuildEditorBackground()`（tile/center/stretch，随视口缩放重铺） |
| QSS 调整 | `template.qss`：编辑器不设 `background`，底色交由 `palette Base`（否则背景图被盖住） |
| 实时预览 | `SettingsDialog::themePreviewRequested` → `MainWindow::previewTheme()`（不落盘；取消回滚） |
| 示范资源 | `themes/<id>/assets/**`（书本/多面体 logo、纸张颗粒、点阵），由生成器产出 |
| 回归测试 | `tests/ThemeAssetTest.cpp`（24 项）；`ThemePackageTest` 扩到 52 项（含安装包资源） |

**P2 实测依据**（探针结论，写在代码注释里）：
- `QPlainTextEdit` 用 `palette Base` 画刷填充视口 → 纹理可透出（`QSS background` 存在时不可见）；
- `viewport()` 是独立子控件，只对编辑器 `setPalette` 不够，必须同时设置 viewport。

**P2 已知边界**：
- 背景图只作用于编辑区（面板/侧栏不参与），符合"纸张纹理"这类用法；
- `assets/` 目前支持图标覆盖与背景图；空状态插画（`assets/empty-state.svg`）等尚无对应 UI，留待有该界面时接入。

### P3 交付记录

| 交付 | 位置 |
|---|---|
| `ThemeLayout` + `layout.json` 解析 + `summary()` | `ThemeManager.h/.cpp`（密钥白名单，非法值忽略；immersive 为预设，显式键覆盖） |
| 形态应用与回滚 | `MainWindow::applyThemeLayout()` / `captureLayoutSnapshot()` / `restoreLayoutSnapshot()` |
| 应用前提示（不再提示 / 只应用配色） | `MainWindow::maybeConfirmThemeLayout()` + `AppSettings`（`themeLayoutApproved` / `themeLayoutDeclined` / `themeLayoutPromptSuppressed`） |
| 编辑区居中限宽、标题下划线 | `CodeEditor::setContentMaxWidth()` / `setHeadingUnderline()`（行号跟随列起点） |
| 标签形态、卡片侧栏样式 | `template.qss`（`#editorTabs[tabStyle=...]`、`#Sidebar[cardStyle=card]` 等） |
| 示范形态 | `themes/typora-immersive/layout.json`（写作形态）、`themes/obsidian-cards/layout.json`（卡片形态） |
| 回归测试 | `tests/ThemeLayoutTest.cpp`（40 项：解析/生效/还原/declined/持久化），全套 15 个测试通过 |

**P3 已知边界**（如实记录）：
- 编辑器**行高/段距**仍不支持：`QPlainTextEdit` 的文档布局忽略行高/段距格式，需自绘排版；
- 布局开关集中**没有尺寸类键**（如 `sidebar.defaultWidth`）——当前只做显隐与样式；
- 主题形态对**新开的编辑器**通过 `applyEditorFont()` 继承；查看器标签页（PDF/图片等）不参与编辑区形态。
