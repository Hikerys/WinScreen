"""
Модуль графического оверлея WinScreen для выделения области.
Реализует два режима:
1. Живое выделение (Live Area Selection).
2. Замороженное выделение (Frozen Frame Selection).

Строгий минимализм: без лупы, без цифр с габаритами, без тулбаров.
Только прямоугольная рамка, мгновенный захват при отпускании ЛКМ,
отмена по Escape или клику правой кнопкой мыши.
"""

import sys
import time
import tkinter as tk
from PIL import Image, ImageTk, ImageEnhance
from capture import grab_all_screens, process_screenshot
from notifier import notify_cancel, notify_error

try:
    import ctypes
    # Устанавливаем Per-Monitor DPI Awareness для четкого совпадения координат на Windows 10/11
    if sys.platform.startswith("win"):
        try:
            ctypes.windll.shcore.SetProcessDpiAwareness(2)
        except Exception:
            try:
                ctypes.windll.user32.SetProcessDPIAware()
            except Exception:
                pass
except Exception:
    ctypes = None


def get_virtual_screen_geometry():
    """
    Возвращает координаты и размеры виртуального экрана для всех мониторов.
    (x, y, width, height)
    """
    if sys.platform.startswith("win") and ctypes:
        user32 = ctypes.windll.user32
        SM_XVIRTUALSCREEN = 76
        SM_YVIRTUALSCREEN = 77
        SM_CXVIRTUALSCREEN = 78
        SM_CYVIRTUALSCREEN = 79
        x = user32.GetSystemMetrics(SM_XVIRTUALSCREEN)
        y = user32.GetSystemMetrics(SM_YVIRTUALSCREEN)
        w = user32.GetSystemMetrics(SM_CXVIRTUALSCREEN)
        h = user32.GetSystemMetrics(SM_CYVIRTUALSCREEN)
        if w > 0 and h > 0:
            return x, y, w, h
    return 0, 0, 1920, 1080


def find_window_under_point(px, py, ignore_hwnd=0):
    """Находит верхнее видимое окно под координатами (px, py) виртуального экрана."""
    if not (sys.platform.startswith("win") and ctypes):
        return None

    user32 = ctypes.windll.user32
    dwmapi = getattr(ctypes.windll, "dwmapi", None)

    class RECT(ctypes.Structure):
        _fields_ = [
            ("left", ctypes.c_long),
            ("top", ctypes.c_long),
            ("right", ctypes.c_long),
            ("bottom", ctypes.c_long),
        ]

    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    found_rect = None

    def enum_proc(hwnd, lparam):
        nonlocal found_rect
        if hwnd == ignore_hwnd:
            return True
        if hwnd == user32.GetDesktopWindow() or hwnd == user32.GetShellWindow():
            return True
        if not user32.IsWindowVisible(hwnd) or user32.IsIconic(hwnd):
            return True

        ex_style = user32.GetWindowLongW(hwnd, -20)
        if ex_style & 0x00000020:  # WS_EX_TRANSPARENT
            return True

        cls_buf = ctypes.create_unicode_buffer(64)
        if user32.GetClassNameW(hwnd, cls_buf, 64):
            if cls_buf.value in ("Progman", "WorkerW"):
                return True

        if dwmapi and hasattr(dwmapi, "DwmGetWindowAttribute"):
            cloaked = ctypes.c_int(0)
            if dwmapi.DwmGetWindowAttribute(hwnd, 14, ctypes.byref(cloaked), ctypes.sizeof(cloaked)) == 0 and cloaked.value != 0:
                return True

        rc = RECT()
        hr = -1
        if dwmapi and hasattr(dwmapi, "DwmGetWindowAttribute"):
            hr = dwmapi.DwmGetWindowAttribute(hwnd, 9, ctypes.byref(rc), ctypes.sizeof(rc))
        if hr != 0:
            user32.GetWindowRect(hwnd, ctypes.byref(rc))

        if rc.right - rc.left <= 10 or rc.bottom - rc.top <= 10:
            return True

        if rc.left <= px < rc.right and rc.top <= py < rc.bottom:
            found_rect = (rc.left, rc.top, rc.right, rc.bottom)
            return False

        return True

    cb = WNDENUMPROC(enum_proc)
    user32.EnumWindows(cb, 0)
    return found_rect


