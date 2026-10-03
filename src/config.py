"""
Модуль конфигурации WinScreen.
Управляет загрузкой, сохранением и путями по умолчанию.
"""

import os
import json
import sys

DEFAULT_CONFIG_DIR_NAME = "WinScreen"

def get_app_dir():
    """Возвращает директорию хранения конфигурации и данных приложения (%APPDATA%\\WinScreen)."""
    appdata = os.environ.get("APPDATA")
    if not appdata:
        appdata = os.path.expanduser("~/.config")
    target_dir = os.path.join(appdata, DEFAULT_CONFIG_DIR_NAME)
    try:
        os.makedirs(target_dir, exist_ok=True)
    except Exception:
        pass
    return target_dir

def get_default_screenshots_dir():
    """Возвращает стандартную папку 'Скриншоты' в 'Изображения' пользователя."""
    user_profile = os.environ.get("USERPROFILE")
    if user_profile:
        pictures = os.path.join(user_profile, "Pictures")
    else:
        pictures = os.path.expanduser("~/Pictures")
    screenshots_dir = os.path.join(pictures, "Скриншоты")
    return screenshots_dir

def get_default_recordings_dir():
    """Возвращает стандартную папку 'Записи WinScreen' в 'Видео' пользователя."""
    user_profile = os.environ.get("USERPROFILE")
    if user_profile:
        videos = os.path.join(user_profile, "Videos")
    else:
        videos = os.path.expanduser("~/Videos")
    recordings_dir = os.path.join(videos, "Записи WinScreen")
    return recordings_dir

DEFAULT_CONFIG = {
    "save_directory": get_default_screenshots_dir(),
    "video_save_directory": get_default_recordings_dir(),
    "file_format": "png",  # "png" или "jpg"
    "jpg_quality": 90,     # от 1 до 100 (по умолчанию 90%)
    "copy_to_clipboard": True,
    "save_to_disk": True,
    "show_notifications": True,
    "hotkey_fullscreen": "ctrl+print_screen",
    "hotkey_area_live": "shift+print_screen",
    "hotkey_area_frozen": "print_screen",
    "hotkey_record_video": "ctrl+shift+print_screen",
    "video_fps": 30,
    "record_system_audio": False,
    "record_microphone": False,
    "selected_microphone": ""
}

class ConfigManager:
    def __init__(self):
        self.app_dir = get_app_dir()
        self.config_path = os.path.join(self.app_dir, "config.json")
        self.config = self.load_config()

    def load_config(self):
        config = DEFAULT_CONFIG.copy()
        if os.path.exists(self.config_path):
            try:
                with open(self.config_path, "r", encoding="utf-8") as f:
                    loaded = json.load(f)
                    config.update(loaded)
            except Exception as e:
                print(f"[WinScreen] Ошибка загрузки config.json: {e}. Используются настройки по умолчанию.")
        
        # Гарантируем существование папок для скриншотов и видеозаписей
        for dir_key in ["save_directory", "video_save_directory"]:
            target_path = config.get(dir_key)
            if target_path:
                try:
                    os.makedirs(target_path, exist_ok=True)
                except Exception:
                    pass

        return config

    def save_config(self):
        try:
            with open(self.config_path, "w", encoding="utf-8") as f:
                json.dump(self.config, f, indent=4, ensure_ascii=False)
            return True
        except Exception as e:
            print(f"[WinScreen] Ошибка сохранения config.json: {e}")
            return False

    def get(self, key, default=None):
        return self.config.get(key, default)

    def set(self, key, value):
        self.config[key] = value
        self.save_config()

# Глобальный экземпляр настроек
cfg = ConfigManager()
