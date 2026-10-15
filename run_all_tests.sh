
#!/bin/bash

set -e

echo "=== Building CPU emulator ==="
clang++ -std=c++17 main.cpp -o cpu_emulator

echo ""
echo "=== Running CPU unit tests ==="
clang++ -std=c++17 test_cpu.cpp -o test_cpu
./test_cpu

echo ""
echo "=== Running emulator integration tests ==="
./test_main.sh

echo ""
echo "All unit and integration tests passed."
