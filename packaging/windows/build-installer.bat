@echo off
rem Hutaomu Editor - one-shot Windows packaging: build + deploy + plugins + zip + installer
rem Prerequisite: Inno Setup 6 (ISCC.exe). If missing, run this folder's innosetup.exe first.
rem NOTE: ASCII-only messages on purpose (cmd parses .bat in the OEM codepage).
rem NOTE: this script lives at <repo>\packaging\windows\ -> dist/build are %~dp0..\..

setlocal
set REPO=%~dp0..\..
set ISCC=

if exist "G:\Inno Setup 6\ISCC.exe" set ISCC=G:\Inno Setup 6\ISCC.exe
if exist "G:\Tools\InnoSetup6\ISCC.exe" set ISCC=G:\Tools\InnoSetup6\ISCC.exe
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" set ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe
if exist "C:\Program Files\Inno Setup 6\ISCC.exe" set ISCC=C:\Program Files\Inno Setup 6\ISCC.exe
if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe

if "%ISCC%"=="" (
    echo [ERROR] Inno Setup 6 ^(ISCC.exe^) not found.
    echo         Run innosetup.exe in this folder first, then re-run this script.
    exit /b 1
)

echo Using compiler: %ISCC%

rem 1) make sure the app is built
call "%~dp0build.bat" Release
if errorlevel 1 exit /b 1

rem 2) deploy Qt runtime into dist\HutaomuEditor
set QT_DIR=G:\Qt\6.10.3\mingw_64
set MINGW_DIR=G:\Qt\Tools\mingw1310_64
set PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%

if exist "%REPO%\dist\HutaomuEditor" rmdir /s /q "%REPO%\dist\HutaomuEditor"
mkdir "%REPO%\dist\HutaomuEditor"
copy "%REPO%\build\Release\HutaomuEditor.exe" "%REPO%\dist\HutaomuEditor\" >nul
windeployqt --release --no-opengl-sw --compiler-runtime "%REPO%\dist\HutaomuEditor\HutaomuEditor.exe"
if errorlevel 1 exit /b 1

rem 2b) pdfium.dll: runtime dependency of the PDF viewer (loaded lazily, so a
rem missing DLL only shows up when the app starts). Ship it next to the exe.
if not exist "%REPO%\third_party\pdfium\bin\pdfium.dll" (
    echo [ERROR] third_party/pdfium/bin/pdfium.dll not found.
    exit /b 1
)
copy /y "%REPO%\third_party\pdfium\bin\pdfium.dll" "%REPO%\dist\HutaomuEditor\" >nul
if errorlevel 1 exit /b 1

rem 3) preinstalled plugins: staged under <build>\plugins at build time
set BUILD_DIR=%REPO%\build\Release
if not exist "%BUILD_DIR%\plugins\hutaomu.official-themes\plugin.json" (
    echo [ERROR] preinstalled plugins not found: %BUILD_DIR%\plugins
    echo         Run a full build first ^(cmake --build build/Release^);
    echo         the plugins are staged automatically as part of the build.
    exit /b 1
)
xcopy /e /i /y "%BUILD_DIR%\plugins" "%REPO%\dist\HutaomuEditor\plugins" >nul
if errorlevel 1 exit /b 1
echo Bundled plugins:
dir /b "%REPO%\dist\HutaomuEditor\plugins"

rem 4) portable zip (unzip and run; includes the plugins)
pushd "%REPO%\dist"
cmake -E tar cf "%REPO%\dist\HutaomuEditor-0.1.0-win64.zip" --format=zip -- HutaomuEditor
if errorlevel 1 (popd & exit /b 1)
popd

rem 5) build the installer
"%ISCC%" "%~dp0installer.iss"
if errorlevel 1 exit /b 1

echo.
echo Done.
echo   installer : %REPO%\dist\HutaomuEditor-Setup-0.1.0.exe
echo   portable  : %REPO%\dist\HutaomuEditor-0.1.0-win64.zip
endlocal
