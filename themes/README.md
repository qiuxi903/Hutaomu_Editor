# 主题格式与官方示范主题

本目录是**官方示范主题**的源码目录，同时也是主题格式的活文档。
主题能力分级、四层能力（T1–T4）与路线图见 [../PLAN_PLUGINS.md](../PLAN_PLUGINS.md) 第 4 章。

```
themes/
├── tools/make_themes.py        生成器：由语义基色推导完整色键集合 + 预览图
└── <theme-id>/
    ├── theme.json              主题定义（必需）
    ├── preview.png             预览图（设置页缩略图；可选但建议）
    └── LICENSE.txt             许可证
```

## 一、主题包 `.htmtpi`

`.htmtpi` 就是一个 **ZIP**，解包后是一个目录形态的主题：

```
my-theme.htmtpi  (ZIP)
└── my-theme/                  ← 可以是包根，也可以是唯一的一级子目录
    ├── theme.json
    ├── preview.png
    └── LICENSE.txt
```

安装：**设置 → 主题 → 导入主题包…**（`*.htmtpi` / `*.zip`）。
安装位置：`%APPDATA%/Hutaomu/Hutaomu Editor/themes/<id>/`（Linux/macOS 为 `QStandardPaths::AppDataLocation/themes`）。

安装时的安全校验（`src/themes/ThemePackage.cpp`）：

- 包内路径不得为绝对路径、不得含盘符、不得出现 `..` 越界；符号链接一律拒绝；
- 条目数与解压后总量有上限（`4000` 条 / `64MB`），防打包炸弹；
- `theme.json` 的 `id` 必须是安全目录名（字母/数字/`-`/`_`/`.`，且不含 `..`）；
- id 与**内置主题**同名时拒绝安装（内置主题不可被覆盖）；与已安装主题同名时提示覆盖。

## 二、theme.json（T1 配色 + T2 度量）

```json
{
  "schemaVersion": 1,
  "id": "my-theme",                  // 必填，安全目录名
  "name": "我的主题",                 // 设置页显示名
  "description": "一句话说明",         // 设置页提示（可空）
  "dark": false,                     // 决定"切换深/浅色"按钮往哪边切
  "capabilities": ["colors", "metrics"],
  "colors": { "accent": "#6c5ce7", "editorBg": "#ffffff", "...": "..." },
  "metrics": { "uiDensity": "spacious", "treeRowHeight": 28 }
}
```

### 2.1 `colors`（T1）

扁平键名，QSS 模板 `src/themes/template.qss` 里以 `@<key>` 引用。
完整键集合以 [paper.json](../src/themes/paper.json) 与 `themes/tools/make_themes.py`
的推导表为准（约 86 个键：窗口/标题栏/活动栏/侧栏/标签页/状态栏/滚动条/菜单/
按钮/提示 + `syntax.*` 语法高亮）。

> 颜色替换按**键名长度倒序**进行，因此 `accent` 与 `accentHover` 这类前缀关系是安全的。

### 2.2 `metrics`（T2）

值可以是数字或字符串；数字统一按 QSS 像素值渲染（模板里写作 `@m.<key>px`）。
**只写你想改的项**，其余用默认值（`comfortable` 密度）。

