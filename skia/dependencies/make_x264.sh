#!/bin/sh

set -e

# Based on http://www.alamtechstuffs.com/ffmpegcompile_with_x264/
# x264 has move here: https://code.videolan.org/videolan/x264
X264_REPO=https://github.com/mirror/x264

# -----------------------------
# Get & Build Skia
# -----------------------------
if [ ! -d x264 ]; then
	echo "Cloning x264."
    git clone $X264_REPO
else
    echo "Already have x264, update it."
    cd x264 && git checkout master && git fetch && git pull
    cd ..
fi

cd x264

./configure --enable-static --enable-pic --prefix=.
make;make install
cd ..