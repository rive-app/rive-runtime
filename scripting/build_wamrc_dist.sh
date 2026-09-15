#!/bin/bash
# Builds the distributable wamrc: static against a trimmed LLVM (AArch64 +
# X86 backends only), no dylib dependencies, shippable beside the editor.
# Usage: build_wamrc_dist.sh <work_dir> [wamr_source_dir]
set -e

WORK="${1:?work dir required}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Default to the vendored, patched clone two packages over; CI passes its own.
WAMR_SRC="${2:-$(ls -d "$SCRIPT_DIR"/../../rive-cli/dependencies/bytecodealliance_wasm-micro-runtime_WAMR-* 2>/dev/null | head -1)}"
[ -d "$WAMR_SRC/wamr-compiler" ] || {
    echo "wamr source not found: $WAMR_SRC" >&2
    exit 1
}

mkdir -p "$WORK"
cd "$WORK"

# WAMR 2.4.x builds against LLVM release/18.x.
if [ ! -d llvm ]; then
    git clone --depth 1 --branch release/18.x \
        https://github.com/llvm/llvm-project.git llvm
fi

cmake -S llvm/llvm -B llvm-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_TARGETS_TO_BUILD="AArch64;X86" \
    -DLLVM_BUILD_LLVM_DYLIB=OFF \
    -DLLVM_ENABLE_BINDINGS=OFF \
    -DLLVM_ENABLE_LIBEDIT=OFF \
    -DLLVM_ENABLE_TERMINFO=OFF \
    -DLLVM_ENABLE_ZSTD=OFF \
    -DLLVM_ENABLE_ZLIB=ON \
    -DLLVM_INCLUDE_BENCHMARKS=OFF \
    -DLLVM_INCLUDE_DOCS=OFF \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_INCLUDE_UTILS=OFF \
    -DLLVM_APPEND_VC_REV=OFF \
    -DLLVM_TOOL_LTO_BUILD=OFF \
    -DLLVM_TOOL_REMARKS_SHLIB_BUILD=OFF \
    -DLLVM_OPTIMIZED_TABLEGEN=ON
cmake --build llvm-build

cmake -S "$WAMR_SRC/wamr-compiler" -B wamrc-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DWAMR_BUILD_WITH_CUSTOM_LLVM=1 \
    -DLLVM_DIR="$PWD/llvm-build/lib/cmake/llvm"
cmake --build wamrc-build

strip -x wamrc-build/wamrc -o wamrc
# The dist contract: no non-system dylibs, or it cannot ship in a bundle.
if otool -L wamrc | grep -vE "wamrc:|/usr/lib/" | grep -q .; then
    echo "wamrc links non-system dylibs:" >&2
    otool -L wamrc >&2
    exit 1
fi
./wamrc --version
ls -la wamrc | awk '{print "dist wamrc size:", $5, "bytes"}'
