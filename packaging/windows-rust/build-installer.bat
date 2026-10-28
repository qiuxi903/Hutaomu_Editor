@echo off
rem Hutaomu Editor (Rust) - 一键构建 + 出安装包（Inno Setup）+ 便携 ZIP
rem 依赖：cargo、Inno Setup 6（ISCC.exe）
setlocal enabledelayedexpansion
cd /d "%~dp0..\.."
set ROOT=%cd%

echo [1/3] cargo build --release ...
pushd rust
cargo build --release
if errorlevel 1 ( popd & echo 构建失败 & exit /b 1 )
popd

set "ISCC="
where ISCC.exe >nul 2>nul && set "ISCC=ISCC.exe"
if not defined ISCC if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"

echo [2/3] Inno Setup 打包 ...
if defined ISCC (
  "%ISCC%" "%~dp0installer.iss"
  if errorlevel 1 ( echo 打包失败 & exit /b 1 )
) else (
  echo [!] 未找到 Inno Setup（ISCC.exe），跳过安装包，仅生成便携 ZIP。
)

echo [3/3] 便携 ZIP ...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
 "$root='%ROOT%'; $d=Join-Path $root 'dist\HutaomuEditorRust-Portable';" ^
 "Remove-Item $d -Recurse -Force -ErrorAction SilentlyContinue;" ^
 "New-Item -ItemType Directory -Path (Join-Path $d 'themes') -Force | Out-Null;" ^
 "Copy-Item (Join-Path $root 'rust\target\release\hutaomu-editor.exe') $d -Force;" ^
 "Copy-Item (Join-Path $root 'rust\themes\*.json') (Join-Path $d 'themes') -Force;" ^
 "Copy-Item (Join-Path $root 'third_party\pdfium\bin\pdfium.dll') $d -Force -ErrorAction SilentlyContinue;" ^
 "Compress-Archive -Path (Join-Path $d '*') -DestinationPath (Join-Path $root 'dist\HutaomuEditorRust-Portable.zip') -Force;" ^
 "Remove-Item $d -Recurse -Force"

echo 完成：
if defined ISCC echo   dist\HutaomuEditorRust-Setup-0.1.0.exe
echo   dist\HutaomuEditorRust-Portable.zip
exit /b 0
