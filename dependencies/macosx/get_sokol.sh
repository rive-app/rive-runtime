#!/bin/sh

set -e

if [[ -z "${DEPENDENCIES}" ]]; then
    echo "DEPENDENCIES env variable must be set. This script is usually called by other scripts."
    exit 1
fi

pushd $DEPENDENCIES
SOKOL_REPO=https://github.com/luigi-rosso/sokol
SOKOL_STABLE_BRANCH=support_transparent_framebuffer

if [ ! -d sokol ]; then
    echo "Cloning sokol."
    git clone $SOKOL_REPO

    if [ $(arch) == arm64 ]; then
        SOKOL_SHDC=https://github.com/floooh/sokol-tools-bin/raw/6c8fc754ad73bf0db759ae44b224093290e5b26e/bin/osx_arm64/sokol-shdc
    else
        SOKOL_SHDC=https://github.com/floooh/sokol-tools-bin/raw/6c8fc754ad73bf0db759ae44b224093290e5b26e/bin/osx/sokol-shdc
    fi
    curl $SOKOL_SHDC -L -o ./bin/sokol-shdc
    chmod +x ./bin/sokol-shdc
fi

cd sokol && git checkout $SOKOL_STABLE_BRANCH && git fetch && git pull
popd
