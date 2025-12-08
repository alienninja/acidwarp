#!/bin/bash
# Build Acidwarp for WebGL using Emscripten
# Make sure to run: source /path/to/emsdk/emsdk_env.sh first

set -e

echo "Building Acidwarp for WebGL..."

emcc acidwarp.c \
    ../acidwarp/bit_map.c \
    ../acidwarp/lut.c \
    ../acidwarp/palinit.c \
    ../acidwarp/rolnfade.c \
    ../acidwarp/warp_text.c \
    -I../acidwarp \
    -sUSE_SDL=2 \
    -sWASM=1 \
    -sALLOW_MEMORY_GROWTH=1 \
    -O2 \
    --shell-file shell.html \
    -o index.html

echo "Build complete! Files generated:"
echo "  - index.html"
echo "  - index.js"
echo "  - index.wasm"
echo ""
echo "To test locally: python3 -m http.server 9090"
echo "Then open: http://localhost:9090"
