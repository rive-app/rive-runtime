#!/bin/bash

OUTPUT_DIR=bin/release

mkdir -p build
pushd build &>/dev/null

# make the output directory if it dont's exist
mkdir -p $OUTPUT_DIR

em++ -Oz --js-opts 0 -g1 \
    --closure 0 \
    --bind \
    -o $OUTPUT_DIR/rive.js \
    -s ASSERTIONS=0 \
    -s FORCE_FILESYSTEM=0 \
    -s MODULARIZE=1 \
    -s NO_EXIT_RUNTIME=1 \
    -s STRICT=1 \
    -s WARN_UNALIGNED=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s DISABLE_EXCEPTION_CATCHING=1 \
    -s WASM=1 \
    -s EXPORT_NAME="Rive" \
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
    --post-js ../js/marker.js \
    ../../rive/src/*/*.cpp \
    ../../rive/src/*.cpp \
    ../../rive/src/core/field_types/*.cpp \
    ../../rive/src/shapes/paint/*.cpp \
    ../src/bindings.cpp

awk 'NR==FNR { a[n++]=$0; next }
/console\.log\("--REPLACE WITH RENDERING CODE--"\);/ { for (i=0;i<n;++i) print a[i]; next }
1' ../js/renderer.js ./bin/release/rive.js >./bin/release/rive_combined.js

if ! command -v terser &>/dev/null; then
    npm install terser -g
fi
terser --compress --mangle -o ./bin/release/rive.min.js -- ./bin/release/rive_combined.js
popd &>/dev/null
