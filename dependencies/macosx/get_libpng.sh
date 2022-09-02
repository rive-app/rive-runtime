#!/bin/sh

set -e

if [[ -z "${DEPENDENCIES}" ]]; then
    echo "DEPENDENCIES env variable must be set. This script is usually called by other scripts."
    exit 1
fi

pushd $DEPENDENCIES
LIBPNG_REPO=https://github.com/glennrp/libpng
LIBPNG_STABLE_BRANCH=libpng16

if [ ! -d libpng ]; then
    echo "Cloning libpng."
    git clone $LIBPNG_REPO
fi

pushd libpng
git checkout $LIBPNG_STABLE_BRANCH && git fetch && git pull
mv scripts/pnglibconf.h.prebuilt pnglibconf.h
popd

ZLIB_REPO=https://github.com/madler/zlib
ZLIB_STABLE_BRANCH=master

if [ ! -d zlib ]; then
    echo "Cloning zlib."
    git clone $ZLIB_REPO
fi

pushd zlib
git checkout $ZLIB_STABLE_BRANCH && git fetch && git pull
popd

popd
