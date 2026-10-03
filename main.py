#!/usr/bin/env python3
"""
WinScreen — Главная точка входа приложения.
"""
import os
import sys

# Добавляем каталог src/ в sys.path для корректного поиска модулей
SRC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "src")
if SRC_DIR not in sys.path:
    sys.path.insert(0, SRC_DIR)

from main import main

if __name__ == "__main__":
    main()
