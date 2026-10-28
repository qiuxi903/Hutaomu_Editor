; Hutaomu Editor (Rust) - Inno Setup 安装包脚本
; 生成：ISCC.exe installer.iss → dist\HutaomuEditorRust-Setup-<版本>.exe
;
; 与 Qt 版 installer.iss 同构：
;   [Components] 决定装哪些内容（程序本体 / 官方插件）
;   [Tasks]      决定是否建立文件关联、是否加桌面图标
; 关联写 HKCU（无需管理员），卸载时 uninsdeletekey 清理。

#define AppName "Hutaomu Editor (Rust)"
#define AppVersion "0.1.0"
#define AppPublisher "Hutaomu"
#define AppExe "hutaomu-editor.exe"
; 关联用的 ProgId 前缀（与 src/associations.rs 的 PROG_ID 保持一致）
#define ProgId "HutaomuEditorRust.Document"

[Setup]
; AppId 独立于 Qt 版：Rust 版与 Qt 版可并存安装，互不覆盖
AppId={{7C41B8E5-4F53-6D2A-9B3E-RUST00000001}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\Hutaomu Editor Rust
DefaultGroupName=Hutaomu Editor Rust
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
OutputDir=..\..\dist
OutputBaseFilename=HutaomuEditorRust-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
; 关联变更后通知外壳刷新
ChangesAssociations=yes
UsePreviousAppDir=yes
UsePreviousTasks=yes
UsePreviousSetupType=yes
UsePreviousPrivileges=yes
; 中文向导（引用 Qt 版同目录语言文件；找不到时回退默认英文）
ShowLanguageDialog=no

[Languages]
Name: "chinese"; MessagesFile: "..\windows\chinese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "完整安装（程序 + 官方插件）"
Name: "compact"; Description: "仅程序本体"
Name: "custom"; Description: "自定义"; Flags: iscustom

[Components]
Name: "app"; Description: "Hutaomu Editor (Rust) 程序"; Types: full compact custom; Flags: fixed
Name: "plugins"; Description: "官方插件（主题包 / 配置格式包 / 示例插件）"; Types: full

[Tasks]
Name: "assoc_md"; Description: "关联 Markdown 文件 (*.md)"; GroupDescription: "文件关联（当前用户）："; Flags: unchecked
Name: "assoc_text"; Description: "关联纯文本文件 (*.txt *.log)"; GroupDescription: "文件关联（当前用户）："; Flags: unchecked
Name: "assoc_code"; Description: "关联代码文件 (*.rs *.py *.js *.ts *.json *.toml *.yaml)"; GroupDescription: "文件关联（当前用户）："; Flags: unchecked
Name: "assoc_office"; Description: "关联 Office / PDF（只读查看 *.docx *.xlsx *.pptx *.pdf）"; GroupDescription: "文件关联（当前用户）："; Flags: unchecked
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "其他："

[Files]
; 程序本体
Source: "..\..\rust\target\release\hutaomu-editor.exe"; DestDir: "{app}"; DestName: "{#AppExe}"; Components: app; Flags: ignoreversion
; pdfium（PDF 查看器，运行时动态加载）
Source: "..\..\third_party\pdfium\bin\pdfium.dll"; DestDir: "{app}"; Components: app; Flags: ignoreversion skipifsourcedoesntexist
; 主题
Source: "..\..\rust\themes\*.json"; DestDir: "{app}\themes"; Components: app; Flags: ignoreversion
; 官方插件（与 Qt 版同源，清单格式兼容）
Source: "..\..\plugins\*"; DestDir: "{app}\plugins"; Components: plugins; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\Hutaomu Editor (Rust)"; Filename: "{app}\{#AppExe}"
Name: "{group}\卸载 Hutaomu Editor (Rust)"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Hutaomu Editor (Rust)"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
; ProgId（HKCU，无需管理员；卸载自动清理）
Root: HKCU; Subkey: "Software\Classes\{#ProgId}"; ValueType: string; ValueName: ""; ValueData: "Hutaomu Editor (Rust) 文档"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\{#ProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Flags: uninsdeletekey
; 按任务关联扩展名
Root: HKCU; Subkey: "Software\Classes\.md"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_md; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.txt"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_text; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.log"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_text; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.rs"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_code; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.py"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_code; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.js"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_code; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.ts"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_code; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.json"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_code; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.toml"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_code; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.yaml"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_code; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.docx"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_office; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.xlsx"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_office; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.pptx"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_office; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.pdf"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Tasks: assoc_office; Flags: uninsdeletevalue
; 注册到系统的"默认应用"候选列表
Root: HKCU; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "Hutaomu Editor (Rust)"; ValueData: "Software\HutaomuRust\Capabilities"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\HutaomuRust\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "Hutaomu Editor (Rust)"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\HutaomuRust\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "原生跨平台文本 / 代码 / Markdown / Office 编辑器（Rust 版）"
Root: HKCU; Subkey: "Software\HutaomuRust\Capabilities"; ValueType: string; ValueName: "ApplicationIcon"; ValueData: "{app}\{#AppExe},0"
Root: HKCU; Subkey: "Software\HutaomuRust\Capabilities\FileAssociations"; ValueType: string; ValueName: ".md"; ValueData: "{#ProgId}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\HutaomuRust\Capabilities\FileAssociations"; ValueType: string; ValueName: ".txt"; ValueData: "{#ProgId}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\HutaomuRust\Capabilities\FileAssociations"; ValueType: string; ValueName: ".json"; ValueData: "{#ProgId}"; Flags: uninsdeletekey

[Run]
Filename: "{app}\{#AppExe}"; Description: "启动 {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; 卸载后通知外壳刷新关联
Filename: "rundll32.exe"; Parameters: "user32.dll,UpdatePerUserSystemParameters"; Flags: runhidden
