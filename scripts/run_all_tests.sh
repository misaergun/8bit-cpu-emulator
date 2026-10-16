#!/bin/bash

set -e

# Always work from the project root, wherever the script is called from.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p build

echo "=== Building CPU emulator ==="
clang++ -std=c++17 src/main.cpp -o build/cpu_emulator

echo ""
echo "=== Running CPU unit tests ==="
clang++ -std=c++17 tests/test_cpu.cpp -o build/test_cpu
./build/test_cpu

echo ""
echo "=== Running emulator integration tests ==="
./tests/test_main.sh

echo ""
echo "=== Building web server ==="
clang++ -std=c++17 -Wall -Wextra src/web_server.cpp -o build/web_server

echo ""
echo "=== Running web session unit tests ==="
clang++ -std=c++17 -Wall -Wextra tests/test_web_session.cpp \
    -o build/test_web_session
./build/test_web_session

echo ""
echo "=== Running web server integration tests ==="
python3 tests/test_web_server.py

echo ""
echo "All unit and integration tests passed."
