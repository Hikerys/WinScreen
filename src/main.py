"""
Главная точка входа WinScreen.
Объединяет автоматическую установку, CLI-команды, перехват горячих клавиш и системный трей.
"""

import sys
import time
import threading
from cli import handle_cli_args
from autostart import auto_install_if_needed


def check_requirements():
    """Проверяет наличие необходимых библиотек перед запуском интерфейса."""
    missing = []
    try:
        import PIL
    except ImportError:
        missing.append("Pillow")
    try:
        import pystray
    except ImportError:
        missing.append("pystray")

    if missing:
        print("[WinScreen] Внимание! Не установлены следующие библиотеки:")
        for pkg in missing:
            print(f"  - {pkg}")
        print("Установите их командой: pip install -r requirements.txt")
        print("Или запустите install.bat для автоматической настройки.")
        return False
    return True


def main():
    # 1. Обработка аргументов командной строки (CLI)
    if len(sys.argv) > 1:
        should_exit = handle_cli_args()
        if should_exit:
            sys.exit(0)

    # 2. Автоматическая установка при первом запуске:
    # Копирует в %APPDATA%\WinScreen, включает автозагрузку и отключает конфликтные Ножницы
    auto_install_if_needed()

    if not check_requirements():
        sys.exit(1)

    from hotkeys import start_keyboard_listener, stop_keyboard_listener
    from capture import capture_fullscreen
    from overlay import start_live_selection, start_frozen_selection
    from tray import start_tray
    from recorder import recorder

    def on_fullscreen_hotkey():
        capture_fullscreen()

    def on_live_area_hotkey():
        start_live_selection()

    def on_frozen_area_hotkey():
        start_frozen_selection()

    def on_record_hotkey():
        recorder.toggle_recording()

    def cleanup():
        print("[WinScreen] Завершение работы...")
        if recorder.is_recording():
            recorder.stop_recording()
        stop_keyboard_listener()

    print("[WinScreen] Запуск фоновой службы скриншотов и видеозаписи...")

    # 3. Запуск низкоуровневого перехвата клавиш (WH_KEYBOARD_LL)
    start_keyboard_listener(
        on_fullscreen=on_fullscreen_hotkey,
        on_live=on_live_area_hotkey,
        on_frozen=on_frozen_area_hotkey,
        on_record=on_record_hotkey
    )

    # 4. Запуск иконки в системном трее (блокирующий вызов основного потока)
    try:
        start_tray(exit_callback=cleanup)
    except KeyboardInterrupt:
        cleanup()
    except Exception as e:
        print(f"[WinScreen] Предупреждение трея: {e}. Работаем в скрытом фоновом режиме.")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            cleanup()


if __name__ == "__main__":
    main()
