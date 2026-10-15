@echo off
chcp 65001 > nul
echo =======================================================
echo     WinScreen — Компиляция C++ (MSVC x64 / ARM64)
echo =======================================================
echo.

cd /d "%~dp0\.."

where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo [ВНИМАНИЕ] Компилятор MSVC (cl.exe) не найден в текущем PATH.
    echo Запустите этот файл из консоли "Developer Command Prompt for VS"
    echo или выполните сборку через scripts\build_exe.bat (PyInstaller).
    echo.
    pause
    exit /b 1
)

if not exist "build" mkdir "build"
if not exist "bin" mkdir "bin"

set TARGET_EXE=bin\WinScreen.exe
set RC_FLAGS=
set CL_EXTRA=

if /i "%VSCMD_ARG_TGT_ARCH%"=="arm64" (
    echo Выбрана целевая архитектура: ARM64
    set TARGET_EXE=bin\WinScreen-ARM.exe
    set RC_FLAGS=/d WINSCREEN_ARM64
    set CL_EXTRA=/DWINSCREEN_ARM64
) else (
    echo Выбрана целевая архитектура: x86_64
)

echo Компиляция ресурсов cpp\res\winscreen.rc...
rc %RC_FLAGS% /fo "build\winscreen.res" cpp\res\winscreen.rc

echo Компиляция cpp\winscreen.cpp -> %TARGET_EXE%...
cl /O2 /EHsc /utf-8 /DUNICODE /D_UNICODE %CL_EXTRA% /Fo"build\\" /Fd"build\\" cpp\winscreen.cpp build\winscreen.res /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib gdiplus.lib shell32.lib advapi32.lib shcore.lib comctl32.lib ole32.lib oleaut32.lib urlmon.lib msimg32.lib dwmapi.lib wininet.lib version.lib /OUT:%TARGET_EXE%

if %errorlevel% equ 0 (
    echo.
    echo =======================================================
    echo Готово! Успешно скомпилировано в %TARGET_EXE%
    echo =======================================================
) else (
    echo.
    echo [ОШИБКА] Во время компиляции произошла ошибка.
)
pause
