# 开发与提交流程约定

本文件记录本仓库的工作约定。**每次改动完成后不要问"要不要构建"，直接构建并自测**。

## 一、每次改动后的固定动作（必做）

```bash
# 1) 构建（所有目标）
export PATH="/g/Qt/6.10.3/mingw_64/bin:/g/Qt/Tools/mingw1310_64/bin:$PATH"
cmake --build build/Release

# 2) 跑全量测试（当前 18 个套件）
cd build/Release && ctest
```

报告里必须给出**实际的测试结果**（通过套件数/失败项），不允许只写"应该可以"。

改动涉及以下内容时，还要额外做对应自检：

| 改动范围 | 额外自检 |
|---|---|
| 打包脚本 / 安装包（`packaging/**`、`installer.iss`、`*.isl`） | 跑 `packaging/windows/build-installer.bat`，然后**静默安装到临时目录 + 启动 `--list-plugins`/`--check-associations` + 卸载**，确认 0 残留 |
| 文件关联逻辑 | `HutaomuEditor.exe --set-associations` 后跑 `--check-associations` |
| 主题 / 插件 | `--list-plugins`、`--list-commands` |
| 查看器（Office/PDF/图片/媒体） | 对应测试套件（`OfficeXlsxTest` / `ViewerTest` / `ViewerFuzzTest` 等） |

## 二、提交规范

- 提交信息用中文，说明**改了什么 + 怎么验证的**（例：`修复标题栏无法拖动窗口` + 根因 + 测试项）；
- 一个提交只做一件事；纯文档/纯测试与功能改动分开更清晰；
- 推送前确保工作区干净（`git status`），不要在仓库里留下个人测试文件。

## 三、仓库卫生（重要）

以下内容**不入库**（已在 `.gitignore` 中）：

- `build/`、`dist/`：构建产物与打包产物；
- `*.docx` / `*.pptx` / `*.xlsx`：本地测试用的 Office 文件（`tests/` 夹具另行放白名单目录）；
- 个人文件（如证书 PDF）。

第三方源码（`third_party/**`）是**真实文件入库**（不是 submodule / gitlink）——
克隆即可构建。若更新第三方库，请确保不带回 `.git` 目录（否则会被提交成空壳 gitlink）。

## 四、已知环境坑（踩过的，别再踩）

| 坑 | 做法 |
|---|---|
| `.bat` 脚本里的中文在 cmd 下乱码/解析失败 | 打包脚本一律用 **ASCII 英文提示** |
| Inno 的 `.iss` / `.isl` 需要 **CRLF**；`\` 续行在 LF 下解析错位 | 改动后转回 CRLF |
| Inno 的任务名/组件名**不能带点**（`assoc.markdown` 非法） | 用下划线：`assoc_markdown` |
| `AppId` 是升级识别键 | **发布后不可再改**（改了会认不出旧安装、卸载留残留） |
| 32 位卸载器里 `{sys}` 被重定向到 SysWOW64（没有 taskkill.exe） | 用 `{sysnative}` |
| 用 shell heredoc 写含 `\n`/反斜杠的 C++ 代码会被转义吃掉 | **别用 heredoc 写代码**，用文件写入或编辑器精确编辑 |
| `QTableView::selectedIndexes()` 是 protected | 用 `selectionModel()->selectedIndexes()` |
| `QString::number(v,'f',-1)` 会按 6 位小数输出 | 用 `'g'` 控制有效数字 |
| 清理调试输出时用 `[\s\S]*?` 正则跨行匹配 | 会误删代码；用锚点精确编辑 |

## 五、测试落点参考

| 主题 | 测试文件 |
|---|---|
| Office 查看器（xlsx/docx/pptx 渲染） | `tests/OfficeXlsxTest.cpp` |
| 查看器工厂/PDF/降级 | `tests/ViewerTest.cpp`、`tests/ViewerFuzzTest.cpp` |
| 窗口交互（标题栏拖动、面板、缩放） | `tests/ViewModeTest.cpp`、`tests/LayoutDragTest.cpp` |
| 主题（配色/度量/资源/布局） | `tests/ThemePackageTest.cpp`、`ThemeAssetTest.cpp`、`ThemeLayoutTest.cpp` |
| 插件（安装/权限/贡献点/命令面板） | `tests/PluginTest.cpp`、`CommandPaletteTest.cpp` |
