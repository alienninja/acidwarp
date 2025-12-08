#!/bin/bash
# Production build for Acidwarp WebGL
# Optimized for deployment

set -e

echo "Building Acidwarp for production..."

emcc acidwarp.c bit_map.c lut.c palinit.c rolnfade.c warp_text.c \
    -s USE_SDL=2 \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ENVIRONMENT=web \
    -s MODULARIZE=0 \
    -s EXPORTED_RUNTIME_METHODS='[]' \
    -s DISABLE_EXCEPTION_CATCHING=1 \
    -s NO_FILESYSTEM=1 \
    -s MINIMAL_RUNTIME=0 \
    -O3 \
    --closure 1 \
    --shell-file shell.html \
    -o index.html

# Show file sizes
echo ""
echo "Production build complete!"
echo "File sizes:"
ls -lh index.html index.js index.wasm 2>/dev/null | awk '{print "  " $9 ": " $5}'

echo ""
echo "Files ready for deployment:"
echo "  - index.html"
echo "  - index.js"
echo "  - index.wasm"
echo ""
echo "Upload these 3 files to your web server."
