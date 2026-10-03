"""
Модуль низкоуровневого перехвата горячих клавиш WinScreen (WH_KEYBOARD_LL).
Обеспечивает перехват клавиши PrintScreen и комбинаций ДО того,
как их получат другие программы (Ножницы Windows, Lightshot, ShareX).
"""

import sys
import threading
import time
from config import cfg

# Действия при нажатии горячих клавиш
trigger_fullscreen_callback = None
trigger_live_area_callback = None
trigger_frozen_area_callback = None
trigger_record_callback = None

# VK коды Windows
VK_SNAPSHOT = 0x2C     # PrintScreen
VK_CONTROL = 0x11
VK_SHIFT = 0x10
VK_MENU = 0x12         # Alt

WH_KEYBOARD_LL = 13
WM_KEYDOWN = 0x0100
WM_SYSKEYDOWN = 0x0104

HOOK_THREAD = None
HOOK_HANDLE = None
IS_RUNNING = False


def parse_hotkey_string(hotkey_str):
    """
    Разбирает строку комбинации клавиш (например, 'ctrl+print_screen')
    в набор модификаторов и основной код клавиши.
    """
    parts = [p.strip().lower() for p in hotkey_str.split("+")]
    modifiers = {
        "ctrl": False,
        "shift": False,
        "alt": False
    }
    key_name = ""

    for p in parts:
        if p in ["ctrl", "control"]:
            modifiers["ctrl"] = True
        elif p == "shift":
            modifiers["shift"] = True
        elif p == "alt":
            modifiers["alt"] = True
        else:
            key_name = p

    return modifiers, key_name


def is_modifier_pressed(vk):
    if not sys.platform.startswith("win"):
        return False
    import ctypes
    # GetAsyncKeyState проверяет физическое состояние клавиши
    return bool(ctypes.windll.user32.GetAsyncKeyState(vk) & 0x8000)


def match_hotkey(target_str, pressed_vk, flags=0):
    """
    Проверяет, соответствует ли нажатая клавиша и текущие зажатые модификаторы
    целевой строке комбинации.
    """
    mods, key_name = parse_hotkey_string(target_str)

    # Проверка основной клавиши
    if key_name in ["print_screen", "printscreen", "prtscn", "snapshot"]:
        if pressed_vk != VK_SNAPSHOT:
            return False
    elif key_name.startswith("f") and key_name[1:].isdigit():
        f_num = int(key_name[1:])
        if pressed_vk != (0x70 + (f_num - 1)):
            return False
    elif len(key_name) == 1 and (key_name.isalpha() or key_name.isdigit()):
        if pressed_vk != ord(key_name.upper()):
            return False
    elif key_name == "space":
        if pressed_vk != 0x20:
            return False
    elif key_name == "insert":
        if pressed_vk != 0x2D:
            return False
    elif key_name == "delete":
        if pressed_vk != 0x2E:
            return False
    else:
        if pressed_vk != VK_SNAPSHOT:
            return False

    # Проверяем совпадение зажатых Ctrl, Shift, Alt
    ctrl_active = is_modifier_pressed(VK_CONTROL)
    shift_active = is_modifier_pressed(VK_SHIFT)
    # LLKHF_ALTDOWN (0x20) в flags гарантирует учет нажатого Alt даже если GetAsyncKeyState не обновился
    alt_active = bool(flags & 0x20) or is_modifier_pressed(VK_MENU)

    if mods["ctrl"] != ctrl_active:
        return False
    if mods["shift"] != shift_active:
        return False
    if mods["alt"] != alt_active:
        return False

    return True


