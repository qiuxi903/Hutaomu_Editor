# 插件（`.htmed`）—— 格式、能力与安装

本目录是**官方示例插件的源码目录**，同时也是插件格式的活文档。
插件体系的设计背景、分级（L1 声明式 / L2 脚本 / L3 网页 UI）与路线图见
[../PLAN_PLUGINS.md](../PLAN_PLUGINS.md)。

```
plugins/
├── tools/make_demo_plugin.py     生成器（示例插件 + 自带主题）
└── hutaomu.demo-toolkit/         示例插件（目录名 = 插件 id）
    ├── plugin.json               插件清单（必需）
    ├── README.md
    └── themes/sunrise/           自带主题（theme.json + preview.png）
```

## 一、v1 的插件是"声明式"的

**不含可执行代码**。插件能做的三件事：

| 贡献点 | 作用 | 生效位置 |
|---|---|---|
| `themes` | 带来整套主题（可含 T1–T4 全部能力） | 设置 → 主题（标记来源插件） |
| `formats` | 认领文件扩展名 → 已注册语言（如 `*.log`） | 打开该后缀文件时的语言识别与状态栏 |
| `commands` | 命令：**内置动作**（调用编辑器功能）或 **外部命令**（需 `shell` 权限） | 菜单栏「插件」菜单 + 快捷键 + **命令面板**（`Ctrl+Shift+P`，中英文皆可搜） |
| `statusBar` | 状态栏部件（只能用宿主内置类型） | 状态栏（左/右） |
| `pages` | 侧栏页面（渲染插件包内的静态文件） | 侧栏卡片面板 + 活动栏按钮 |

- **内置动作**零风险：只是调用编辑器既有功能（保存/查找/切换主题…）。
- **外部命令**需要 `permissions: ["shell"]`，且**首次执行会弹窗**展示完整命令行，
  由用户勾选"信任此插件"后才执行（之后不再逐次询问）。
- `fs.read` / `fs.write` / `net` 会被记录并在安装页展示，但**v1 不生效**
  （属于脚本插件阶段的能力）。

## 二、plugin.json

```json
{
  "id": "hutaomu.demo-toolkit",       // 必填；必须是 publisher.name，且目录名 = id
  "name": "示例插件：日志与写作工具",   // 必填
  "version": "1.0.0",                 // 必填，语义化版本
  "publisher": "hutaomu",             // 必填，且必须是 id 的前缀（防仿冒）
  "description": "一句话说明",
  "license": "LicenseRef-Proprietary",
  "engines": { "hutaomu": "0.1.0" },  // 可选：声明期望的宿主版本
  "permissions": ["shell"],           // 只允许 fs.read/fs.write/net/shell
  "contributes": {
    "themes":  ["themes/sunrise"],                       // 目录或 .htmtpi
    "formats": [{ "extension": "log", "language": "plaintext", "label": "日志" }],
    "commands": [
      { "id": "toggleWrap", "title": "切换自动换行", "action": "view.toggleWordWrap",
        "shortcut": "Ctrl+Alt+W" },
      { "id": "setSunriseTheme", "title": "切换到「日出」主题",
        "action": "theme.set:sunrise" },
      { "id": "revealInExplorer", "title": "在文件管理器中显示当前文件",
        "shell": "explorer", "args": "/select,{file}" }
    ]
  }
}
```

### 2.1 内置动作清单（`commands[].action`）

| 动作 | 说明 |
|---|---|
| `editor.new` / `editor.save` / `editor.saveAs` / `editor.close` | 文件级操作 |
| `editor.find` / `editor.quickOpen` | 查找条 / 快速打开 |
| `editor.zoomIn` / `editor.zoomOut` / `editor.resetZoom` | 字号缩放 |
| `view.toggleSidebar` / `view.toggleOutline` / `view.toggleWordWrap` / `view.toggleTheme` | 视图开关 |
| `view.markdownLive` / `view.markdownSplit` / `view.markdownSource` | Markdown 视图模式 |
| `app.settings` / `app.about` / `app.openFolder` / `app.openFile` | 应用级操作 |
| `theme.set:<主题 id>` | 切换到指定主题（含插件自带主题） |

### 2.2 状态栏部件（`contributes.statusBar`）

```json
{ "id": "words", "type": "wordCount", "alignment": "right",
  "visibleFor": ["markdown", "text"] }
```

