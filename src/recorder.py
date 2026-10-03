"""
Модуль видеозаписи экрана WinScreen.
Поддерживает высокопроизводительную запись через FFmpeg (gdigrab на Windows / x11grab на Linux)
с автоматическим резервным вариантом (fallback) через OpenCV (cv2.VideoWriter).
"""

import os
import sys
import re
import time
import shutil
import threading
import subprocess
import urllib.request
from datetime import datetime

from config import cfg, get_default_recordings_dir
from notifier import notify_recording_started, notify_recording_stopped, notify_error


_dshow_alt_names = {}


def get_audio_devices():
    """
    Возвращает список обнаруженных микрофонов и устройство звука системы (Stereo Mix).
    Формат: {'microphones': [list of str], 'system_audio': str or None}
    """
    global _dshow_alt_names
    mics = []
    sys_audio = None

    if sys.platform.startswith("win"):
        ffmpeg_bin = find_ffmpeg_executable()
        if ffmpeg_bin:
            try:
                cmd = [ffmpeg_bin, "-list_devices", "true", "-f", "dshow", "-i", "dummy"]
                creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
                res = subprocess.run(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    encoding="utf-8",
                    errors="ignore",
                    creationflags=creationflags,
                    timeout=5
                )
                in_audio = False
                last_name = None
                for line in res.stderr.splitlines():
                    if "DirectShow audio devices" in line:
                        in_audio = True
                        continue
                    if in_audio:
                        if "DirectShow video devices" in line:
                            break
                        if "Alternative name" in line:
                            alt_match = re.search(r'"([^"]+)"', line)
                            if alt_match and last_name:
                                _dshow_alt_names[last_name] = alt_match.group(1).strip()
                            continue
                        match = re.search(r'"([^"]+)"', line)
                        if match:
                            name = match.group(1).strip()
                            last_name = name
                            lower = name.lower()
                            if any(s in lower for s in ["stereo mix", "стерео микшер", "стерео микс", "what u hear", "wave out", "cable output", "virtual-audio-capturer", "loopback"]):
                                if not sys_audio:
                                    sys_audio = name
                            else:
                                if name not in mics:
                                    mics.append(name)
            except Exception as e:
                print(f"[WinScreen] Ошибка обнаружения аудио через FFmpeg: {e}")
    else:
        # Linux detection: монитор PulseAudio/PipeWire для захвата звука системы
        try:
            pactl_res = subprocess.run(["pactl", "get-default-sink"], capture_output=True, text=True, errors="ignore")
            if pactl_res.returncode == 0 and pactl_res.stdout.strip():
                sys_audio = f"{pactl_res.stdout.strip()}.monitor"
            else:
                sys_audio = "default.monitor"
        except Exception:
            sys_audio = "default.monitor"

        try:
            res = subprocess.run(["arecord", "-l"], capture_output=True, text=True, errors="ignore")
            for line in res.stdout.splitlines():
                if line.startswith("card"):
                    match = re.search(r'card\s+\d+:\s+([^,]+),\s+device\s+\d+:\s+([^\[]+)', line)
                    if match:
                        name = f"{match.group(1).strip()} ({match.group(2).strip()})"
                        if name not in mics:
                            mics.append(name)
        except Exception:
            pass

    return {"microphones": mics, "system_audio": sys_audio}


def find_ffmpeg_executable():
    """
    Ищет исполняемый файл ffmpeg:
    1. В системном PATH
    2. Рядом с исполняемым файлом/скриптом
    3. В директории %APPDATA%\\WinScreen
    """
    system_ffmpeg = shutil.which("ffmpeg")
    if system_ffmpeg:
        return system_ffmpeg

    current_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
    exe_name = "ffmpeg.exe" if sys.platform.startswith("win") else "ffmpeg"
    local_ffmpeg = os.path.join(current_dir, exe_name)
    if os.path.exists(local_ffmpeg):
        return local_ffmpeg

    app_dir = cfg.app_dir
    app_ffmpeg = os.path.join(app_dir, exe_name)
    if os.path.exists(app_ffmpeg):
        return app_ffmpeg

    return None


