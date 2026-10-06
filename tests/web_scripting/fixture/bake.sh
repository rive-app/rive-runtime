#!/bin/bash
# Rebakes <name>.riv from each <name>/main.as, or only the names given. The
# other files in a fixture's folder join its project, and a rive.yaml there
# adds keys to the generated one. RIVE_BAKE_DIR bakes another folder's
# fixtures, RIVE_BAKE_FLAGS adds rive-cli flags. Needs rive-cli built with
# packages/rive-cli/build.sh, release if there is one, else debug, or the one
# RIVE_CLI names. RIVE_BAKE_OUT keeps each staged project there and writes the
# .riv beside it, leaving the committed one alone.
set -e
HERE="$(cd "${RIVE_BAKE_DIR:-$(dirname "$0")}" && pwd)"
PACKAGES="$(cd "$(dirname "$0")/../../../.." && pwd)"
CLI="$PACKAGES/rive-cli/out/release/rive"
[[ -x "$CLI" ]] || CLI="$PACKAGES/rive-cli/out/debug/rive"
CLI="${RIVE_CLI:-$CLI}"
if [[ -n "$RIVE_BAKE_OUT" ]]; then
    mkdir -p "$RIVE_BAKE_OUT"
    WORK="$(cd "$RIVE_BAKE_OUT" && pwd)"
    DEST="$WORK"
else
    WORK="$(mktemp -d)"
    trap 'rm -rf "$WORK"' EXIT
    DEST="$HERE"
fi

NAMES=("$@")
if [[ ${#NAMES[@]} -eq 0 ]]; then
    for SOURCE in "$HERE"/*/main.as; do
        NAMES+=("$(basename "$(dirname "$SOURCE")")")
    done
fi
for NAME in "${NAMES[@]}"; do
    rm -rf "$WORK/$NAME"
    cp -R "$HERE/$NAME" "$WORK/$NAME"
    # The std path resolves against the working directory, so make it
    # absolute.
    {
        printf 'name: app\nscripting: wasm\nrascStd: %s\n' \
            "$PACKAGES/rasc/animascript/std"
        cat "$HERE/$NAME/rive.yaml" 2>/dev/null || true
    } > "$WORK/$NAME/rive.yaml"
    RIVE_DEV_SKIP_AUTH=1 "$CLI" "$WORK/$NAME" \
        --once $RIVE_BAKE_FLAGS
    cp "$WORK/$NAME/build/app.riv" "$DEST/$NAME.riv"
done
