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
echo "=== Building WebAssembly module ==="
if command -v em++ > /dev/null; then
    ./scripts/build_wasm.sh

    echo ""
    echo "=== Running CPU unit tests compiled to WebAssembly ==="
    em++ -std=c++17 -O2 -fexceptions tests/test_cpu.cpp \
        -o build/test_cpu_wasm.js
    node build/test_cpu_wasm.js

    echo ""
    echo "=== Running web session unit tests compiled to WebAssembly ==="
    em++ -std=c++17 -O2 -Wall -Wextra -fexceptions tests/test_web_session.cpp \
        -o build/test_web_session_wasm.js
    node build/test_web_session_wasm.js

    if command -v git > /dev/null && git rev-parse --git-dir > /dev/null 2>&1 &&
        ! git diff --quiet -- web/generated; then
        echo ""
        echo "NOTE: web/generated changed in this build. Commit it so the"
        echo "      deployed site uses the current C++ code."
    fi
else
    echo "SKIPPED: Emscripten (em++) is not installed, so the WebAssembly"
    echo "module was NOT rebuilt and the WebAssembly unit tests did NOT run."
    echo "The tests below use the committed web/generated files."
    echo "Install with: brew install emscripten"
fi

echo ""
echo "=== Running web server integration tests ==="
python3 tests/test_web_server.py

echo ""
echo "=== Running WebAssembly interface tests ==="
node tests/test_wasm.mjs

echo ""
echo "=== Running browser frontend tests ==="
node tests/test_frontend.mjs

echo ""
echo "All unit and integration tests passed."