def install_ffmpeg(progress_callback=None) -> str:
    """
    Загружает и устанавливает автономный бинарник FFmpeg в директорию приложения (%APPDATA%\\WinScreen).
    :param progress_callback: callback(percent: int, downloaded_bytes: int, total_bytes: int)
    :return: абсолютный путь к установленному ffmpeg
    """
    if sys.platform.startswith("win"):
        url = "https://github.com/imageio/imageio-binaries/raw/master/ffmpeg/ffmpeg-win64-v4.2.2.exe"
        exe_name = "ffmpeg.exe"
    elif sys.platform.startswith("darwin"):
        url = "https://github.com/imageio/imageio-binaries/raw/master/ffmpeg/ffmpeg-osx64-v4.2.2"
        exe_name = "ffmpeg"
    else:
        url = "https://github.com/imageio/imageio-binaries/raw/master/ffmpeg/ffmpeg-linux64-v4.2.2"
        exe_name = "ffmpeg"

    dest_dir = cfg.app_dir
    os.makedirs(dest_dir, exist_ok=True)
    dest_path = os.path.join(dest_dir, exe_name)
    temp_path = dest_path + ".download"

    try:
        req = urllib.request.Request(
            url,
            headers={"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) WinScreen/1.0"}
        )
        with urllib.request.urlopen(req, timeout=60) as response, open(temp_path, "wb") as out_file:
            total_size_header = response.headers.get("Content-Length")
            total_size = int(total_size_header) if total_size_header else 0
            downloaded = 0
            chunk_size = 64 * 1024

            while True:
                chunk = response.read(chunk_size)
                if not chunk:
                    break
                out_file.write(chunk)
                downloaded += len(chunk)
                if progress_callback and total_size > 0:
                    percent = min(100, int((downloaded / total_size) * 100))
                    progress_callback(percent, downloaded, total_size)

        if not sys.platform.startswith("win"):
            os.chmod(temp_path, 0o755)

        if os.path.exists(dest_path):
            try:
                os.remove(dest_path)
            except Exception:
                pass
        os.replace(temp_path, dest_path)
        return dest_path
    except Exception as e:
        if os.path.exists(temp_path):
            try:
                os.remove(temp_path)
            except Exception:
                pass
        raise RuntimeError(f"Не удалось скачать FFmpeg: {e}")


