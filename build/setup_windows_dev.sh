#!/bin/bash
# setup_windows_dev.sh — add Visual Studio dev tools (msbuild, fxc, cl, etc.)
# to the current bash session's PATH. Equivalent of setup_windows_dev.ps1 but
# callable from Git Bash / MSYS2 without a PowerShell detour.
#
# Usage: source setup_windows_dev.sh

if [[ "$(uname -s)" != MINGW* && "$(uname -s)" != MSYS* ]]; then
    return 0 2>/dev/null || exit 0
fi

# Already set up?
if command -v msbuild.exe &>/dev/null; then
    return 0 2>/dev/null || exit 0
fi

# Locate Visual Studio via vswhere (works for any version/edition/location).
VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
if [ ! -f "$VSWHERE" ]; then
    echo "setup_windows_dev.sh: vswhere.exe not found — is Visual Studio installed?" >&2
    return 1 2>/dev/null || exit 1
fi

VS_INSTALL_PATH=$("$VSWHERE" -latest -property installationPath 2>/dev/null | tr -d '\r')
if [ -z "$VS_INSTALL_PATH" ]; then
    echo "setup_windows_dev.sh: no Visual Studio installation found" >&2
    return 1 2>/dev/null || exit 1
fi

VS_DEVSHELL="${VS_INSTALL_PATH}\\Common7\\Tools\\Launch-VsDevShell.ps1"
VS_DEVSHELL_UNIX=$(cygpath -u "$VS_DEVSHELL" 2>/dev/null)
if [ ! -f "$VS_DEVSHELL_UNIX" ]; then
    echo "setup_windows_dev.sh: Launch-VsDevShell.ps1 not found at $VS_DEVSHELL" >&2
    return 1 2>/dev/null || exit 1
fi

# Run Launch-VsDevShell in PowerShell and capture the resulting PATH.
NEW_PATH=$(powershell -NoProfile -ExecutionPolicy Bypass -Command "
    & '${VS_DEVSHELL}' -SkipAutomaticLocation *>\$null
    \$env:Path
" 2>/dev/null | tr -d '\r')

if [ -z "$NEW_PATH" ]; then
    echo "setup_windows_dev.sh: failed to extract VS dev PATH" >&2
    return 1 2>/dev/null || exit 1
fi

# Convert Windows PATH (;-separated) to bash PATH (:-separated, unix paths).
UNIX_PATH=""
IFS=';' read -ra ENTRIES <<< "$NEW_PATH"
for entry in "${ENTRIES[@]}"; do
    unix_entry=$(cygpath -u "$entry" 2>/dev/null || echo "$entry")
    if [ -n "$UNIX_PATH" ]; then
        UNIX_PATH="$UNIX_PATH:$unix_entry"
    else
        UNIX_PATH="$unix_entry"
    fi
done

export PATH="$UNIX_PATH"

# Also set RIVE_ROOT if not already set.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
export RIVE_ROOT="${RIVE_ROOT:-$(cd "$SCRIPT_DIR/../../.." && pwd)}"
export PATH="$SCRIPT_DIR:$PATH"
