# 打包与分发

本目录存放三端打包脚本。当前状态：

| 平台 | 状态 | 脚本 | 产物 |
|---|---|---|---|
| **Windows** | ✅ 可用并已实测 | `windows/build-installer.bat` | `dist/HutaomuEditor-Setup-<版本>.exe`（Inno Setup 安装包）+ `dist/HutaomuEditor-<版本>-win64.zip`（便携版） |
| **macOS** | ⛔ 未做 | — | 需 `.app` 打包 + 签名/公证（M5 出口） |
| **Linux** | ⛔ 未做 | — | 建议 AppImage（或 deb）+ 系统 WebKitGTK 依赖说明 |

## 一、Windows：一条命令出包

```bat
packaging\windows\build-installer.bat
```

它依次做四件事：

1. `build.bat Release` —— 配置 + 构建（预装插件在构建期自动铺到 `<build>\plugins\`）；
2. 部署运行时到 `dist\HutaomuEditor\`：`HutaomuEditor.exe` + `windeployqt` 生成的 Qt DLL/插件，
   外加 **`pdfium.dll`**（PDF 查看器的运行时依赖，缺失会导致程序启动即失败）；
3. 装入**预装插件** `dist\HutaomuEditor\plugins\`（官方主题包 / 配置文件格式包 / 示例 `.htmed`），
   并校验 `hutaomu.official-themes\plugin.json` 存在——缺了直接报错退出，避免"静默少装插件"；
4. 打便携 ZIP（`cmake -E tar --format=zip`）+ 调 `ISCC.exe` 生成安装包。

### 定制点（想改外观改这里）

| 想改什么 | 改哪个文件 |
|---|---|
| 向导界面文字 | `chinese.isl`（通用条目）+ `installer.iss` 的 `[Messages]`（本产品专属文案） |
| 又发现哪句是英文 | 在 `Default.isl` 里搜到那句话的键名，把 `键名=中文` 加进 `chinese.isl` 即可（只覆盖用到的键，**缺失的自动回退英文、不会编译失败**） |
| 向导配图（左侧大图/右上小图） | `make-art.py`（纯 Python 画 BMP，改颜色/元素后重新运行） |
| 安装包图标 | `make-icon.py`（生成 `app.ico`） |
| 应用名/版本/发布者、开始菜单项 | `installer.iss` 顶部 `#define` 与 `[Icons]` |
| 可选组件（装哪些插件） | `installer.iss` 的 `[Components]`/`[Types]`（新增插件目录时加一个 component + 一条 `[Files]`） |
| 文件关联与右键菜单 | `installer.iss` 的 `[Tasks]` + `[Registry]`（新增扩展名补一行 `.ext` → ProgId） |

### 安装包里的可选项（实测）

| 类别 | 选项 | 静默安装参数 |
|---|---|---|
| 组件 | 官方主题包 / 配置文件格式包 / 示例插件包 | `/COMPONENTS="plugins	hemes,pluginsormats,plugins\sample"`（留空 = 都不装） |
| 关联 | Markdown / 纯文本日志 / 代码配置 | `/TASKS="desktopicon,assoc_markdown,assoc_text,assoc_code"` |
| 右键菜单 | 「用 Hutaomu Editor 打开」 | 加 `contextmenu` |

- 任务名必须是标识符（**不能带点**），所以是 `assoc_markdown` 这种写法；
- 注册表根用 `HKA`：管理员安装写 HKLM、当前用户安装写 HKCU，卸载自动清除；
- `ChangesAssociations=yes` 已打开，装完会通知外壳刷新图标与关联；
- **`AppId` 是升级识别键，发布后不可再改**（改动会导致：认不出旧安装 → 不覆盖升级 → 卸载留残留）；
- Win10/11 不允许安装程序静默成为默认程序（`UserChoice` 有哈希保护）：安装包只声明
  `Capabilities`/`RegisteredApplications`，用户到「设置 → 应用 → 默认应用」点一次即可；
- 覆盖安装用 `UsePreviousAppDir/Tasks/SetupType/Privileges=yes` 沿用上次选择，
  `CloseApplications=yes` 避免程序在运行时卸载留下被占用的 DLL。

> 向导配图只认 **BMP/PNG/JPEG**（老版本仅 BMP），脚本统一输出 24 位 BMP 以保证兼容。
> `[Messages]` 里写了 Inno 不认识的键名只会有 Warning（会被忽略），不会导致编译失败。

### 前置

- **Inno Setup 6**：本目录带离线安装器 `innosetup.exe`（装完 `ISCC.exe` 默认在
  `G:\Inno Setup 6\` 或 `%LOCALAPPDATA%\Programs\Inno Setup 6\`，脚本会依次探测）。
- Qt 6.10.3 MinGW 与工具链路径见 `build.bat` 顶部（当前硬编码 `G:\Qt\...`）。

### 安装包行为

- 安装到 `{autopf}\Hutaomu Editor`，可创建桌面快捷方式；
- **卸载不清空 `{app}\plugins`**：用户可能手工放了自己的插件，Inno 默认只删自己装过的文件，
  留下的用户文件不会被误删；
- `installer.iss` 需要 `.ico`，图标由 `make-icon.py` 生成（`app.ico`，纯 Python 无依赖）。

### 实测记录（2026-09-30）

- `build-installer.bat` 全流程通过，产出 Setup（约 19MB）与便携 ZIP（约 25MB）；
- 静默安装到临时目录后：`HutaomuEditor.exe --list-plugins` 正常输出，
  **安装副本能发现随包的 `hutaomu.config-formats` / `hutaomu.official-themes`**，
  官方主题（`themes\typora-immersive` 等）随包到位；
- 卸载后临时目录清理干净。

### 回归提醒

打包链路最容易出的问题是**运行时依赖漏带**（本次就是 `pdfium.dll`：它由 `/DELAYLOAD`
或运行时加载，缺了只有启动时才报错）。改动依赖或新增 DLL 后，请至少跑一次
"静默安装到临时目录 + 启动 `--list-plugins`"这条自检。
