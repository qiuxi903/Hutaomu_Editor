; Hutaomu Editor - Inno Setup 简体中文覆盖消息
;
; 用法（见 installer.iss）：
;   [Languages]
;   Name: "chinesesimplified"; MessagesFile: "compiler:Default.isl,chinese.isl"
;
; Inno 支持逗号分隔的多个消息文件，后面的覆盖前面的：
; 因此这里只列"实际会出现在向导里"的条目，未列到的（罕见错误等）
; 自动回退到 Default.isl 的英文，不会导致编译失败。
; 应用专属文案（标题/欢迎语等）放在 installer.iss 的 [Messages] 段。

[Messages]
SetupAppTitle=安装程序
SetupWindowTitle=安装 - %1
UninstallAppTitle=卸载
UninstallAppFullTitle=%1 卸载
WelcomeLabel1=欢迎使用 [name] 安装向导
WelcomeLabel2=即将在你的电脑上安装 [name/ver]。%n%n继续之前，建议先关闭其他正在运行的程序。
SelectDirDesc=[name] 要装到哪里？
SelectDirLabel3=安装程序会把 [name] 装到下面的文件夹。
SelectDirBrowseLabel=点"下一步"继续；想换目录就点"浏览"。
DiskSpaceGBLabel=至少需要 [gb] GB 可用空间。
DiskSpaceMBLabel=至少需要 [mb] MB 可用空间。
WizardSelectDir=选择安装位置
WizardSelectProgramGroup=选择开始菜单文件夹
SelectTasksDesc=还要做哪些附加任务？
SelectTasksLabel2=选择安装 [name] 时要执行的附加任务，然后点"下一步"。
WizardReady=准备安装
ReadyLabel1=安装程序已准备好，即将把 [name] 装到你的电脑上。
ReadyLabel2a=点"安装"开始安装；想回头改设置就点"上一步"。
ReadyLabel2b=点"安装"开始安装。
WizardPreparing=正在准备
PreparingDesc=安装程序正在准备安装 [name]。
WizardInstalling=正在安装
InstallingLabel=正在安装 [name]，请稍候。
ExtractingLabel=正在解压文件…
FinishedLabel=安装完成，[name] 已装到你的电脑上。可以用开始菜单里的快捷方式启动它。
FinishedLabelNoIcons=安装完成，[name] 已装到你的电脑上。
ClickFinish=点"完成"退出安装程序。
ButtonNext=下一步(&N) >
ButtonBack=< 上一步(&B)
ButtonInstall=安装(&I)
ButtonCancel=取消
ButtonFinish=完成(&F)
ButtonBrowse=浏览(&B)…
ButtonYes=是(&Y)
ButtonNo=否(&N)
ButtonOK=确定
ConfirmUninstall=确定要完全卸载 %1 及其所有组件吗？
UninstallStatusLabel=正在从你的电脑上卸载 %1，请稍候。
UninstalledAll=%1 已成功卸载。
UninstalledMost=%1 卸载完成。%n%n有少量文件未能删除，可以手动清理。
UninstalledAndNeedsRestart=要完成 %1 的卸载，需要重启电脑。%n%n现在就重启吗？
ErrorCopying=复制文件时出错：
ErrorCreatingDir=安装程序无法创建目录 "%1"
ErrorTooManyFilesInDir=无法在目录 "%1" 中创建文件：该目录里的文件太多了
ErrorChangingAttr=修改已存在文件的属性时出错：
ErrorCloseApplications=安装程序无法自动关闭所有程序。建议先手动关闭正在使用待更新文件的程序，再继续。
SetupAppRunningError=安装程序检测到 %1 正在运行。%n%n请先关闭它的所有窗口，然后点"确定"继续，或点"取消"退出。
UninstallAppRunningError=卸载程序检测到 %1 正在运行。%n%n请先关闭它的所有窗口，然后点"确定"继续，或点"取消"退出。
AdminPrivilegesRequired=安装本程序需要以管理员身份登录。
PowerUserPrivilegesRequired=安装本程序需要以管理员或 Power Users 组成员身份登录。
PrivilegesRequiredOverrideTitle=选择安装模式
PrivilegesRequiredOverrideInstruction=请选择安装模式
PrivilegesRequiredOverrideText1=%1 可以装给所有用户（需要管理员权限），也可以只装给你自己。
PrivilegesRequiredOverrideText2=%1 可以只装给你自己，也可以装给所有用户（需要管理员权限）。
PrivilegesRequiredOverrideAllUsers=装给所有用户(&A)
PrivilegesRequiredOverrideAllUsersRecommended=装给所有用户(&A)（推荐）
PrivilegesRequiredOverrideCurrentUser=只装给我自己(&M)
PrivilegesRequiredOverrideCurrentUserRecommended=只装给我自己(&M)（推荐）
UninstallDisplayNameMarkAllUsers=所有用户
UninstallDisplayNameMarkCurrentUser=当前用户
ExitSetupTitle=退出安装
ExitSetupMessage=安装尚未完成。现在退出的话，程序不会被安装。%n%n你可以稍后再运行安装程序来完成安装。%n%n确定退出安装吗？
WizardSelectComponents=选择组件
WizardSelectTasks=选择附加任务
FinishedHeadingLabel=[name] 安装完成
ConfirmTitle=请确认
SelectComponentsDesc=要安装哪些组件？
SelectComponentsLabel2=选择要安装的组件；不想装的就取消勾选。选好后点"下一步"。
SetupLdrStartupMessage=即将安装 %1。是否继续？
UninstallNotFound=文件 "%1" 不存在，无法卸载。
UninstallOpenError=无法打开文件 "%1"，无法卸载。
UninstallUnsupportedVer=卸载记录文件 "%1" 的格式不被当前卸载程序识别，无法卸载。
ErrorRestartReplace=替换文件失败（重启后才能完成）：
