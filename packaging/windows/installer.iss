; Hutaomu Editor - Inno Setup 安装包脚本
; 生成：ISCC.exe installer.iss → dist\HutaomuEditor-Setup-<版本>.exe
;
; 可选内容分两部分：
;   [Components] 决定装哪些插件（官方主题包 / 配置文件格式包 / 示例插件）
;   [Tasks]      决定是否建立文件关联、是否加右键菜单
; 两者都能在向导里勾选；静默安装可用 /COMPONENTS="..." /TASKS="..." 指定。

#define AppName "Hutaomu Editor"
#define AppVersion "0.1.0"
#define AppPublisher "Hutaomu"
#define AppExe "HutaomuEditor.exe"
; 关联用的 ProgId 前缀（改这里时 [Registry] 里的名字要一起改）
#define ProgId "HutaomuEditor"

[Setup]
; 注意：AppId 一旦发布就不能再改 —— 它是"认出旧安装/覆盖升级/卸载干净"的唯一凭据。
; （开发期曾短暂改成别的值，导致旧安装认不出来、卸载留残留，这里已恢复。）
AppId={{8E5B7C41-6D2A-4F53-9B3E-HUTAOMU00001}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
OutputDir=..\..\dist
OutputBaseFilename=HutaomuEditor-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
; 写了文件关联就必须打开它：安装完成后通知外壳刷新图标/关联
ChangesAssociations=yes
; 覆盖安装：认出旧安装、沿用上次的安装位置/组件/任务/权限模式
UsePreviousAppDir=yes
UsePreviousTasks=yes
UsePreviousSetupType=yes
UsePreviousPrivileges=yes
; 程序正在运行时提示关闭（避免留下被占用、删不掉的 DLL）
CloseApplications=yes
RestartApplications=no
; 与程序里的命名互斥体同名：安装/卸载时能发现"程序正在运行"
AppMutex=HutaomuEditorMutex
SetupIconFile=app.ico
; 品牌向导图（由 make-art.py 生成；Inno 只认 BMP/PNG/JPEG）
WizardImageFile=wizard-big.bmp
WizardSmallImageFile=wizard-small.bmp
; 开始菜单组页对这类小工具没必要，去掉可少一步
DisableProgramGroupPage=yes
; 版本信息（在文件属性里可见）
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} 安装程序
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
VersionInfoCopyright=Copyright (C) 2026 {#AppPublisher}

[Types]
Name: "full"; Description: "完整安装（推荐）"
Name: "compact"; Description: "精简安装（不含官方扩展与示例）"
Name: "custom"; Description: "自定义安装"; Flags: iscustom

[Components]
Name: "plugins"; Description: "官方扩展与示例"; Types: full
Name: "plugins\themes"; Description: "官方主题包：沉浸写作 Typora / 卡片笔记 Obsidian / VS Code 深浅（4 套主题）"; Types: full
Name: "plugins\formats"; Description: "配置文件格式包：认领 .env/.ini/.conf/.log/.jsx/.tsx 等 13 种扩展名"; Types: full
Name: "plugins\sample"; Description: "示例插件包（.htmed，可在「设置 → 插件」导入，供学习插件写法）"; Types: full

[Files]
; 程序与 Qt 运行时（必装）：dist\HutaomuEditor 由 build-installer.bat 铺好
Source: "..\..\dist\HutaomuEditor\*"; DestDir: "{app}"; Excludes: "plugins\*"; \
    Flags: recursesubdirs createallsubdirs ignoreversion
; 以下是可选的插件内容（对应上面的 [Components]）
Source: "..\..\dist\HutaomuEditor\plugins\hutaomu.official-themes\*"; \
    DestDir: "{app}\plugins\hutaomu.official-themes"; \
    Components: plugins\themes; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\..\dist\HutaomuEditor\plugins\hutaomu.config-formats\*"; \
    DestDir: "{app}\plugins\hutaomu.config-formats"; \
    Components: plugins\formats; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\..\dist\HutaomuEditor\plugins\*.htmed"; DestDir: "{app}\plugins"; \
    Components: plugins\sample; Flags: ignoreversion

[Languages]
; Default.isl 提供完整消息集，chinese.isl 覆盖常用条目（未覆盖的罕见提示回退英文）
Name: "chinesesimplified"; MessagesFile: "compiler:Default.isl,chinese.isl"

[Messages]
; 应用专属文案（覆盖语言文件里的通用词）
SetupAppTitle=安装 Hutaomu Editor
SetupWindowTitle=安装 Hutaomu Editor
UninstallAppTitle=卸载 Hutaomu Editor
UninstallAppFullTitle=卸载 Hutaomu Editor
WelcomeLabel1=欢迎安装 Hutaomu Editor
WelcomeLabel2=这是一个原生跨平台的文本、代码与 Markdown 编辑器。%n%n继续之前，建议先关闭其他正在运行的程序。
SelectComponentsDesc=要安装哪些内容？
SelectComponentsLabel2=选择要安装的内容，然后点"下一步"。官方扩展可以稍后在「设置 → 插件」里启用或禁用。
ReadyLabel1=安装程序已准备好把 Hutaomu Editor 装到你的电脑上。
ReadyLabel2a=点"安装"开始；想改设置就点"上一步"。
FinishedLabel=安装完成，Hutaomu Editor 已就绪。%n%n若要让 .md 等文件"双击就用它打开"：Windows 10/11 需要你到「设置 → 应用 → 默认应用」里选一次 Hutaomu Editor（系统不允许安装程序代改默认程序）。
ClickFinish=点"完成"退出安装程序。
ConfirmUninstall=确定要完全卸载 Hutaomu Editor 吗？%n%n你的个人设置与自行导入的插件保存在用户目录，不会被删除。
SelectTasksLabel2=选择安装 Hutaomu Editor 时要执行的附加任务，然后点"下一步"。
SelectTasksDesc=还要做哪些附加任务？

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\卸载 {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务："; Flags: checkedonce
Name: "assoc_markdown"; Description: "关联 Markdown 文档（.md .markdown .mkd）"; \
    GroupDescription: "文件关联与右键菜单："; Flags: checkedonce
Name: "assoc_text"; Description: "关联纯文本与日志（.txt .log）"; \
    GroupDescription: "文件关联与右键菜单："; Flags: checkedonce
Name: "assoc_code"; Description: "关联常见代码与配置文件（.c .cpp .py .js .ts .json .yml .ini 等）"; \
    GroupDescription: "文件关联与右键菜单："
Name: "contextmenu"; Description: "在资源管理器右键菜单加入「用 Hutaomu Editor 打开」（所有文件）"; \
    GroupDescription: "文件关联与右键菜单："

[Registry]
; ---- 能力注册：Windows「设置 → 应用 → 默认应用」里可一键把本程序设为默认 ----
; Win10/11 不允许安装程序静默改写"默认应用"（UserChoice 受哈希保护），
; 所以按官方做法声明能力，用户点一次即可生效。
Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; \
    ValueName: "{#ProgId}"; ValueData: "Software\Hutaomu\{#AppName}\Capabilities"; \
    Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Hutaomu\{#AppName}\Capabilities"; ValueType: string; \
    ValueName: "ApplicationName"; ValueData: "{#AppName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Hutaomu\{#AppName}\Capabilities"; ValueType: string; \
    ValueName: "ApplicationDescription"; ValueData: "原生跨平台的文本、代码与 Markdown 编辑器"
Root: HKA; Subkey: "Software\Hutaomu\{#AppName}\Capabilities\FileAssociations"; \
    ValueType: string; ValueName: ".md"; ValueData: "{#ProgId}.md"; Tasks: assoc_markdown
Root: HKA; Subkey: "Software\Hutaomu\{#AppName}\Capabilities\FileAssociations"; \
    ValueType: string; ValueName: ".markdown"; ValueData: "{#ProgId}.md"; Tasks: assoc_markdown
Root: HKA; Subkey: "Software\Hutaomu\{#AppName}\Capabilities\FileAssociations"; \
    ValueType: string; ValueName: ".mkd"; ValueData: "{#ProgId}.md"; Tasks: assoc_markdown
Root: HKA; Subkey: "Software\Hutaomu\{#AppName}\Capabilities\FileAssociations"; \
    ValueType: string; ValueName: ".txt"; ValueData: "{#ProgId}.txt"; Tasks: assoc_text
Root: HKA; Subkey: "Software\Hutaomu\{#AppName}\Capabilities\FileAssociations"; \
    ValueType: string; ValueName: ".log"; ValueData: "{#ProgId}.txt"; Tasks: assoc_text

; ---- "打开方式"列表里始终出现本程序（不带任务，装了就能右键→打开方式找到） ----
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}\shell\open\command"; \
    ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}\DefaultIcon"; \
    ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}\SupportedTypes"; \
    ValueType: string; ValueName: ".md"; ValueData: ""; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}\SupportedTypes"; \
    ValueType: string; ValueName: ".txt"; ValueData: ""

; ---- Markdown 关联（任务：assoc_markdown） ----
Root: HKA; Subkey: "Software\Classes\{#ProgId}.md"; ValueType: string; ValueName: ""; \
    ValueData: "Markdown 文档"; Flags: uninsdeletekey; Tasks: assoc_markdown
Root: HKA; Subkey: "Software\Classes\{#ProgId}.md\DefaultIcon"; ValueType: string; \
    ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: assoc_markdown
Root: HKA; Subkey: "Software\Classes\{#ProgId}.md\shell\open\command"; ValueType: string; \
    ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: assoc_markdown
Root: HKA; Subkey: "Software\Classes\.md"; ValueType: string; ValueName: ""; \
    ValueData: "{#ProgId}.md"; Flags: uninsdeletevalue; Tasks: assoc_markdown
Root: HKA; Subkey: "Software\Classes\.markdown"; ValueType: string; ValueName: ""; \
    ValueData: "{#ProgId}.md"; Flags: uninsdeletevalue; Tasks: assoc_markdown
Root: HKA; Subkey: "Software\Classes\.mkd"; ValueType: string; ValueName: ""; \
    ValueData: "{#ProgId}.md"; Flags: uninsdeletevalue; Tasks: assoc_markdown

; ---- 纯文本 / 日志关联（任务：assoc_text） ----
Root: HKA; Subkey: "Software\Classes\{#ProgId}.txt"; ValueType: string; ValueName: ""; \
    ValueData: "文本文档"; Flags: uninsdeletekey; Tasks: assoc_text
Root: HKA; Subkey: "Software\Classes\{#ProgId}.txt\DefaultIcon"; ValueType: string; \
    ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: assoc_text
Root: HKA; Subkey: "Software\Classes\{#ProgId}.txt\shell\open\command"; ValueType: string; \
    ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: assoc_text
Root: HKA; Subkey: "Software\Classes\.txt"; ValueType: string; ValueName: ""; \
    ValueData: "{#ProgId}.txt"; Flags: uninsdeletevalue; Tasks: assoc_text
Root: HKA; Subkey: "Software\Classes\.log"; ValueType: string; ValueName: ""; \
    ValueData: "{#ProgId}.txt"; Flags: uninsdeletevalue; Tasks: assoc_text

; ---- 代码 / 配置关联（任务：assoc_code） ----
Root: HKA; Subkey: "Software\Classes\{#ProgId}.code"; ValueType: string; ValueName: ""; \
    ValueData: "源代码文件"; Flags: uninsdeletekey; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\{#ProgId}.code\DefaultIcon"; ValueType: string; \
    ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\{#ProgId}.code\shell\open\command"; ValueType: string; \
    ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: assoc_code
; 代码/配置扩展名逐个列在下面（Inno 不支持循环生成条目）
Root: HKA; Subkey: "Software\Classes\.c"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.h"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.cpp"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.hpp"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.cc"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.py"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.js"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.mjs"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.ts"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.tsx"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.jsx"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.json"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.html"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.htm"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.css"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.xml"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.yml"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.yaml"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.toml"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.ini"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.sql"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code
Root: HKA; Subkey: "Software\Classes\.sh"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}.code"; Flags: uninsdeletevalue; Tasks: assoc_code

; ---- 资源管理器右键菜单（任务：contextmenu，默认不勾） ----
Root: HKA; Subkey: "Software\Classes\*\shell\{#ProgId}"; ValueType: string; ValueName: ""; \
    ValueData: "用 Hutaomu Editor 打开"; Flags: uninsdeletekey; Tasks: contextmenu
Root: HKA; Subkey: "Software\Classes\*\shell\{#ProgId}"; ValueType: string; ValueName: "Icon"; \
    ValueData: "{app}\{#AppExe},0"; Tasks: contextmenu
Root: HKA; Subkey: "Software\Classes\*\shell\{#ProgId}\command"; ValueType: string; \
    ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: contextmenu

[Run]
Filename: "{app}\{#AppExe}"; Description: "立即运行 {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\translations"
; 预装插件目录刻意不加入卸载清理：用户可能在里面手工放了插件，
; Inno 默认只删自己装过的文件，留下的用户文件不会被误删。

[Code]
// 卸载时程序还在运行的话，exe/DLL 被占用会删不干净（实测留下 12 个文件）。
// usUninstall 是"开始实际卸载（删文件）之前"的官方时机，这里先结束进程。
// 交互式卸载此前已由系统提示用户关闭程序，这一步主要保静默卸载与兜底。
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  // 用 {sysnative}（真 64 位 System32）：卸载器是 32 位进程，{sys} 会被
  // WOW64 重定向到 SysWOW64，那里没有 taskkill.exe（探针实测返回码=2，
  // 即"找不到可执行文件"）——这就是之前"运行中卸载不干净"的根因。
  if CurUninstallStep = usUninstall then begin
    Exec(ExpandConstant('{sysnative}	askkill.exe'), '/IM HutaomuEditor.exe /F', '',
         SW_HIDE, ewWaitUntilTerminated, ResultCode);
    SaveStringToFile(ExpandConstant('{localappdata}\hutaomu-uninstall-probe.txt'),
                     'sysnative kill result=' + IntToStr(ResultCode), False);
  end;
end;