| 键 | 默认 | 作用 |
|---|---|---|
| `uiDensity` | `comfortable` | 预设内边距组合：`compact` / `comfortable` / `spacious`（显式项优先于预设） |
| `fontSizeSmall` / `fontSizeNormal` | 12 / 13 | 次级文字（标题栏、侧栏标题、提示） / 正文级 UI 文字 |
| `treeRowHeight` | 24 | 文件树/大纲/搜索结果行高 |
| `menuBarItemPaddingV/H` | 9 / 10 | 菜单栏项内边距 |
| `menuItemPaddingV` / `menuItemPaddingLeft` / `menuItemPaddingRight` | 5 / 24 / 28 | 下拉菜单项内边距 |
| `tabPaddingV/H` / `tabHeight` | 6 / 10 / 34 | 标签内边距与标签高度（新建按钮对齐用） |
| `inputPaddingV/H` | 5 / 8 | 搜索框内边距 |
| `compactInputPaddingV/H` | 3 / 6 | 查找条输入框内边距 |
| `sidebarButtonPadding` | 3 | 侧栏小按钮内边距 |
| `welcomeButtonPaddingV/H` | 7 / 24 | 欢迎页按钮内边距 |
| `buttonPaddingV/H` | 6 / 18 | 对话框按钮内边距 |
| `cornerRadius` / `cornerRadiusLarge` | 4 / 6 | 圆角（常规 / 查找条） |
| `scrollbarWidth` | 12 | 滚动条粗细 |
| `statusBarPaddingH` | 8 | 状态栏文字水平内边距 |
| `tooltipPadding` | 4 | 工具提示内边距 |
| `editorFontFamily` | 空 | **编辑器字体**：可给候选列表 `"Georgia, Noto Serif SC, serif"`，Qt 按序回退（`QFont::setFamilies`） |
| `editorFontSize` | 0 | **编辑器字号**（pt）；0 = 不覆盖用户设置 |

**关于编辑器字体/字号**：主题指定时**覆盖**用户在设置页的选择，设置页会明确提示
"当前主题指定了编辑器字体/字号，将覆盖上方设置"。这是 Typora 类主题达成观感的关键。

### 2.3 `background`（T3：编辑区背景图）

```json
"background": { "image": "assets/background.png", "mode": "tile", "opacity": 0.55 }
```

| 键 | 取值 | 说明 |
|---|---|---|
| `image` | 包内相对路径 | 必须是主题包内的相对路径（绝对路径 / 盘符 / `..` 会被**拒绝并忽略**） |
| `mode` | `tile`(默认) / `center` / `stretch` | 平铺（纹理）/ 居中原始尺寸 / 拉伸铺满 |
| `opacity` | 0..1（默认 1） | 图片叠在 `editorBg` 之上的不透明度；纹理建议 0.4–0.7 |

图片是**绘制在文字之下**的：实现上把图片按上述参数合成到视口大小的纹理，
作为编辑器 `palette Base` 画刷（Qt 用它填充视口）。

> 因此有一条实现约束：`template.qss` **不能**给 `QPlainTextEdit` 设置 `background`
> （QSS 会盖住 palette 画刷，背景图就看不见）。编辑器底色由 palette Base 提供，
> 观感与 QSS 方案一致。第三方主题无需关心，改 `background` 即可。

### 2.4 `layout.json`（T4：界面形态）

同一个 `theme.json`（配色/度量/资源）之外，主题可以用 **`layout.json`** 声明"换形态"。
文件存在即视为该主题会调整界面布局，应用时宿主会提示一次（可勾选"不再提示"，
也可选"只应用配色"而保持原界面）。

```json
{
  "activityBar":  { "visible": true },
  "statusBar":    { "visible": false },
  "sidebar":      { "visible": false, "style": "plain" },
  "outlinePanel": { "visible": false },
  "previewPane":  { "visible": true },
  "tabs":         { "style": "underline" },
  "editor":       { "centered": true, "maxWidth": 760, "headingUnderline": true },
  "immersive":    false
}
```

| 键 | 取值 | 说明 |
|---|---|---|
| `activityBar.visible` / `statusBar.visible` | true/false | 活动栏 / 状态栏显隐 |
| `sidebar.visible` | true/false | 侧栏（文件树 + 搜索）显隐 |
| `sidebar.style` | `plain`(默认) / `card` | `card` 时面板显示为卡片（圆角+边框，底色用 `editorBg`） |
| `outlinePanel.visible` / `previewPane.visible` | true/false | 大纲 / 预览面板显隐 |
| `tabs.style` | `tab`(默认) / `underline` / `pill` / `hidden` | 标签样式；`hidden` 隐藏标签栏（单栏沉浸） |
| `editor.centered` + `editor.maxWidth` | true/false + px | 编辑区内容居中限宽（写作栏宽） |
| `editor.headingUnderline` | true/false | h1/h2 加下划线（Typora 风标题装饰） |
| `immersive` | true/false | 专注写作预设：隐藏活动栏/状态栏/侧栏/大纲/预览 + 居中限宽（显式键可覆盖预设） |

