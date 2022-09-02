#!/bin/sh

set -e

if [[ -z "${DEPENDENCIES}" ]]; then
    echo "DEPENDENCIES env variable must be set. This script is usually called by other scripts."
    exit 1
fi
pushd $DEPENDENCIES

if [ ! -d harfbuzz ]; then
    echo "Cloning Harfbuzz."
    git clone https://github.com/harfbuzz/harfbuzz
    cd harfbuzz
    git checkout 858570b1d9912a1b746ab39fbe62a646c4f7a5b1 .
fi