class AreaSelectionOverlay:
    def __init__(self, mode="live"):
        """
        mode: "live" (живой экран) или "frozen" (стоп-кадр)
        """
        self.mode = mode
        self.start_x = None
        self.start_y = None
        self.current_x = None
        self.current_y = None
        self.rect_id = None
        self.hover_rect_id = None
        self.hovered_rect = None
        self.is_dragging = False
        self.is_captured = False

        self.vx, self.vy, self.vw, self.vh = get_virtual_screen_geometry()

        # В режиме заморозки делаем снимок в эту же миллисекунду
        self.frozen_image = None
        self.tk_dimmed_image = None
        self.tk_bright_image = None

        if self.mode == "frozen":
            try:
                self.frozen_image = grab_all_screens()
                self.vw, self.vh = self.frozen_image.size
            except Exception as e:
                notify_error(f"Не удалось заморозить экран: {e}")
                return

        self.root = tk.Tk()
        self.setup_window()
        self.setup_canvas()
        self.bind_events()

    def setup_window(self):
        self.root.overrideredirect(True)
        self.root.attributes("-topmost", True)
        self.root.geometry(f"{self.vw}x{self.vh}+{self.vx}+{self.vy}")

        if self.mode == "live":
            # Для живого режима: полупрозрачное затемнение
            self.root.attributes("-alpha", 0.3)
            self.root.config(bg="black")
            self.root.config(cursor="crosshair")
        else:
            # Для замороженного: непрозрачное окно с отрисовкой стоп-кадра
            self.root.attributes("-alpha", 1.0)
            self.root.config(cursor="crosshair")

    def setup_canvas(self):
        self.canvas = tk.Canvas(
            self.root,
            width=self.vw,
            height=self.vh,
            cursor="crosshair",
            highlightthickness=0,
            bg="black" if self.mode == "live" else None
        )
        self.canvas.pack(fill="both", expand=True)

        if self.mode == "frozen" and self.frozen_image:
            # Создаем слегка затемненный фон (brightness 0.6)
            enhancer = ImageEnhance.Brightness(self.frozen_image)
            dimmed = enhancer.enhance(0.65)
            self.tk_dimmed_image = ImageTk.PhotoImage(dimmed)
            self.tk_bright_image = ImageTk.PhotoImage(self.frozen_image)

            # Отрисовываем затемненный кадр
            self.canvas.create_image(0, 0, anchor="nw", image=self.tk_dimmed_image)

    def bind_events(self):
        self.canvas.bind("<Motion>", self.on_mouse_move)
        self.canvas.bind("<ButtonPress-1>", self.on_mouse_down)
        self.canvas.bind("<B1-Motion>", self.on_mouse_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_mouse_up)
        # Отмена по Escape или правой кнопке мыши
        self.root.bind("<Escape>", self.on_cancel)
        self.canvas.bind("<Button-3>", self.on_cancel)

    def on_mouse_move(self, event):
        if self.is_dragging or self.start_x is not None:
            return

        screen_x = self.vx + event.x
        screen_y = self.vy + event.y
        found = find_window_under_point(screen_x, screen_y)
        if found:
            x1 = max(0, found[0] - self.vx)
            y1 = max(0, found[1] - self.vy)
            x2 = min(self.vw, found[2] - self.vx)
            y2 = min(self.vh, found[3] - self.vy)
            if x2 > x1 and y2 > y1:
                rect = (x1, y1, x2, y2)
                if rect != self.hovered_rect:
                    self.hovered_rect = rect
                    color = "#00D2FF" if self.mode == "live" else "#00E5FF"
                    if self.hover_rect_id:
                        self.canvas.coords(self.hover_rect_id, x1, y1, x2, y2)
                    else:
                        self.hover_rect_id = self.canvas.create_rectangle(
                            x1, y1, x2, y2, outline=color, width=2
                        )
                return

        if self.hovered_rect is not None:
            self.hovered_rect = None
            if self.hover_rect_id:
                self.canvas.delete(self.hover_rect_id)
                self.hover_rect_id = None

    def on_mouse_down(self, event):
        self.start_x = event.x
        self.start_y = event.y
        self.is_dragging = False

    def on_mouse_drag(self, event):
        if not self.is_dragging:
            if abs(event.x - self.start_x) >= 4 or abs(event.y - self.start_y) >= 4:
                self.is_dragging = True
                if self.hover_rect_id:
                    self.canvas.delete(self.hover_rect_id)
                    self.hover_rect_id = None
                self.hovered_rect = None

                color = "#00D2FF" if self.mode == "live" else "#00E5FF"
                self.rect_id = self.canvas.create_rectangle(
                    self.start_x, self.start_y, self.start_x, self.start_y,
                    outline=color, width=2
                )

        if self.is_dragging and self.rect_id:
            self.current_x = event.x
            self.current_y = event.y
            self.canvas.coords(
                self.rect_id,
                self.start_x, self.start_y,
                self.current_x, self.current_y
            )

    def on_mouse_up(self, event):
        if self.start_x is None or self.start_y is None:
            self.on_cancel()
            return

        x1, y1, x2, y2 = 0, 0, 0, 0
        valid_capture = False

        if self.is_dragging:
            end_x = event.x
            end_y = event.y
            x1 = min(self.start_x, end_x)
            y1 = min(self.start_y, end_y)
            x2 = max(self.start_x, end_x)
            y2 = max(self.start_y, end_y)
            if (x2 - x1 >= 5) and (y2 - y1 >= 5):
                valid_capture = True
        elif self.hovered_rect:
            x1, y1, x2, y2 = self.hovered_rect
            valid_capture = True

        if not valid_capture:
            self.on_cancel()
            return

        if self.mode == "frozen":
            # Вырезаем из уже сохраненного в памяти кадра ДО закрытия окна
            try:
                cropped = self.frozen_image.crop((x1, y1, x2, y2))
                self.is_captured = True
                self.root.destroy()
                process_screenshot(cropped)
            except Exception as e:
                self.is_captured = True
                self.root.destroy()
                notify_error(f"Ошибка кадрирования: {e}")
        else:
            self.is_captured = True
            self.root.destroy()
            # В живом режиме: окно закрыто, захватываем область экрана
            try:
                # Переводим в координаты виртуального экрана
                screen_x1 = self.vx + x1
                screen_y1 = self.vy + y1
                screen_x2 = self.vx + x2
                screen_y2 = self.vy + y2

                # Небольшая пауза для гарантированного скрытия оверлея
                time.sleep(0.05)
                from PIL import ImageGrab
                cropped = ImageGrab.grab(bbox=(screen_x1, screen_y1, screen_x2, screen_y2), all_screens=True)
                process_screenshot(cropped)
            except Exception as e:
                notify_error(f"Ошибка захвата области: {e}")

    def on_cancel(self, event=None):
        if not self.is_captured:
            self.root.destroy()
            notify_cancel()

    def run(self):
        # Принудительно выводим окно на передний план
        self.root.lift()
        self.root.focus_force()
        self.root.mainloop()


def start_live_selection():
    """Запуск режима 2: живое выделение области."""
    app = AreaSelectionOverlay(mode="live")
    app.run()


def start_frozen_selection():
    """Запуск режима 3: стоп-кадр и выделение области из замороженного экрана."""
    app = AreaSelectionOverlay(mode="frozen")
    app.run()