def low_level_handler(nCode, wParam, lParam):
    """
    Низкоуровневая процедура обработки клавиатуры (LowLevelKeyboardProc).
    Возврат 1 подавляет событие и не дает другим программам среагировать на клавишу.
    """
    import ctypes
    from ctypes import wintypes

    if nCode >= 0 and (wParam == WM_KEYDOWN or wParam == WM_SYSKEYDOWN):
        # Структура KBDLLHOOKSTRUCT
        # typedef struct tagKBDLLHOOKSTRUCT { DWORD vkCode; DWORD scanCode; DWORD flags; DWORD time; ULONG_PTR dwExtraInfo; }
        vk_code = ctypes.cast(lParam, ctypes.POINTER(wintypes.DWORD))[0]
        flags = ctypes.cast(lParam + 8, ctypes.POINTER(wintypes.DWORD))[0]

        hk_fullscreen = cfg.get("hotkey_fullscreen", "ctrl+print_screen")
        hk_live = cfg.get("hotkey_area_live", "shift+print_screen")
        hk_frozen = cfg.get("hotkey_area_frozen", "print_screen")
        hk_record = cfg.get("hotkey_record_video", "ctrl+shift+print_screen")

        # 1. Проверяем режим видеозаписи
        if match_hotkey(hk_record, vk_code, flags):
            if trigger_record_callback:
                threading.Thread(target=trigger_record_callback, daemon=True).start()
            return 1  # Подавляем клавишу

        # 2. Проверяем режим полного экрана
        if match_hotkey(hk_fullscreen, vk_code, flags):
            if trigger_fullscreen_callback:
                threading.Thread(target=trigger_fullscreen_callback, daemon=True).start()
            return 1  # Подавляем клавишу для всей системы!

        # 3. Проверяем режим живого выделения
        if match_hotkey(hk_live, vk_code, flags):
            if trigger_live_area_callback:
                threading.Thread(target=trigger_live_area_callback, daemon=True).start()
            return 1  # Подавляем клавишу

        # 4. Проверяем режим заморозки экрана
        if match_hotkey(hk_frozen, vk_code, flags):
            if trigger_frozen_area_callback:
                threading.Thread(target=trigger_frozen_area_callback, daemon=True).start()
            return 1  # Подавляем клавишу

    user32 = ctypes.windll.user32
    return user32.CallNextHookEx(HOOK_HANDLE, nCode, wParam, lParam)


def _hook_loop():
    global HOOK_HANDLE, IS_RUNNING
    import ctypes
    from ctypes import wintypes

    user32 = ctypes.windll.user32
    kernel32 = ctypes.windll.kernel32

    HOOKPROC = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_int, wintypes.WPARAM, wintypes.LPARAM)
    pointer_callback = HOOKPROC(low_level_handler)

    h_module = kernel32.GetModuleHandleW(None)
    HOOK_HANDLE = user32.SetWindowsHookExW(
        WH_KEYBOARD_LL,
        pointer_callback,
        h_module,
        0
    )

    if not HOOK_HANDLE:
        print("[WinScreen] Ошибка установки хука клавиатуры!")
        return

    IS_RUNNING = True
    print("[WinScreen] Низкоуровневый перехватчик клавиш активен (приоритет над другими программами включен).")

    # Стандартный цикл обработки сообщений Windows
    msg = wintypes.MSG()
    while IS_RUNNING and user32.GetMessageW(ctypes.byref(msg), None, 0, 0) != 0:
        user32.TranslateMessage(ctypes.byref(msg))
        user32.DispatchMessageW(ctypes.byref(msg))

    if HOOK_HANDLE:
        user32.UnhookWindowsHookEx(HOOK_HANDLE)
        HOOK_HANDLE = None


def start_keyboard_listener(on_fullscreen, on_live, on_frozen, on_record=None):
    """
    Запускает слушатель клавиатуры в фоновом потоке.
    """
    global trigger_fullscreen_callback, trigger_live_area_callback, trigger_frozen_area_callback, trigger_record_callback
    global HOOK_THREAD

    trigger_fullscreen_callback = on_fullscreen
    trigger_live_area_callback = on_live
    trigger_frozen_area_callback = on_frozen
    trigger_record_callback = on_record

    if sys.platform.startswith("win"):
        HOOK_THREAD = threading.Thread(target=_hook_loop, daemon=True)
        HOOK_THREAD.start()
    else:
        print("[WinScreen] Перехватчик клавиш через Windows Hook доступен только на Windows.")


def stop_keyboard_listener():
    global IS_RUNNING, HOOK_HANDLE
    IS_RUNNING = False
    if sys.platform.startswith("win") and HOOK_HANDLE:
        import ctypes
        user32 = ctypes.windll.user32
        user32.PostQuitMessage(0)
        user32.UnhookWindowsHookEx(HOOK_HANDLE)
        HOOK_HANDLE = None
