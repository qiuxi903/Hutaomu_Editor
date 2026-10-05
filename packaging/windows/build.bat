@echo off
rem Hutaomu Editor - Windows build script (MinGW toolchain installed via aqtinstall)
rem Layout assumed: Qt at G:\Qt\6.10.3\mingw_64, MinGW at G:\Qt\Tools\mingw1310_64,
rem cmake/ninja from pip (pip install aqtinstall cmake ninja).
rem NOTE: this script lives at <repo>\packaging\windows\, so the repo root is %~dp0..\..
rem Messages are ASCII on purpose: cmd parses .bat in the OEM codepage and would
rem mangle non-ASCII echo text.

setlocal
set REPO=%~dp0..\..
set QT_DIR=G:\Qt\6.10.3\mingw_64
set MINGW_DIR=G:\Qt\Tools\mingw1310_64
set PIP_SCRIPTS=C:\Users\%USERNAME%\AppData\Roaming\Python\Python314\Scripts
set PATH=%MINGW_DIR%\bin;%QT_DIR%\bin;%PIP_SCRIPTS%;%PATH%

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

cmake -S "%REPO%" -B "%REPO%\build\%BUILD_TYPE%" -G Ninja ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_PREFIX_PATH=%QT_DIR%
if errorlevel 1 exit /b 1

cmake --build "%REPO%\build\%BUILD_TYPE%"
if errorlevel 1 exit /b 1

echo.
echo Build OK: %REPO%\build\%BUILD_TYPE%\HutaomuEditor.exe
echo Run with Qt DLLs on PATH:
echo     set PATH=%MINGW_DIR%\bin;%QT_DIR%\bin;%%PATH%%
echo     %REPO%\build\%BUILD_TYPE%\HutaomuEditor.exe
endlocal
