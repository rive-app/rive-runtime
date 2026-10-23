#!/bin/bash
# Rebakes the stress fixtures here, or only the names given, without the
# debugger's line probes, since the browser lane times them.
RIVE_BAKE_DIR="$(dirname "$0")" RIVE_BAKE_FLAGS=--optimize \
    exec "$(dirname "$0")/../fixture/bake.sh" "$@"
