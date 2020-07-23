#!/bin/bash

OUTPUT_DIR=bin/debug

pushd build &>/dev/null

# make the output directory if it dont's exist
mkdir -p $OUTPUT_DIR

emcc -O3 \
    --bind \
    -o $OUTPUT_DIR/rive_wasm.js \
    -s FORCE_FILESYSTEM=0 \
    -s MODULARIZE=1 \
    -s NO_EXIT_RUNTIME=1 \
    -s STRICT=1 \
    -s WARN_UNALIGNED=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s DISABLE_EXCEPTION_CATCHING=1 \
    -s WASM=1 \
    -s EXPORT_NAME="RiveWASM" \
    -DSINGLE \
    -DANSI_DECLARATORS \
    -DTRILIBRARY \
    -Wno-c++17-extensions \
    -fno-rtti \
    -fno-exceptions \
    -I../include \
    --no-entry \
    ../src/*/*.cpp

popd &>/dev/null