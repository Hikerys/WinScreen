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
        self.canvas.bind("<ButtonPress-1>", self.on_mouse_down)
        self.canvas.bind("<B1-Motion>", self.on_mouse_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_mouse_up)
        # Отмена по Escape или правой кнопке мыши
        self.root.bind("<Escape>", self.on_cancel)
        self.canvas.bind("<Button-3>", self.on_cancel)

    def on_mouse_down(self, event):
        self.start_x = event.x
        self.start_y = event.y
        if self.mode == "live":
            self.rect_id = self.canvas.create_rectangle(
                self.start_x, self.start_y, self.start_x, self.start_y,
                outline="#00D2FF", width=2
            )
        else:
            # В замороженном режиме рамка с яркой окантовкой
            self.rect_id = self.canvas.create_rectangle(
                self.start_x, self.start_y, self.start_x, self.start_y,
                outline="#00E5FF", width=2
            )

    def on_mouse_drag(self, event):
        self.current_x = event.x
        self.current_y = event.y
        if self.rect_id:
            self.canvas.coords(
                self.rect_id,
                self.start_x, self.start_y,
                self.current_x, self.current_y
            )

    def on_mouse_up(self, event):
        if self.start_x is None or self.start_y is None:
            self.on_cancel()
            return

        end_x = event.x
        end_y = event.y

        # Рассчитываем координаты прямоугольника
        x1 = min(self.start_x, end_x)
        y1 = min(self.start_y, end_y)
        x2 = max(self.start_x, end_x)
        y2 = max(self.start_y, end_y)

        width = x2 - x1
        height = y2 - y1

        # Если область слишком маленькая (случайный клик без протягивания), отменяем
        if width < 5 or height < 5:
            self.on_cancel()
            return

        self.is_captured = True
        self.root.destroy()

        if self.mode == "frozen":
            # Вырезаем из уже сохраненного в памяти кадра
            try:
                cropped = self.frozen_image.crop((x1, y1, x2, y2))
                process_screenshot(cropped)
            except Exception as e:
                notify_error(f"Ошибка кадрирования: {e}")
        else:
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