| 键 | 取值 | 说明 |
|---|---|---|
| `type` | `wordCount` / `charCount` / `lineCount` / `indent` / `eol` / `encoding` | **只能是宿主内置类型**（插件不提供计算代码） |
| `alignment` | `left` / `right`（默认） | 状态栏左侧还是右侧 |
| `visibleFor` | 语言 id 数组 | 只在这些语言下显示；空 = 始终显示 |

`wordCount` 按空白切分（中英混排按字块计），`indent` 显示"缩进 级数（N 空格）"。

### 2.3 侧栏页面（`contributes.pages`）

```json
{ "id": "guide", "title": "插件说明", "type": "markdown",
  "content": "README.md", "icon": "assets/icons/page.svg", "defaultSide": "left" }
```

| 键 | 取值 | 说明 |
|---|---|---|
| `type` | `markdown` / `text` / `fileList` | 渲染包内 Markdown / 纯文本 / 列出包内某目录的文件 |
| `content` | 包内相对路径 | 文件（markdown/text）或目录（fileList）；越界路径会被拒绝 |
| `icon` | 包内 svg 相对路径 | 可选；支持 `FILLCOLOR` 占位符随主题换色 |
| `defaultSide` | `left` / `right` | 默认停靠侧（之后可以拖动换列/收起，与内置面板一致） |

页面内容**不执行任何脚本**：markdown 渲染为静态 HTML，`fileList` 双击可打开文件。

### 2.4 外部命令与占位符

`args` 支持三个占位符（没有对应值时参数会被清空，避免把字面量传给外部程序）：

| 占位符 | 含义 |
|---|---|
| `{file}` | 当前文件绝对路径 |
| `{dir}` | 当前文件所在目录 |
| `{workspace}` | 当前工作区路径 |

## 三、预装（随包分发）

官方插件**随程序预装**，放在**程序目录旁的 `plugins/`**（`<安装目录>/plugins/<插件 id>/`），
在 `设置 → 插件` 里标记为「预装」：**可以禁用、不能卸载**（想停用就取消勾选）。

- 便携部署/多套插件目录：环境变量 `HUTAOMU_PLUGIN_DIRS`（`;` 分隔）追加搜索目录；
- 同 id 的插件若同时存在于用户目录（`<AppData>/plugins`）与预装目录，**用户副本生效**，
  便于覆盖预装版本做更新；
- 当前随包的两套：`hutaomu.official-themes`（4 套官方主题 + 2 条切换主题命令）、
  `hutaomu.config-formats`（13 个扩展名 + 缩进状态栏段）。

> 构建目录里它们位于 `<build>/Release/plugins/<id>/`（`cmake --build` 会自动铺开），
> 可以直接用 `--list-plugins` 看到。

## 四、安装与验证

```
构建产物：<build>/plugins/hutaomu.demo-toolkit.htmed
安装：设置 → 插件 → 导入插件包…（会先展示贡献点与申请的权限）
```

- 安装位置：`%APPDATA%/Hutaomu/Hutaomu Editor/plugins/<id>/`
  （Linux/macOS 为 `QStandardPaths::AppDataLocation/plugins`）
- 启用状态与 shell 信任：同目录 `state.json`（与用户设置分开存）
- 卸载：设置 → 插件 → 卸载插件（其主题/格式/命令一并消失）

**排查"插件装了但没生效"**：

```bash
HutaomuEditor.exe --list-plugins
# 打印插件目录、每个插件的启用状态与全部贡献点
```

**脚本化验证命令**：

```bash
# 执行插件命令并正常退出（会走正常保存路径）
HutaomuEditor.exe --run-plugin-command hutaomu.demo-toolkit.toggleWrap
```

## 五、安全校验（安装时执行）

- `id` 必须是 `publisher.name`，且 `publisher` 必须是 id 前缀；目录名必须等于 id；
- 包内路径不得为绝对路径、不得含盘符、不得出现 `..`；符号链接一律拒绝；
- 条目数与解压后总量有上限（4000 条 / 64MB）；
- 声明了 `shell` 命令却未声明 `shell` 权限的清单**直接拒绝安装**；
- 覆盖安装保留原有的启用状态与信任记录。

## 六、自己写一个插件（最小步骤）

1. 新建目录 `<publisher>.<name>/`，写 `plugin.json`（`id` 与目录名一致）；
2. 需要的主题放到 `themes/<主题 id>/`（可从 [themes/README.md](../themes/README.md)
   复制一个现成主题再改色，保证色键完整）；
3. 压成 ZIP 并把扩展名改成 `.htmed`（或用 `cmake --build build/Release
   --target plugin_packages` 的产物格式作参考）；
4. 设置 → 插件 → 导入插件包…，确认权限页后即生效（无需重启）。
