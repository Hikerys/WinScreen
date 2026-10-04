"""
Модуль графического интерфейса настроек WinScreen (GUI).
Реализован на Tkinter, полностью на русском языке,
в современном минималистичном стиле Windows 10/11.
"""

import os
import sys
import json
import urllib.request
import webbrowser
import threading
import subprocess
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from config import cfg, get_default_recordings_dir
from autostart import is_autostart_enabled, set_autostart, uninstall
from recorder import find_ffmpeg_executable, install_ffmpeg, get_audio_devices

APP_VERSION = "1.1.0"
GITHUB_REPO_URL = "https://github.com/Hikerys/WinScreen"
GITHUB_API_LATEST_RELEASE = "https://api.github.com/repos/Hikerys/WinScreen/releases/latest"


def parse_version(v_str):
    s = v_str.lstrip("vV ").strip()
    parts = []
    for part in s.split("."):
        digits = ""
        for ch in part:
            if ch.isdigit():
                digits += ch
            else:
                break
        parts.append(int(digits) if digits else 0)
    while len(parts) < 3:
        parts.append(0)
    return tuple(parts[:3])


def is_newer_version(remote_str, current_str):
    return parse_version(remote_str) > parse_version(current_str)


def check_for_updates():
    try:
        req = urllib.request.Request(
            GITHUB_API_LATEST_RELEASE,
            headers={"User-Agent": "WinScreen-UpdateChecker/1.1.0"}
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            tag = data.get("tag_name", "")
            html_url = data.get("html_url", GITHUB_REPO_URL + "/releases/latest")
            download_url = ""
            for asset in data.get("assets", []):
                if asset.get("name", "").endswith(".exe"):
                    download_url = asset.get("browser_download_url", "")
                    break
            if not download_url:
                download_url = f"{GITHUB_REPO_URL}/releases/download/{tag}/WinScreen.exe"
            if tag and is_newer_version(tag, APP_VERSION):
                return {
                    "has_update": True,
                    "latest_version": tag,
                    "release_url": html_url,
                    "download_url": download_url
                }
    except Exception:
        pass
    return None


class SettingsWindow:
    def __init__(self, on_close_callback=None):
        self.on_close_callback = on_close_callback
        self.root = tk.Tk()
        self.root.title("WinScreen — Настройки")
        self.root.geometry("540x850")
        self.root.resizable(False, False)

        # Центрирование окна на экране
        self.center_window()

        # Цветовая палитра
        self.bg_color = "#F3F3F3"
        self.card_bg = "#FFFFFF"
        self.primary_color = "#0078D4"
        self.text_color = "#202020"

        self.root.configure(bg=self.bg_color)

        self.recording_target = None
        self.pressed_mods = set()
        self.hk_vars = {}
        self.hk_btns = {}
        self.prev_hk_values = {}
        self.detected_devices = {"microphones": [], "system_audio": None}
        self.update_info = None
        self.setup_ui()
        self.load_values()
        threading.Thread(target=self.check_updates_background, daemon=True).start()

    def center_window(self):
        self.root.update_idletasks()
        w = 540
        h = 850
        sw = self.root.winfo_screenwidth()
        sh = self.root.winfo_screenheight()
        x = (sw - w) // 2
        y = (sh - h) // 2
        self.root.geometry(f"{w}x{h}+{x}+{y}")

    def setup_ui(self):
        # Заголовок
        header_frame = tk.Frame(self.root, bg=self.bg_color)
        header_frame.pack(fill="x", padx=20, pady=(15, 10))

        title_lbl = tk.Label(
            header_frame,
            text="WinScreen",
            font=("Segoe UI", 16, "bold"),
            fg=self.primary_color,
            bg=self.bg_color
        )
        title_lbl.pack(anchor="w")

        subtitle_lbl = tk.Label(
            header_frame,
            text="Настройки системы скриншотов",
            font=("Segoe UI", 9),
            fg="#606060",
            bg=self.bg_color
        )
        subtitle_lbl.pack(anchor="w")

        # 1. Секция: Папка для сохранения
        dir_frame = tk.LabelFrame(
            self.root,
            text=" Папка для скриншотов ",
            font=("Segoe UI", 10, "bold"),
            bg=self.card_bg,
            fg=self.text_color,
            padx=12,
            pady=10
        )
        dir_frame.pack(fill="x", padx=20, pady=6)

        self.dir_var = tk.StringVar()
        dir_entry = tk.Entry(
            dir_frame,
            textvariable=self.dir_var,
            font=("Segoe UI", 9),
            relief="solid",
            bd=1
        )
        dir_entry.pack(fill="x", pady=(0, 8))

        btn_row = tk.Frame(dir_frame, bg=self.card_bg)
        btn_row.pack(fill="x")

        browse_btn = tk.Button(
            btn_row,
            text="📁 Обзор...",
            font=("Segoe UI", 9),
            command=self.browse_directory,
            relief="groove",
            padx=10
        )
        browse_btn.pack(side="left", padx=(0, 6))

        open_btn = tk.Button(
            btn_row,
            text="Открыть папку",
            font=("Segoe UI", 9),
            command=self.open_current_directory,
            relief="groove",
            padx=10
        )
        open_btn.pack(side="left")

        # 2. Секция: Папка для видеозаписей
        vdir_frame = tk.LabelFrame(
            self.root,
            text=" Папка для видеозаписей ",
            font=("Segoe UI", 10, "bold"),
            bg=self.card_bg,
            fg=self.text_color,
            padx=12,
            pady=10
        )
        vdir_frame.pack(fill="x", padx=20, pady=6)

        self.video_dir_var = tk.StringVar()
        vdir_entry = tk.Entry(
            vdir_frame,
            textvariable=self.video_dir_var,
            font=("Segoe UI", 9),
            relief="solid",
            bd=1
        )
        vdir_entry.pack(fill="x", pady=(0, 8))

        vbtn_row = tk.Frame(vdir_frame, bg=self.card_bg)
        vbtn_row.pack(fill="x")

        vbrowse_btn = tk.Button(
            vbtn_row,
            text="📁 Обзор...",
            font=("Segoe UI", 9),
            command=self.browse_video_directory,
            relief="groove",
            padx=10
        )
        vbrowse_btn.pack(side="left", padx=(0, 6))

        vopen_btn = tk.Button(
            vbtn_row,
            text="Открыть папку",
            font=("Segoe UI", 9),
            command=self.open_current_video_directory,
            relief="groove",
            padx=10
        )
        vopen_btn.pack(side="left")

        # Кнопка установки FFmpeg (отображается ТОЛЬКО если FFmpeg не установлен)
        self.ffmpeg_btn = tk.Button(
            vbtn_row,
            text="📥 Установить FFmpeg для видеозаписи",
            font=("Segoe UI", 9),
            command=self.start_ffmpeg_installation,
            relief="groove",
            padx=10,
            bg="#e3f2fd",
            fg="#0d47a1"
        )
        self.check_ffmpeg_ui()

        # 3. Секция: Звук при видеозаписи
        audio_frame = tk.LabelFrame(
            self.root,
            text=" Звук при видеозаписи ",
            font=("Segoe UI", 10, "bold"),
            bg=self.card_bg,
            fg=self.text_color,
            padx=12,
            pady=8
        )
        audio_frame.pack(fill="x", padx=20, pady=6)

        self.sys_audio_var = tk.BooleanVar(value=False)
        self.cb_sys_audio = tk.Checkbutton(
            audio_frame,
            text="Записывать звук системы",
            variable=self.sys_audio_var,
            bg=self.card_bg,
            font=("Segoe UI", 9)
        )
        self.cb_sys_audio.pack(anchor="w")

        self.mic_var = tk.BooleanVar(value=False)
        self.cb_mic = tk.Checkbutton(
            audio_frame,
            text="Записывать микрофон",
            variable=self.mic_var,
            command=self.on_mic_toggle,
            bg=self.card_bg,
            font=("Segoe UI", 9)
        )
        self.cb_mic.pack(anchor="w")

        self.mic_choice_frame = tk.Frame(audio_frame, bg=self.card_bg)
        lbl_mic = tk.Label(self.mic_choice_frame, text="Выбор микрофона:", font=("Segoe UI", 9), bg=self.card_bg)
        lbl_mic.pack(side="left", padx=(0, 8))
        self.mic_combo_var = tk.StringVar()
        self.mic_combo = ttk.Combobox(self.mic_choice_frame, textvariable=self.mic_combo_var, state="readonly", width=34)
        self.mic_combo.pack(side="left", fill="x", expand=True)

        # 4. Секция: Горячие клавиши
        hk_frame = tk.LabelFrame(
            self.root,
            text=" Горячие клавиши ",
            font=("Segoe UI", 10, "bold"),
            bg=self.card_bg,
            fg=self.text_color,
            padx=12,
            pady=10
        )
        hk_frame.pack(fill="x", padx=20, pady=6)

        # Режим 1: Полный экран
        self.create_hotkey_row(hk_frame, "1. Полный экран:", "full")

        # Режим 2: Выделение области (живое)
        self.create_hotkey_row(hk_frame, "2. Выделение (живое):", "live")

        # Режим 3: Заморозка экрана
        self.create_hotkey_row(hk_frame, "3. Заморозка экрана:", "frozen")

        # Режим 4: Видеозапись экрана
        self.create_hotkey_row(hk_frame, "4. Видеозапись (старт/стоп):", "record")

        # 4. Секция: Формат и качество
        fmt_frame = tk.LabelFrame(
            self.root,
            text=" Формат и качество изображения ",
            font=("Segoe UI", 10, "bold"),
            bg=self.card_bg,
            fg=self.text_color,
            padx=12,
            pady=10
        )
        fmt_frame.pack(fill="x", padx=20, pady=6)

        self.fmt_var = tk.StringVar(value="png")
        radio_row = tk.Frame(fmt_frame, bg=self.card_bg)
        radio_row.pack(fill="x", pady=(0, 6))

        rb_png = tk.Radiobutton(
            radio_row, text="PNG (без сжатия, максимальное качество)",
            variable=self.fmt_var, value="png",
            bg=self.card_bg, command=self.on_format_change,
            font=("Segoe UI", 9)
        )
        rb_png.pack(anchor="w")

        rb_jpg = tk.Radiobutton(
            radio_row, text="JPG (сжатие с качеством)",
            variable=self.fmt_var, value="jpg",
            bg=self.card_bg, command=self.on_format_change,
            font=("Segoe UI", 9)
        )
        rb_jpg.pack(anchor="w")

        self.slider_row = tk.Frame(fmt_frame, bg=self.card_bg)
        # self.slider_row отображается только при выборе формата JPG

        self.qlbl = tk.Label(self.slider_row, text="Качество JPG:", font=("Segoe UI", 9), bg=self.card_bg)
        self.qlbl.pack(side="left")

        self.quality_slider = tk.Scale(
            self.slider_row,
            from_=1, to=100,
            orient="horizontal",
            showvalue=False,
            command=self.on_quality_change,
            bg=self.card_bg,
            highlightthickness=0
        )
        self.quality_slider.pack(side="left", fill="x", expand=True, padx=(10, 8))

        self.quality_val_var = tk.StringVar(value="90")
        self.quality_entry = tk.Spinbox(
            self.slider_row,
            from_=1, to=100,
            textvariable=self.quality_val_var,
            width=4,
            font=("Segoe UI", 9),
            command=self.on_quality_entry_change
        )
        self.quality_entry.pack(side="left", padx=(0, 2))
        self.quality_entry.bind("<KeyRelease>", self.on_quality_entry_change)

        self.qlbl_percent = tk.Label(self.slider_row, text="%", font=("Segoe UI", 9), bg=self.card_bg)
        self.qlbl_percent.pack(side="left")

        # 4. Секция: Опции и автозагрузка
        opts_frame = tk.LabelFrame(
            self.root,
            text=" Параметры работы ",
            font=("Segoe UI", 10, "bold"),
            bg=self.card_bg,
            fg=self.text_color,
            padx=12,
            pady=8
        )
        opts_frame.pack(fill="x", padx=20, pady=6)

        self.clip_var = tk.BooleanVar(value=True)
        cb_clip = tk.Checkbutton(
            opts_frame, text="Копировать скриншот в буфер обмена",
            variable=self.clip_var, bg=self.card_bg, font=("Segoe UI", 9)
        )
        cb_clip.pack(anchor="w")

        self.disk_var = tk.BooleanVar(value=True)
        cb_disk = tk.Checkbutton(
            opts_frame, text="Сохранять скриншот в папку",
            variable=self.disk_var, bg=self.card_bg, font=("Segoe UI", 9)
        )
        cb_disk.pack(anchor="w")

        self.notif_var = tk.BooleanVar(value=True)
        cb_notif = tk.Checkbutton(
            opts_frame, text="Показывать всплывающие уведомления Windows",
            variable=self.notif_var, bg=self.card_bg, font=("Segoe UI", 9)
        )
        cb_notif.pack(anchor="w")

        self.auto_var = tk.BooleanVar(value=True)
        cb_auto = tk.Checkbutton(
            opts_frame, text="Запускать автоматически вместе с Windows",
            variable=self.auto_var, bg=self.card_bg, font=("Segoe UI", 9)
        )
        cb_auto.pack(anchor="w")

        # Нижняя панель действий
        bottom_frame = tk.Frame(self.root, bg=self.bg_color)
        bottom_frame.pack(fill="x", padx=20, pady=(12, 10))

        del_btn = tk.Button(
            bottom_frame,
            text="Удалить из системы",
            font=("Segoe UI", 9),
            fg="#D13438",
            command=self.confirm_uninstall,
            relief="groove"
        )
        del_btn.pack(side="left")

        save_btn = tk.Button(
            bottom_frame,
            text="Сохранить",
            font=("Segoe UI", 9, "bold"),
            bg=self.primary_color,
            fg="white",
            relief="flat",
            padx=18,
            pady=4,
            command=self.save_and_apply
        )
        save_btn.pack(side="right")

        # Подвал: версия, кнопка обновления и переход на GitHub
        footer_frame = tk.Frame(self.root, bg=self.bg_color)
        footer_frame.pack(fill="x", padx=20, pady=(2, 12))

        version_lbl = tk.Label(
            footer_frame,
            text=f"WinScreen v{APP_VERSION}",
            font=("Segoe UI", 9),
            fg="#707070",
            bg=self.bg_color
        )
        version_lbl.pack(side="left")

        self.update_btn = tk.Button(
            footer_frame,
            text="🚀 Обновить",
            font=("Segoe UI", 9, "bold"),
            bg="#107C41",
            fg="white",
            relief="flat",
            padx=10,
            pady=2,
            command=self.on_update_clicked
        )
        # Отображается только при наличии более новой версии на GitHub

        github_btn = tk.Button(
            footer_frame,
            text="GitHub",
            font=("Segoe UI", 9),
            relief="groove",
            command=lambda: webbrowser.open(GITHUB_REPO_URL)
        )
        github_btn.pack(side="right")

        self.root.bind("<KeyPress>", self.on_key_press)
        self.root.bind("<KeyRelease>", self.on_key_release)

    def create_hotkey_row(self, parent, label_text, row_id):
        row = tk.Frame(parent, bg=self.card_bg)
        row.pack(fill="x", pady=4)

        lbl = tk.Label(row, text=label_text, font=("Segoe UI", 9, "bold"), bg=self.card_bg, width=22, anchor="w")
        lbl.pack(side="left")

        var = tk.StringVar(value="")
        entry = tk.Entry(
            row,
            textvariable=var,
            font=("Segoe UI", 9),
            relief="solid",
            bd=1,
            state="readonly",
            width=22
        )
        entry.pack(side="left", padx=(0, 10))

        btn = tk.Button(
            row,
            text="Задать...",
            font=("Segoe UI", 9),
            command=lambda: self.toggle_recording(row_id),
            relief="groove",
            padx=10
        )
        btn.pack(side="left")

        self.hk_vars[row_id] = var
        self.hk_btns[row_id] = btn

    def toggle_recording(self, row_id):
        if self.recording_target == row_id:
            self.cancel_recording()
            return

        if self.recording_target:
            self.cancel_recording()

        self.recording_target = row_id
        self.pressed_mods.clear()
        self.hk_btns[row_id].config(text="Отмена")
        self.hk_vars[row_id].set("[Нажмите клавиши...]")
        self.root.focus_set()

    def cancel_recording(self):
        if not self.recording_target:
            return
        row_id = self.recording_target
        prev = self.prev_hk_values.get(row_id, "")
        self.hk_vars[row_id].set(prev)
        self.hk_btns[row_id].config(text="Задать...")
        self.recording_target = None
        self.pressed_mods.clear()

    def on_key_press(self, event):
        if not self.recording_target:
            return

        row_id = self.recording_target
        keysym = event.keysym.lower()

        if keysym == "escape":
            self.cancel_recording()
            return "break"

        # Проверяем модификаторы
        is_mod = False
        if "control" in keysym or keysym in ["ctrl_l", "ctrl_r"]:
            self.pressed_mods.add("Ctrl")
            is_mod = True
        elif "shift" in keysym:
            self.pressed_mods.add("Shift")
            is_mod = True
        elif "alt" in keysym:
            self.pressed_mods.add("Alt")
            is_mod = True
        elif "win" in keysym or "super" in keysym:
            self.pressed_mods.add("Win")
            is_mod = True

        # Проверяем флаги состояния модификаторов event.state
        if event.state & 0x0004:
            self.pressed_mods.add("Ctrl")
        if event.state & 0x0001:
            self.pressed_mods.add("Shift")
        if event.state & 0x20000 or event.state & 0x0008:
            self.pressed_mods.add("Alt")

        if is_mod:
            mods_str = " + ".join(sorted(self.pressed_mods)) + " + ..."
            self.hk_vars[row_id].set(mods_str)
            return "break"

        # Нажата основная клавиша
        if "print" in keysym or "snapshot" in keysym:
            key_name = "PrintScreen"
        elif keysym.startswith("f") and keysym[1:].isdigit():
            key_name = keysym.upper()
        elif keysym == "space":
            key_name = "Space"
        elif keysym == "return":
            key_name = "Enter"
        elif len(keysym) == 1:
            key_name = keysym.upper()
        else:
            key_name = keysym.capitalize()

        ordered_mods = []
        for m in ["Win", "Ctrl", "Shift", "Alt"]:
            if m in self.pressed_mods:
                ordered_mods.append(m)

        if ordered_mods:
            final_str = " + ".join(ordered_mods + [key_name])
        else:
            final_str = key_name

        self.hk_vars[row_id].set(final_str)
        self.prev_hk_values[row_id] = final_str
        self.hk_btns[row_id].config(text="Задать...")
        self.recording_target = None
        self.pressed_mods.clear()
        return "break"

    def on_key_release(self, event):
        if not self.recording_target:
            return

        row_id = self.recording_target
        keysym = event.keysym.lower()

        mod_changed = False
        if "control" in keysym or keysym in ["ctrl_l", "ctrl_r"]:
            self.pressed_mods.discard("Ctrl")
            mod_changed = True
        elif "shift" in keysym:
            self.pressed_mods.discard("Shift")
            mod_changed = True
        elif "alt" in keysym:
            self.pressed_mods.discard("Alt")
            mod_changed = True
        elif "win" in keysym or "super" in keysym:
            self.pressed_mods.discard("Win")
            mod_changed = True

        if mod_changed:
            if self.pressed_mods:
                mods_str = " + ".join(sorted(self.pressed_mods)) + " + ..."
                self.hk_vars[row_id].set(mods_str)
            else:
                self.hk_vars[row_id].set("[Нажмите клавиши...]")

    def browse_directory(self):
        current = self.dir_var.get()
        selected = filedialog.askdirectory(initialdir=current, title="Выберите папку для скриншотов")
        if selected:
            self.dir_var.set(os.path.normpath(selected))

    def open_current_directory(self):
        target = self.dir_var.get()
        if os.path.exists(target):
            if sys.platform.startswith("win"):
                os.startfile(target)
            else:
                subprocess.run(["xdg-open", target])
        else:
            messagebox.showwarning("Внимание", "Указанная папка еще не существует.")

    def browse_video_directory(self):
        current = self.video_dir_var.get()
        selected = filedialog.askdirectory(initialdir=current, title="Выберите папку для видеозаписей")
        if selected:
            self.video_dir_var.set(os.path.normpath(selected))

    def open_current_video_directory(self):
        target = self.video_dir_var.get()
        if os.path.exists(target):
            if sys.platform.startswith("win"):
                os.startfile(target)
            else:
                subprocess.run(["xdg-open", target])
        else:
            messagebox.showwarning("Внимание", "Указанная папка еще не существует.")

    def on_format_change(self):
        is_jpg = (self.fmt_var.get() == "jpg")
        if is_jpg:
            self.slider_row.pack(fill="x", pady=2)
        else:
            self.slider_row.pack_forget()

    def check_ffmpeg_ui(self):
        """Если FFmpeg уже установлен и работает, кнопка установки не отображается."""
        if find_ffmpeg_executable():
            if hasattr(self, "ffmpeg_btn") and self.ffmpeg_btn:
                self.ffmpeg_btn.pack_forget()
        else:
            if hasattr(self, "ffmpeg_btn") and self.ffmpeg_btn:
                self.ffmpeg_btn.pack(side="left", padx=(6, 0))

    def start_ffmpeg_installation(self):
        self.ffmpeg_btn.config(state="disabled", text="⏳ Подготовка к загрузке...")

        def worker():
            try:
                def on_progress(percent, downloaded, total):
                    mb_down = downloaded / (1024 * 1024)
                    mb_tot = total / (1024 * 1024)
                    self.root.after(0, lambda: self.ffmpeg_btn.config(
                        text=f"⏳ Загрузка FFmpeg: {percent}% ({mb_down:.1f}/{mb_tot:.1f} МБ)"
                    ))

                install_ffmpeg(progress_callback=on_progress)
                self.root.after(0, self._on_ffmpeg_success)
            except Exception as err:
                self.root.after(0, lambda: self._on_ffmpeg_fail(str(err)))

        threading.Thread(target=worker, daemon=True).start()

    def _on_ffmpeg_success(self):
        messagebox.showinfo(
            "WinScreen",
            "FFmpeg успешно установлен!\nВидеозапись экрана теперь готова к работе."
        )
        self.check_ffmpeg_ui()

    def _on_ffmpeg_fail(self, error_msg):
        messagebox.showerror(
            "Ошибка установки FFmpeg",
            f"Не удалось установить FFmpeg:\n{error_msg}\n\nПроверьте подключение к Интернету."
        )
        self.ffmpeg_btn.config(state="normal", text="📥 Попробовать снова установить FFmpeg")

    def on_quality_change(self, val):
        if hasattr(self, "quality_val_var"):
            self.quality_val_var.set(str(int(float(val))))

    def on_quality_entry_change(self, event=None):
        try:
            val = int(self.quality_val_var.get())
            if 1 <= val <= 100:
                self.quality_slider.set(val)
        except (ValueError, tk.TclError):
            pass

    def on_mic_toggle(self):
        mics = self.detected_devices.get("microphones", [])
        if len(mics) > 1 and self.mic_var.get():
            self.mic_choice_frame.pack(fill="x", pady=(4, 2), padx=(20, 0))
        else:
            self.mic_choice_frame.pack_forget()

    def load_values(self):
        self.dir_var.set(cfg.get("save_directory"))
        self.video_dir_var.set(cfg.get("video_save_directory", get_default_recordings_dir()))

        # Настройки звука видео
        self.detected_devices = get_audio_devices()
        mics = self.detected_devices.get("microphones", [])

        self.sys_audio_var.set(bool(cfg.get("record_system_audio", False)))

        saved_rec_mic = bool(cfg.get("record_microphone", False))
        saved_selected_mic = cfg.get("selected_microphone", "")

        if not mics:
            self.mic_var.set(False)
            self.cb_mic.config(state="disabled", text="Записывать микрофон (микрофон не обнаружен)")
            self.mic_choice_frame.pack_forget()
        elif len(mics) == 1:
            self.cb_mic.config(state="normal", text=f"Записывать микрофон ({mics[0]})")
            self.mic_var.set(saved_rec_mic)
            self.mic_combo["values"] = mics
            self.mic_combo_var.set(mics[0])
            self.mic_choice_frame.pack_forget()
        else:
            self.cb_mic.config(state="normal", text="Записывать микрофон")
            self.mic_var.set(saved_rec_mic)
            self.mic_combo["values"] = mics
            if saved_selected_mic in mics:
                self.mic_combo_var.set(saved_selected_mic)
            else:
                self.mic_combo_var.set(mics[0])
            self.on_mic_toggle()

        def to_display(hk_str):
            parts = [p.strip().capitalize() for p in hk_str.split("+")]
            display_parts = []
            for p in parts:
                if p.lower() in ["print_screen", "printscreen", "snapshot"]:
                    display_parts.append("PrintScreen")
                elif p.lower() == "ctrl":
                    display_parts.append("Ctrl")
                elif p.lower() == "shift":
                    display_parts.append("Shift")
                elif p.lower() == "alt":
                    display_parts.append("Alt")
                elif p.lower() == "win":
                    display_parts.append("Win")
                else:
                    display_parts.append(p.upper() if len(p) <= 3 else p.capitalize())
            return " + ".join(display_parts)

        hk_full = to_display(cfg.get("hotkey_fullscreen", "ctrl+print_screen"))
        hk_live = to_display(cfg.get("hotkey_area_live", "shift+print_screen"))
        hk_frozen = to_display(cfg.get("hotkey_area_frozen", "print_screen"))
        hk_record = to_display(cfg.get("hotkey_record_video", "ctrl+shift+print_screen"))

        self.hk_vars["full"].set(hk_full)
        self.hk_vars["live"].set(hk_live)
        self.hk_vars["frozen"].set(hk_frozen)
        self.hk_vars["record"].set(hk_record)

        self.prev_hk_values["full"] = hk_full
        self.prev_hk_values["live"] = hk_live
        self.prev_hk_values["frozen"] = hk_frozen
        self.prev_hk_values["record"] = hk_record

        fmt = cfg.get("file_format", "png")
        self.fmt_var.set(fmt)
        quality = int(cfg.get("jpg_quality", 90))
        self.quality_slider.set(quality)
        self.quality_val_var.set(str(quality))
        self.on_format_change()

        self.clip_var.set(cfg.get("copy_to_clipboard", True))
        self.disk_var.set(cfg.get("save_to_disk", True))
        self.notif_var.set(cfg.get("show_notifications", True))
        self.auto_var.set(is_autostart_enabled())

    def save_and_apply(self):
        # Сохранение настроек
        cfg.set("save_directory", self.dir_var.get())
        cfg.set("video_save_directory", self.video_dir_var.get())

        def to_cfg_str(display_str):
            parts = [p.strip().lower() for p in display_str.split("+")]
            cfg_parts = []
            for p in parts:
                if p in ["printscreen", "print_screen", "snapshot"]:
                    cfg_parts.append("print_screen")
                else:
                    cfg_parts.append(p)
            return "+".join(cfg_parts)

        cfg.set("hotkey_fullscreen", to_cfg_str(self.hk_vars["full"].get()))
        cfg.set("hotkey_area_live", to_cfg_str(self.hk_vars["live"].get()))
        cfg.set("hotkey_area_frozen", to_cfg_str(self.hk_vars["frozen"].get()))
        cfg.set("hotkey_record_video", to_cfg_str(self.hk_vars["record"].get()))

        cfg.set("file_format", self.fmt_var.get())
        try:
            q_val = int(self.quality_val_var.get())
        except (ValueError, tk.TclError):
            q_val = int(self.quality_slider.get())
        cfg.set("jpg_quality", max(1, min(100, q_val)))
        cfg.set("copy_to_clipboard", self.clip_var.get())
        cfg.set("save_to_disk", self.disk_var.get())
        cfg.set("show_notifications", self.notif_var.get())

        # Аудио настройки
        cfg.set("record_system_audio", self.sys_audio_var.get())
        cfg.set("record_microphone", self.mic_var.get())
        if self.mic_combo_var.get():
            cfg.set("selected_microphone", self.mic_combo_var.get())

        # Автозагрузка
        set_autostart(self.auto_var.get())

        messagebox.showinfo("WinScreen", "Настройки успешно сохранены!")
        self.root.destroy()
        if self.on_close_callback:
            self.on_close_callback()

    def confirm_uninstall(self):
        if messagebox.askyesno("Удаление WinScreen", "Вы действительно хотите удалить WinScreen из автозагрузки и системы?"):
            uninstall()
            messagebox.showinfo("WinScreen", "WinScreen успешно удален из автозагрузки.")
            self.root.destroy()
            sys.exit(0)

    def check_updates_background(self):
        info = check_for_updates()
        if info and info.get("has_update"):
            self.root.after(0, lambda: self._show_update_button(info))

    def _show_update_button(self, info):
        self.update_info = info
        self.update_btn.config(text=f"🚀 Обновить ({info['latest_version']})")
        self.update_btn.pack(side="left", padx=10)

    def on_update_clicked(self):
        if not self.update_info:
            return
        info = self.update_info
        res = messagebox.askyesnocancel(
            "Обновление WinScreen",
            f"Доступна новая версия {info['latest_version']}!\n\n"
            "Нажмите «Да», чтобы обновить приложение автоматически без переустановки.\n"
            "Нажмите «Нет», чтобы перейти на страницу релиза на GitHub.\n"
            "Нажмите «Отмена», чтобы отложить."
        )
        if res is False:
            webbrowser.open(info["release_url"])
        elif res is True:
            self.update_btn.config(state="disabled", text="⏳ Загрузка обновления...")
            threading.Thread(target=self._perform_auto_update, args=(info,), daemon=True).start()

    def _perform_auto_update(self, info):
        try:
            import tempfile
            temp_dir = tempfile.gettempdir()
            new_exe = os.path.join(temp_dir, "WinScreen_update.exe")
            bat_path = os.path.join(temp_dir, "winscreen_updater.bat")
            cur_exe = sys.executable if getattr(sys, "frozen", False) else os.path.abspath(sys.argv[0])

            req = urllib.request.Request(info["download_url"], headers={"User-Agent": "WinScreen-UpdateChecker"})
            with urllib.request.urlopen(req, timeout=30) as resp, open(new_exe, "wb") as f:
                f.write(resp.read())

            with open(bat_path, "w", encoding="utf-8") as bat:
                bat.write(
                    f'@echo off\n'
                    f'chcp 65001 >nul\n'
                    f'timeout /t 1 /nobreak >nul\n'
                    f':retry\n'
                    f'del /f /q "{cur_exe}" >nul 2>&1\n'
                    f'if exist "{cur_exe}" (\n'
                    f'    timeout /t 1 /nobreak >nul\n'
                    f'    goto retry\n'
                    f')\n'
                    f'move /y "{new_exe}" "{cur_exe}" >nul 2>&1\n'
                    f'start "" "{cur_exe}"\n'
                    f'del /f /q "%~f0" >nul 2>&1\n'
                )

            subprocess.Popen(["cmd.exe", "/c", bat_path], creationflags=0x08000000 if os.name == "nt" else 0)
            self.root.destroy()
            sys.exit(0)
        except Exception as e:
            def on_fail():
                self.update_btn.config(state="normal", text="🚀 Обновить")
                if messagebox.askyesno("Ошибка авто-обновления", f"Не удалось автоматически загрузить файл обновления: {e}\n\nОткрыть страницу релиза на GitHub?"):
                    webbrowser.open(info["release_url"])
            self.root.after(0, on_fail)

    def show(self):
        self.root.mainloop()


def open_settings_window(on_close=None):
    app = SettingsWindow(on_close_callback=on_close)
    app.show()


if __name__ == "__main__":
    open_settings_window()
