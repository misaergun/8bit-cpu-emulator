#!/bin/bash
# Builds the C++ CPU core into WebAssembly for the browser.
# Output: web/generated/cpu.js (loader) and web/generated/cpu.wasm.
# Requires Emscripten (macOS: brew install emscripten).

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! command -v em++ > /dev/null; then
    echo "Error: em++ (Emscripten) not found." >&2
    echo "Install it with: brew install emscripten" >&2
    echo "Then check it with: emcc --version" >&2
    exit 1
fi

OUT_DIR="web/generated"
mkdir -p "$OUT_DIR"

EXPORTS="_cpu_programs,_cpu_state,_cpu_load,_cpu_load_assembly,_cpu_step,_cpu_run,_cpu_reset,_cpu_memory_byte"

# -fexceptions: cpu.h reports CPU faults by throwing, and the session
#               catches them, so C++ exceptions must work in WebAssembly.
# MODULARIZE:   cpu.js defines createCpuModule() instead of a global Module.
# ENVIRONMENT:  web for the browser, node for tests/test_wasm.mjs.
em++ -std=c++17 -O2 -Wall -Wextra -fexceptions \
    src/wasm_api.cpp \
    -o "$OUT_DIR/cpu.js" \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=createCpuModule \
    -sENVIRONMENT=web,node \
    -sFILESYSTEM=0 \
    -sEXPORTED_FUNCTIONS="$EXPORTS" \
    -sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString

echo "Built $OUT_DIR/cpu.js and $OUT_DIR/cpu.wasm"
ls -l "$OUT_DIR"
