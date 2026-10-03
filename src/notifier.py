"""
Модуль уведомлений WinScreen.
Отображает нативные всплывающие тост-уведомления Windows 10/11 на русском языке
в неблокирующем фоновом потоке.
"""

import sys
import os
import threading
import subprocess
from config import cfg


def _show_windows_toast_powershell(title, message):
    """
    Отправляет нативное уведомление Windows 10/11 через PowerShell (Windows.UI.Notifications).
    Не требует сторонних зависимостей.
    """
    # Экранируем кавычки
    safe_title = title.replace('"', '`"')
    safe_msg = message.replace('"', '`"')

    ps_script = f"""
    [Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime] > $null
    $template = [Windows.UI.Notifications.ToastNotificationManager]::GetTemplateContent([Windows.UI.Notifications.ToastTemplateType]::ToastText02)
    $textNodes = $template.GetElementsByTagName("text")
    $textNodes.Item(0).AppendChild($template.CreateTextNode("{safe_title}")) > $null
    $textNodes.Item(1).AppendChild($template.CreateTextNode("{safe_msg}")) > $null
    $toast = [Windows.UI.Notifications.ToastNotification]::new($template)
    $notifier = [Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier("WinScreen")
    $notifier.Show($toast)
    """

    try:
        # Запускаем PowerShell скрытно
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startupinfo.wShowWindow = 0  # SW_HIDE
        subprocess.run(
            ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps_script],
            startupinfo=startupinfo,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform.startswith("win") else 0,
            timeout=5
        )
    except Exception:
        # Резервный вариант, если WinRT недоступен
        pass


def notify(title, message):
    """
    Отправляет уведомление пользователю, если уведомления включены в настройках.
    Выполняется в фоновом потоке, чтобы не блокировать процесс создания скриншотов.
    """
    if not cfg.get("show_notifications", True):
        return

    print(f"[WinScreen] [{title}] {message}")

    if sys.platform.startswith("win"):
        t = threading.Thread(target=_show_windows_toast_powershell, args=(title, message), daemon=True)
        t.start()


def notify_success(file_path=None):
    """Уведомление при успешном создании скриншота."""
    title = "Скриншот готов"
    if file_path and os.path.exists(file_path):
        msg = f"Скопирован в буфер и сохранён в файл:\n{os.path.basename(file_path)}"
    else:
        msg = "Скопирован в буфер обмена и сохранён"
    notify(title, msg)


def notify_cancel():
    """Уведомление при отмене выделения области."""
    notify("Снимок отменён", "Выделение области прервано")


def notify_error(details="Произошла непредвиденная ошибка"):
    """Уведомление при сбое создания скриншота."""
    notify("Ошибка создания снимка", str(details))


def notify_recording_started(hotkey_hint=None):
    """Уведомление о старте видеозаписи."""
    title = "Запись видео начата"
    if hotkey_hint:
        msg = f"Идет запись экрана...\nНажмите {hotkey_hint} для остановки"
    else:
        msg = "Идет запись экрана...\nНажмите горячую клавишу для остановки"
    notify(title, msg)


def notify_recording_stopped(file_path=None):
    """Уведомление о завершении видеозаписи."""
    title = "Видеозапись сохранена"
    if file_path and os.path.exists(file_path):
        msg = f"Файл сохранён в папку записей:\n{os.path.basename(file_path)}"
    else:
        msg = "Видео успешно сохранено в папку записей"
    notify(title, msg)