**语义与边界**：

- **不写某项 = 不管某项**；没有 `layout.json` 的主题完全保持默认形态（VS Code 类主题就是这样）。
- 主题的形态是**临时覆盖**：切回无形态的主题（或选"只应用配色"）时，宿主会把你原来的
  面板/状态栏状态**原样还回来**，不会写坏你的界面设置。
- 键集合是宿主定义的有限开关——主题**不写代码**，不能发明新的界面结构。
- 编辑器**行高/段距**仍未支持：QPlainTextEdit 的文档布局忽略行高/段距格式；
  需要编辑器自绘排版才能实现（后续阶段）。
- `sidebar.defaultWidth` 之类的尺寸控制尚未实现（当前只做显隐与样式）。

### 2.5 `assets/`（T3：logo 与图标覆盖）

```
<theme>/
└── assets/
    ├── background.png            背景图（由 background.image 指向）
    └── icons/
        ├── brand.svg             品牌 logo（标题栏左上）
        ├── activity-explorer.svg 活动栏图标（按同名文件覆盖）
        ├── activity-search.svg
        └── tab-add.svg …
```

- **查找顺序**：`<主题根>/assets/icons/<name>.svg` → 内置 `:/icons/<name>.svg`；
  主题没提供的图标自动回退内置，不必整套复制。
- 覆盖文件里可以继续使用 `FILLCOLOR` / `PANELCOLOR` 占位符（随主题换色，
  内置图标就是这样写的），也可以写死颜色（品牌 logo 常有固有色）。
- 图标缓存键包含主题 id，切换主题不会串图；导入/覆盖安装主题后缓存也会清理。

**暂未生效的项**（随 P3"形态"阶段落地，先写不报错）：
`lineHeight`、`paragraphSpacing`、`contentMaxWidth` —— QPlainTextEdit 的文档布局
不支持行高/段距格式（`FixedHeight` 被忽略），需要编辑器自绘排版支持；

## 三、官方示范主题

| id | 名称 | 风格 | 资源（T3） | 形态（T4） |
|---|---|---|---|---|
| `typora-immersive` | 沉浸写作 Typora | 浅色、衬线字体、大字号行距、`spacious` 密度 | 书本 logo + 纸张颗粒背景 | 收起侧栏、下划线标签、居中 760px、标题下划线 |
| `obsidian-cards` | 卡片笔记 Obsidian | 深色、紫色强调、大圆角 | 多面体 logo + 点阵背景 | 卡片侧栏、胶囊标签、隐藏大纲 |
| `vscode-dark-plus` | VS Code Dark+ | 对齐 VS Code 默认深色，`compact` 密度 | 无（示范"资源可省"） | 无（保持 IDE 形态） |
| `vscode-light-plus` | VS Code Light+ | 对齐 VS Code 默认浅色，`compact` 密度 | 无 | 无 |

三套内置主题 `paper` / `greenwood-dark` / `graphite` 仍是扁平 JSON
（`src/themes/*.json`，无预览图，设置页按配色自动画缩略图）——两种形态都被支持。

## 四、生成 / 打包

```bash
# 重新生成示范主题（theme.json + preview.png）
python themes/tools/make_themes.py

# 打包成 .htmtpi（构建时自动执行，产物在 <build>/themes/）
cmake --build build/Release --target theme_packages
```

示范主题随**预装插件**（`hutaomu.official-themes`，见 [plugins/README.md](../plugins/README.md)）
分发，因此**无需安装**即可在设置页看到（资源里另有一份兜底副本）；
`.htmtpi` 产物用于分发、以及安装链路的回归测试（`tests/ThemePackageTest.cpp`）。

## 五、第三方主题作者的最小步骤

1. 复制任一示范主题目录，改 `id` / `name` / 配色；
2. 若从零开始，可只写想改的颜色键（缺的键会保留模板里的默认值——但**建议**在
   `paper.json` 的基础上补全，避免个别控件颜色突兀）；
3. 压成 ZIP（目录名 = `id`），扩展名改 `.htmtpi`；
4. 设置页「导入主题包…」，即时生效、无需重启。
