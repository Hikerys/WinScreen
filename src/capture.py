"""
Модуль захвата экрана и работы с буфером обмена WinScreen.
Поддерживает мультимониторные конфигурации, копирование в буфер (CF_DIB)
и сохранение в файлы (PNG/JPG) с настраиваемым качеством.
"""

import os
import sys
import io
import time
from datetime import datetime
from PIL import Image, ImageGrab

try:
    import ctypes
    from ctypes import wintypes
except ImportError:
    ctypes = None

from config import cfg
from notifier import notify_success, notify_error

# Windows API константы для буфера обмена
CF_DIB = 8
GMEM_MOVEABLE = 0x0002


def copy_image_to_clipboard(image: Image.Image):
    """
    Копирует PIL изображение в буфер обмена Windows в формате CF_DIB (Device Independent Bitmap).
    Это обеспечивает 100% совместимость со всеми приложениями (Telegram, Discord, Браузеры, Word и т.д.).
    """
    if not sys.platform.startswith("win") or ctypes is None:
        return False

    try:
        # Конвертируем изображение в RGB перед записью в BMP
        if image.mode != "RGB":
            image = image.convert("RGB")

        output = io.BytesIO()
        image.save(output, "BMP")
        data = output.getvalue()
        output.close()

        # Для CF_DIB требуется отрезать 14 байт заголовка файла BMP (BITMAPFILEHEADER)
        dib_data = data[14:]

        user32 = ctypes.windll.user32
        kernel32 = ctypes.windll.kernel32

        user32.OpenClipboard.argtypes = [wintypes.HWND]
        user32.OpenClipboard.restype = wintypes.BOOL
        user32.EmptyClipboard.restype = wintypes.BOOL
        user32.SetClipboardData.argtypes = [wintypes.UINT, wintypes.HANDLE]
        user32.SetClipboardData.restype = wintypes.HANDLE
        user32.CloseClipboard.restype = wintypes.BOOL

        kernel32.GlobalAlloc.argtypes = [wintypes.UINT, ctypes.c_size_t]
        kernel32.GlobalAlloc.restype = wintypes.HGLOBAL
        kernel32.GlobalLock.argtypes = [wintypes.HGLOBAL]
        kernel32.GlobalLock.restype = wintypes.LPVOID
        kernel32.GlobalUnlock.argtypes = [wintypes.HGLOBAL]
        kernel32.GlobalUnlock.restype = wintypes.BOOL

        # Ждем освобождения буфера другими программами
        attempts = 5
        opened = False
        while attempts > 0:
            if user32.OpenClipboard(None):
                opened = True
                break
            time.sleep(0.05)
            attempts -= 1

        if not opened:
            return False

        try:
            user32.EmptyClipboard()
            h_global = kernel32.GlobalAlloc(GMEM_MOVEABLE, len(dib_data))
            if not h_global:
                return False

            ptr = kernel32.GlobalLock(h_global)
            if ptr:
                ctypes.memmove(ptr, dib_data, len(dib_data))
                kernel32.GlobalUnlock(h_global)
                user32.SetClipboardData(CF_DIB, h_global)
                return True
        finally:
            user32.CloseClipboard()

    except Exception as e:
        print(f"[WinScreen] Ошибка копирования в буфер обмена: {e}")
        return False


def save_image_to_disk(image: Image.Image) -> str:
    """
    Сохраняет изображение в настроенную папку с выбранным форматом и качеством.
    Возвращает полный путь к созданному файлу.
    """
    save_dir = cfg.get("save_directory")
    os.makedirs(save_dir, exist_ok=True)

    fmt = cfg.get("file_format", "png").lower()
    quality = max(1, min(100, int(cfg.get("jpg_quality", 90))))

    now = datetime.now()
    timestamp = now.strftime("%Y-%m-%d_%H-%M-%S")
    extension = "jpg" if fmt in ["jpg", "jpeg"] else "png"
    filename = f"Скриншот_{timestamp}.{extension}"
    file_path = os.path.join(save_dir, filename)

    # Защита от перезаписи при совпадении секунд
    counter = 1
    while os.path.exists(file_path):
        filename = f"Скриншот_{timestamp}_{counter}.{extension}"
        file_path = os.path.join(save_dir, filename)
        counter += 1

    try:
        if extension == "jpg":
            if image.mode in ("RGBA", "P"):
                image = image.convert("RGB")
            image.save(file_path, "JPEG", quality=quality)
        else:
            image.save(file_path, "PNG", optimize=False)
        return file_path
    except Exception as e:
        print(f"[WinScreen] Ошибка сохранения файла на диск: {e}")
        raise


def process_screenshot(image: Image.Image):
    """
    Единая обработка полученного скриншота:
    1. Запись в буфер обмена (если включено).
    2. Сохранение на диск (если включено).
    3. Отправка уведомления об успешном создании.
    """
    saved_file_path = None
    try:
        if cfg.get("copy_to_clipboard", True):
            copy_image_to_clipboard(image)

        if cfg.get("save_to_disk", True):
            saved_file_path = save_image_to_disk(image)

        notify_success(saved_file_path)
    except Exception as e:
        notify_error(f"Не удалось сохранить скриншот: {e}")


def grab_all_screens() -> Image.Image:
    """
    Захватывает полный экран со всех подключенных мониторов.
    """
    try:
        # На Windows параметр all_screens=True захватывает виртуальный экран целиком
        return ImageGrab.grab(all_screens=True)
    except TypeError:
        # Резервный вызов для старых версий PIL
        return ImageGrab.grab()


def capture_fullscreen():
    """
    Режим 1: Мгновенный захват полного экрана.
    """
    try:
        img = grab_all_screens()
        process_screenshot(img)
    except Exception as e:
        notify_error(f"Ошибка захвата экрана: {e}")
