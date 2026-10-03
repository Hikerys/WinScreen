@echo off
chcp 65001 > nul
echo =======================================================
echo     WinScreen — Компиляция C++ WinScreen.exe (MSVC)
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

echo Компиляция cpp\winscreen.cpp...
cl /O2 /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo"build\\" /Fd"build\\" cpp\winscreen.cpp /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib gdiplus.lib shell32.lib advapi32.lib shcore.lib comctl32.lib ole32.lib oleaut32.lib urlmon.lib /OUT:bin\WinScreen.exe

if %errorlevel% equ 0 (
    echo.
    echo =======================================================
    echo Готово! WinScreen.exe успешно скомпилирован в bin\WinScreen.exe
    echo =======================================================
) else (
    echo.
    echo [ОШИБКА] Во время компиляции произошла ошибка.
)
pause