class ScreenRecorder:
    def __init__(self):
        self._lock = threading.Lock()
        self._is_recording = False
        self._ffmpeg_process = None
        self._cv_thread = None
        self._stop_cv_event = threading.Event()
        self._current_file = None
        self._on_state_change = []

    def add_state_listener(self, callback):
        """Регистрирует функцию обратного вызова при изменении состояния записи (start/stop)."""
        if callback not in self._on_state_change:
            self._on_state_change.append(callback)

    def _notify_state_change(self):
        for cb in self._on_state_change:
            try:
                cb(self._is_recording)
            except Exception as e:
                print(f"[WinScreen] Ошибка в listener состояния видео: {e}")

    def is_recording(self) -> bool:
        with self._lock:
            return self._is_recording

    def _generate_video_path(self) -> str:
        video_dir = cfg.get("video_save_directory", get_default_recordings_dir())
        os.makedirs(video_dir, exist_ok=True)

        now = datetime.now()
        timestamp = now.strftime("%Y-%m-%d_%H-%M-%S")
        filename = f"Запись_{timestamp}.mp4"
        file_path = os.path.join(video_dir, filename)

        counter = 1
        while os.path.exists(file_path):
            filename = f"Запись_{timestamp}_{counter}.mp4"
            file_path = os.path.join(video_dir, filename)
            counter += 1

        return file_path

    def start_recording(self) -> bool:
        with self._lock:
            if self._is_recording:
                return False

            file_path = self._generate_video_path()
            fps = int(cfg.get("video_fps", 30))
            ffmpeg_path = find_ffmpeg_executable()

            if ffmpeg_path:
                started = self._start_ffmpeg_recording(ffmpeg_path, file_path, fps)
            else:
                started = self._start_opencv_recording(file_path, fps)

            if started:
                self._is_recording = True
                self._current_file = file_path
                hotkey_disp = cfg.get("hotkey_record_video", "ctrl+shift+print_screen").replace("_", " ").upper()
                notify_recording_started(hotkey_disp)
                self._notify_state_change()
                print(f"[WinScreen] Запись видео запущена: {file_path}")
                return True
            else:
                return False

    def _start_ffmpeg_recording(self, ffmpeg_bin: str, output_path: str, fps: int) -> bool:
        try:
            rec_sys = cfg.get("record_system_audio", False)
            rec_mic = cfg.get("record_microphone", False)
            chosen_mic = cfg.get("selected_microphone", "")

            audio_devs = get_audio_devices()
            available_mics = audio_devs["microphones"]
            sys_audio = audio_devs["system_audio"]

            mic_to_use = None
            if rec_mic and available_mics:
                if chosen_mic and chosen_mic in available_mics:
                    mic_to_use = chosen_mic
                else:
                    mic_to_use = available_mics[0]

            sys_to_use = sys_audio if (rec_sys and sys_audio) else None

            if sys.platform.startswith("win"):
                mic_spec = _dshow_alt_names.get(mic_to_use, mic_to_use) if mic_to_use else None
                sys_spec = _dshow_alt_names.get(sys_to_use, sys_to_use) if sys_to_use else None

                cmd = [
                    ffmpeg_bin,
                    "-y",
                    "-f", "gdigrab",
                    "-framerate", str(fps),
                    "-i", "desktop"
                ]

                if mic_spec and sys_spec:
                    cmd += [
                        "-f", "dshow", "-i", f"audio={mic_spec}",
                        "-f", "dshow", "-i", f"audio={sys_spec}",
                        "-filter_complex", "[1:a][2:a]amix=inputs=2:duration=first[aout]",
                        "-map", "0:v",
                        "-map", "[aout]",
                        "-c:a", "aac"
                    ]
                elif mic_spec:
                    cmd += [
                        "-f", "dshow", "-i", f"audio={mic_spec}",
                        "-c:a", "aac"
                    ]
                elif sys_spec:
                    cmd += [
                        "-f", "dshow", "-i", f"audio={sys_spec}",
                        "-c:a", "aac"
                    ]

                cmd += [
                    "-c:v", "libx264",
                    "-preset", "ultrafast",
                    "-pix_fmt", "yuv420p",
                    "-shortest",
                    "-movflags", "+faststart",
                    output_path
                ]
                creationflags = subprocess.CREATE_NO_WINDOW
            else:
                cmd = [
                    ffmpeg_bin,
                    "-y",
                    "-f", "x11grab",
                    "-framerate", str(fps),
                    "-i", os.environ.get("DISPLAY", ":0.0")
                ]
                if mic_to_use and sys_to_use:
                    cmd += [
                        "-f", "pulse", "-i", "default",
                        "-f", "pulse", "-i", sys_to_use,
                        "-filter_complex", "[1:a][2:a]amix=inputs=2:duration=first[aout]",
                        "-map", "0:v",
                        "-map", "[aout]",
                        "-c:a", "aac"
                    ]
                elif mic_to_use:
                    cmd += ["-f", "pulse", "-i", "default", "-c:a", "aac"]
                elif sys_to_use:
                    cmd += ["-f", "pulse", "-i", sys_to_use, "-c:a", "aac"]

                cmd += [
                    "-c:v", "libx264",
                    "-preset", "ultrafast",
                    "-pix_fmt", "yuv420p",
                    "-shortest",
                    "-movflags", "+faststart",
                    output_path
                ]
                creationflags = 0

            self._ffmpeg_process = subprocess.Popen(
                cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                creationflags=creationflags
            )
            time.sleep(0.2)
            if self._ffmpeg_process.poll() is not None:
                print(f"[WinScreen] FFmpeg завершился с ошибкой: код {self._ffmpeg_process.returncode}")
                notify_error(f"FFmpeg завершился с ошибкой (код {self._ffmpeg_process.returncode})")
                self._ffmpeg_process = None
                return False
            return True
        except Exception as e:
            print(f"[WinScreen] Ошибка запуска FFmpeg: {e}")
            notify_error(f"Не удалось запустить FFmpeg: {e}")
            return False

    def _start_opencv_recording(self, output_path: str, fps: int) -> bool:
        try:
            import cv2
            import numpy as np
            from PIL import ImageGrab
        except ImportError:
            msg = "Для видеозаписи требуется FFmpeg или библиотеки opencv-python и numpy."
            print(f"[WinScreen] {msg}")
            notify_error(msg)
            return False

        try:
            sample = ImageGrab.grab()
            w, h = sample.size

            fourcc = cv2.VideoWriter_fourcc(*"mp4v")
            writer = cv2.VideoWriter(output_path, fourcc, fps, (w, h))

            if not writer.isOpened():
                # Пробуем резервный кодек AVI если mp4v не поддерживается установленным ffmpeg бэкендом OpenCV
                alt_path = output_path.replace(".mp4", ".avi")
                fourcc = cv2.VideoWriter_fourcc(*"XVID")
                writer = cv2.VideoWriter(alt_path, fourcc, fps, (w, h))
                if not writer.isOpened():
                    notify_error("Не удалось инициализировать кодек видеозаписи OpenCV.")
                    return False
                self._current_file = alt_path

            self._stop_cv_event.clear()

            def capture_loop():
                frame_duration = 1.0 / fps
                while not self._stop_cv_event.is_set():
                    t0 = time.time()
                    try:
                        img = ImageGrab.grab()
                        frame = cv2.cvtColor(np.array(img), cv2.COLOR_RGB2BGR)
                        writer.write(frame)
                    except Exception as err:
                        print(f"[WinScreen] Ошибка захвата кадра: {err}")
                    elapsed = time.time() - t0
                    sleep_time = frame_duration - elapsed
                    if sleep_time > 0:
                        time.sleep(sleep_time)

                writer.release()
                print("[WinScreen] Запись OpenCV завершена и сохранена.")

            self._cv_thread = threading.Thread(target=capture_loop, daemon=True)
            self._cv_thread.start()
            return True
        except Exception as e:
            print(f"[WinScreen] Ошибка старта OpenCV записи: {e}")
            notify_error(f"Ошибка старта записи: {e}")
            return False

    def stop_recording(self) -> str:
        with self._lock:
            if not self._is_recording:
                return None

            saved_path = self._current_file

            # Остановка FFmpeg
            if self._ffmpeg_process:
                try:
                    if self._ffmpeg_process.stdin:
                        self._ffmpeg_process.stdin.write(b"q\n")
                        self._ffmpeg_process.stdin.flush()
                        self._ffmpeg_process.stdin.close()
                    self._ffmpeg_process.wait(timeout=10)
                except Exception:
                    try:
                        self._ffmpeg_process.terminate()
                        self._ffmpeg_process.wait(timeout=3)
                    except Exception:
                        pass
                self._ffmpeg_process = None

            # Остановка OpenCV потока
            if self._cv_thread and self._cv_thread.is_alive():
                self._stop_cv_event.set()
                self._cv_thread.join(timeout=5)
                self._cv_thread = None

            self._is_recording = False
            self._notify_state_change()

            notify_recording_stopped(saved_path)
            print(f"[WinScreen] Запись видео завершена: {saved_path}")
            return saved_path

    def toggle_recording(self):
        """Переключает состояние записи: если идет — останавливает, если нет — запускает."""
        if self.is_recording():
            return self.stop_recording()
        else:
            return self.start_recording()


# Глобальный синглтон модуля записи
recorder = ScreenRecorder()
