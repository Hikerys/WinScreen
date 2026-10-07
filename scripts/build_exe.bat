@echo off
chcp 65001 > nul
echo =======================================================
echo          WinScreen — Сборка автономного .EXE
echo =======================================================
echo.

cd /d "%~dp0\.."

pip install pyinstaller
echo Сборка исполняемого файла WinScreen.exe...
pyinstaller --noconsole --onefile --paths=src --distpath bin --workpath build --icon=cpp/res/WinScreen.ico --name WinScreen main.py

echo.
echo =======================================================
echo Готово! Скомпилированный файл находится в bin\WinScreen.exe
echo =======================================================
pause
