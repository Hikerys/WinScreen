"""
Модуль системного трея WinScreen.
Отображает иконку в области уведомлений Windows с контекстным меню на русском языке.
"""

import os
import sys
import subprocess
import threading
from PIL import Image, ImageDraw
from config import cfg, get_default_recordings_dir
from autostart import is_autostart_enabled, set_autostart
from capture import capture_fullscreen
from overlay import start_live_selection, start_frozen_selection
from gui import open_settings_window
from recorder import recorder

try:
    import pystray
except ImportError:
    pystray = None


def create_tray_icon_image():
    """
    Генерирует аккуратную иконку скриншотера 64x64 в памяти без необходимости внешних .ico файлов.
    """
    size = (64, 64)
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    # Синий фон со скругленными углами
    draw.rounded_rectangle((4, 4, 60, 60), radius=12, fill="#0078D4")

    # Белая рамка видоискателя / прицела скриншота
    draw.rectangle((16, 16, 48, 48), outline="white", width=3)

    # Угловые метки прицела
    draw.line((12, 16, 22, 16), fill="#00E5FF", width=3)
    draw.line((16, 12, 16, 22), fill="#00E5FF", width=3)

    draw.line((42, 16, 52, 16), fill="#00E5FF", width=3)
    draw.line((48, 12, 48, 22), fill="#00E5FF", width=3)

    draw.line((12, 48, 22, 48), fill="#00E5FF", width=3)
    draw.line((16, 42, 16, 52), fill="#00E5FF", width=3)

    draw.line((42, 48, 52, 48), fill="#00E5FF", width=3)
    draw.line((48, 42, 48, 52), fill="#00E5FF", width=3)

    # Центральная точка
    draw.ellipse((30, 30, 34, 34), fill="white")

    return image


class TrayApp:
    def __init__(self, exit_callback=None):
        self.exit_callback = exit_callback
        self.icon = None

    def open_folder(self, icon=None, item=None):
        save_dir = cfg.get("save_directory")
        os.makedirs(save_dir, exist_ok=True)
        if sys.platform.startswith("win"):
            os.startfile(save_dir)
        else:
            subprocess.run(["xdg-open", save_dir])

    def open_video_folder(self, icon=None, item=None):
        video_dir = cfg.get("video_save_directory", get_default_recordings_dir())
        os.makedirs(video_dir, exist_ok=True)
        if sys.platform.startswith("win"):
            os.startfile(video_dir)
        else:
            subprocess.run(["xdg-open", video_dir])

    def trigger_fullscreen(self, icon=None, item=None):
        threading.Thread(target=capture_fullscreen, daemon=True).start()

    def trigger_live(self, icon=None, item=None):
        threading.Thread(target=start_live_selection, daemon=True).start()

    def trigger_frozen(self, icon=None, item=None):
        threading.Thread(target=start_frozen_selection, daemon=True).start()

    def trigger_toggle_record(self, icon=None, item=None):
        threading.Thread(target=recorder.toggle_recording, daemon=True).start()

    def show_settings(self, icon=None, item=None):
        threading.Thread(target=open_settings_window, daemon=True).start()

    def toggle_autostart(self, icon=None, item=None):
        current = is_autostart_enabled()
        set_autostart(not current)

    def on_recording_state_change(self, is_recording):
        if self.icon:
            self.icon.title = "WinScreen — Идёт запись видео..." if is_recording else "WinScreen — Система скриншотов"
            try:
                self.icon.update_menu()
            except Exception:
                pass

    def on_exit(self, icon=None, item=None):
        if recorder.is_recording():
            recorder.stop_recording()
        if self.icon:
            self.icon.stop()
        if self.exit_callback:
            self.exit_callback()
        sys.exit(0)

    def run(self):
        if pystray is None:
            print("[WinScreen] Библиотека pystray не установлена. Трей отключен, горячие клавиши продолжают работать.")
            return

        image = create_tray_icon_image()
        recorder.add_state_listener(self.on_recording_state_change)

        menu = pystray.Menu(
            pystray.MenuItem("Сделать скриншот", pystray.Menu(
                pystray.MenuItem("Полный экран", self.trigger_fullscreen),
                pystray.MenuItem("Выделение области (живое)", self.trigger_live),
                pystray.MenuItem("Заморозка экрана", self.trigger_frozen),
            )),
            pystray.MenuItem(
                lambda item: "⏹ Остановить запись видео" if recorder.is_recording() else "🎥 Начать запись видео",
                self.trigger_toggle_record
            ),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem("Открыть папку со скриншотами", self.open_folder),
            pystray.MenuItem("Открыть папку с видеозаписями", self.open_video_folder),
            pystray.MenuItem("Настройки...", self.show_settings, default=True),
            pystray.MenuItem("Автозагрузка с Windows", self.toggle_autostart, checked=lambda item: is_autostart_enabled()),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem("Выход", self.on_exit)
        )

        self.icon = pystray.Icon("WinScreen", image, "WinScreen — Система скриншотов", menu)
        self.icon.run()


def start_tray(exit_callback=None):
    tray = TrayApp(exit_callback=exit_callback)
    tray.run()
