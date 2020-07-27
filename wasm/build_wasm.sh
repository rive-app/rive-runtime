#!/bin/bash

OUTPUT_DIR=bin/debug

mkdir -p build
pushd build &>/dev/null

# make the output directory if it dont's exist
mkdir -p $OUTPUT_DIR

em++ -O3 \
    --bind \
    --closure 1 \
    -o $OUTPUT_DIR/rive_wasm.js \
    -s ASSERTIONS=0 \
    -s FORCE_FILESYSTEM=0 \
    -s MODULARIZE=1 \
    -s NO_EXIT_RUNTIME=1 \
    -s STRICT=1 \
    -s WARN_UNALIGNED=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s DISABLE_EXCEPTION_CATCHING=1 \
    -s WASM=1 \
    -s EXPORT_NAME="RiveWASM" \
    -s LLD_REPORT_UNDEFINED \
    -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 \
    -DSINGLE \
    -DANSI_DECLARATORS \
    -Wno-c++17-extensions \
    -fno-exceptions \
    -fno-rtti \
    -fno-unwind-tables \
    -I../../rive/include \
    --no-entry \
    ../../rive/src/*/*.cpp \
    ../../rive/src/*.cpp \
    ../../rive/src/core/field_types/*.cpp \
    ../../rive/src/shapes/paint/*.cpp \
    ../src/bindings.cpp

popd &>/dev/null