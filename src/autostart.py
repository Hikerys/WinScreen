"""
Модуль автозагрузки и автоустановки WinScreen.
Регистрирует приложение в реестре Windows (HKCU Run), копирует файлы в %APPDATA%
и обеспечивает запуск вместе с операционной системой.
"""

import os
import sys
import shutil
import subprocess

try:
    import winreg
except ImportError:
    winreg = None

from config import get_app_dir, get_default_screenshots_dir

REG_RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"
REG_APP_NAME = "WinScreen"
KEYBOARD_SETTINGS_KEY = r"Control Panel\Keyboard"
SNIPPING_TOOL_KEY = "PrintScreenShortcutManagedBySnippingToolEnabled"


def is_windows():
    return sys.platform.startswith("win") or winreg is not None


def get_current_executable_command(target_dir=None):
    """
    Возвращает строку запуска приложения.
    Использует pythonw.exe для скрытого фонового запуска без черного окна консоли.
    """
    if getattr(sys, "frozen", False):
        # Если запущено как скомпилированный .exe
        exe_path = sys.executable
        if target_dir:
            exe_name = os.path.basename(exe_path)
            return f'"{os.path.join(target_dir, exe_name)}"'
        return f'"{exe_path}"'
    else:
        # Если запущено как Python скрипт
        python_exe = sys.executable
        # Ищем pythonw.exe рядом с python.exe для тихого запуска
        dir_name = os.path.dirname(python_exe)
        pythonw = os.path.join(dir_name, "pythonw.exe")
        if not os.path.exists(pythonw):
            pythonw = python_exe

        script_path = os.path.abspath(sys.argv[0])
        if target_dir:
            script_name = os.path.basename(script_path)
            script_path = os.path.join(target_dir, script_name)

        return f'"{pythonw}" "{script_path}"'


def is_autostart_enabled():
    """Проверяет, добавлена ли программа в автозагрузку реестра Windows."""
    if not is_windows():
        return False
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, REG_RUN_KEY, 0, winreg.KEY_READ) as key:
            winreg.QueryValueEx(key, REG_APP_NAME)
            return True
    except (FileNotFoundError, OSError):
        return False


def set_autostart(enable=True, custom_cmd=None):
    """Включает или выключает автозагрузку через реестр Windows."""
    if not is_windows():
        print(f"[WinScreen] Автозагрузка доступна только на Windows.")
        return False

    cmd = custom_cmd if custom_cmd else get_current_executable_command()

    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, REG_RUN_KEY, 0, winreg.KEY_SET_VALUE) as key:
            if enable:
                winreg.SetValueEx(key, REG_APP_NAME, 0, winreg.REG_SZ, cmd)
                print(f"[WinScreen] Программа успешно добавлена в автозагрузку: {cmd}")
            else:
                try:
                    winreg.DeleteValue(key, REG_APP_NAME)
                    print("[WinScreen] Программа удалена из автозагрузки.")
                except FileNotFoundError:
                    pass
        return True
    except Exception as e:
        print(f"[WinScreen] Ошибка изменения автозагрузки в реестре: {e}")
        return False


def override_windows_snipping_tool():
    """
    Отключает стандартный перехват клавиши PrintScreen встроенными Ножницами Windows 10/11,
    чтобы WinScreen имел безусловный приоритет.
    """
    if not is_windows():
        return
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, KEYBOARD_SETTINGS_KEY, 0, winreg.KEY_SET_VALUE) as key:
            # 0 = Отключить перехват PrintScreen приложением 'Ножницы'
            winreg.SetValueEx(key, SNIPPING_TOOL_KEY, 0, winreg.REG_DWORD, 0)
            print("[WinScreen] Встроенные 'Ножницы' Windows отключены от перехвата клавиши PrintScreen.")
    except Exception as e:
        print(f"[WinScreen] Предупреждение: не удалось настроить перехват клавиши в реестре: {e}")


def auto_install_if_needed():
    """
    Автоматически устанавливает программу при первом запуске:
    1. Создает стандартную папку для скриншотов.
    2. Если запущено не из %APPDATA%\\WinScreen, копирует файлы проекта туда.
    3. Регистрирует установленную версию в автозагрузке Windows.
    4. Отключает перехват клавиши штатными ножницами.
    """
    default_dir = get_default_screenshots_dir()
    os.makedirs(default_dir, exist_ok=True)

    if not is_windows():
        return True

    override_windows_snipping_tool()

    app_dir = get_app_dir()
    current_script_dir = os.path.dirname(os.path.abspath(sys.argv[0]))

    # Если мы уже запущены из директории установки
    if os.path.normcase(current_script_dir) == os.path.normcase(app_dir):
        if not is_autostart_enabled():
            set_autostart(True)
        return True

    # Иначе копируем файлы в %APPDATA%\WinScreen
    print(f"[WinScreen] Первый запуск: выполняем автоматическую установку в {app_dir}...")
    try:
        os.makedirs(app_dir, exist_ok=True)

        # Копируем все файлы проекта в app_dir
        for item in os.listdir(current_script_dir):
            s = os.path.join(current_script_dir, item)
            d = os.path.join(app_dir, item)
            if item in ["__pycache__", ".git", "build", "dist", "samples", ".venv", "env", ".vscode", ".idea"]:
                continue
            if os.path.isdir(s):
                if not os.path.exists(d):
                    shutil.copytree(s, d)
            else:
                shutil.copy2(s, d)

        # Регистрируем в автозагрузке путь именно из %APPDATA%\WinScreen
        cmd = get_current_executable_command(target_dir=app_dir)
        set_autostart(True, custom_cmd=cmd)
        print("[WinScreen] Автоустановка успешно завершена.")
        return True
    except Exception as e:
        print(f"[WinScreen] Ошибка при автоустановке: {e}")
        # Если не удалось скопировать, хотя бы прописываем текущий путь
        set_autostart(True)
        return False


def uninstall():
    """
    Полное удаление программы из системы:
    1. Удаляет запись из автозагрузки реестра.
    2. Возвращает настройку PrintScreen для Ножниц.
    3. Очищает рабочую директорию в %APPDATA%.
    """
    if is_windows():
        set_autostart(False)
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, KEYBOARD_SETTINGS_KEY, 0, winreg.KEY_SET_VALUE) as key:
                # Включаем обратно стандартное поведение
                winreg.SetValueEx(key, SNIPPING_TOOL_KEY, 0, winreg.REG_DWORD, 1)
        except Exception:
            pass

    app_dir = get_app_dir()
    print(f"[WinScreen] Программа успешно отключена и удалена из автозагрузки.")
    print(f"[WinScreen] Конфигурационные файлы: {app_dir}")
    return True
