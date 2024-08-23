#!/bin/sh

set -e

if [[ -z "${DEPENDENCIES}" ]]; then
    echo "DEPENDENCIES env variable must be set. This script is usually called by other scripts."
    exit 1
fi

pushd $DEPENDENCIES
EARCUT_REPO=https://github.com/mapbox/earcut.hpp
EARCUT_STABLE_BRANCH=master

if [ ! -d earcut.hpp ]; then
    echo "Cloning earcut."
    git clone $EARCUT_REPO
fi

pushd earcut.hpp
git checkout $EARCUT_STABLE_BRANCH && git fetch && git pull
popd

popd
