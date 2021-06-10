#!/bin/sh

set -e

# libzip requires CMake

LIBZIP_REPO=https://github.com/nih-at/libzip/
LIBZIP_RELEASE=v1.7.3

# -----------------------------
# Get & Build libzip
# -----------------------------
if [ ! -d libzip ]; then
	echo "Cloning libzip."
    git clone $LIBZIP_REPO --branch $LIBZIP_RELEASE
else
    echo "Already have libzip, update it."
    # cd libzip && git fetch && git merge master
    cd libzip
    LATEST_TAG=$(git describe --tags `git rev-list --tags --max-count=1`)
    git checkout $LATEST_TAG
    cd ..
fi

mkdir -p libzip_build
cd libzip_build
cmake ../libzip -DBUILD_SHARED_LIBS=OFF
make