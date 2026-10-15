#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "======================================================="
echo "       Запуск набора тестов для WinScreen              "
echo "======================================================="

# 1. Проверка структуры PE заголовков бинарников
echo ">>> [1/3] Тестирование архитектуры PE-бинарников..."
python3 tests/test_binaries.py

# 2. Тестирование логики обновления GUI (Python)
echo ">>> [2/3] Тестирование логики обновлений в Python (gui.py)..."
python3 tests/test_gui_updates.py

# 3. Компиляция и тестирование логики обновлений C++
echo ">>> [3/3] Компиляция и запуск тестов логики C++ обновлений..."
mkdir -p build
g++ -std=c++17 tests/test_update_logic.cpp -o build/test_update_logic
./build/test_update_logic

echo "======================================================="
echo "       Все тесты успешно пройдены! (100% PASS)         "
echo "======================================================="
