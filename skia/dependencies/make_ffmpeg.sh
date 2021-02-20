#!/bin/sh

set -e

FFMPEG_REPO=https://github.com/FFmpeg/FFmpeg
FFMPEG_BRANCH=release/4.3

# -----------------------------
# Get & Build FFMPEG
# -----------------------------
if [ ! -d ffmpeg ]; then
	echo "Cloning ffmpeg."
    git clone $FFMPEG_REPO
    git checkout $FFMPEG_BRANCH
else
    echo "Already have FFmpeg, update it."
    cd ffmpeg && git checkout $FFMPEG_BRANCH && git fetch && git pull
    cd ..
fi

# uh build it...
# forget that for now...
# brew install ffmpeg
# brew install libav