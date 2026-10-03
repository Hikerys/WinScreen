@echo off
chcp 65001 > nul
echo =======================================================
echo          WinScreen — Установка для Windows 10/11
echo =======================================================
echo.

cd /d "%~dp0\.."

python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ОШИБКА] Python 3 не обнаружен в системе!
    echo Пожалуйста, установите Python с официального сайта python.org
    echo и обязательно отметьте галочку "Add Python to PATH".
    echo.
    pause
    exit /b 1
)

echo [1/3] Проверка и установка необходимых библиотек...
pip install -r requirements.txt
if %errorlevel% neq 0 (
    echo [ПРЕДУПРЕЖДЕНИЕ] Не удалось обновить некоторые пакеты через pip.
)

echo [2/3] Автоматическая установка и добавление в автозагрузку...
python main.py --install

echo [3/3] Запуск программы в фоновом режиме...
start "" pythonw main.py

echo.
echo =======================================================
echo     WinScreen успешно установлен и запущен!
echo     Иконка программы появилась в системном трее (возле часов).
echo     Теперь WinScreen будет запускаться вместе с Windows.
echo =======================================================
echo.
pause
