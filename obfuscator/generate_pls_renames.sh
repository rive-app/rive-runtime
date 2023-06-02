set -e

# This script regenerates pls_renames.h from the PLS build.
rm -f pls_renames.h

CWD="$(pwd)"
cd ../out

# No-op the Objective-C classes, since castxml doesn't support Objective-C.
premake5 --file=premake5_pls_renderer.lua --nop-obj-c gmake2

# Use the `castxml` compiler to generate XML files with symbol names.
export CC=castxml
export CXX=castxml
export CFLAGS="--castxml-output=1 --sysroot $(xcrun --sdk macosx --show-sdk-path)"
export CXXFLAGS=$CFLAGS

# Use our own pseudo archiver that actually just pulls all the PLS symbols from XML and generates
# pls_renames.h.
export AR="python3 $CWD/generate_pls_renames.py"

make -C ../out config=release clean
make -C ../out config=release rive_pls_renderer -j8
make -C ../out config=release clean

cat $CWD/pls_renames.h
