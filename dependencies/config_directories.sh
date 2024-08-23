#!/bin/sh

# Platform specific build scripts should source directly the platform specific
# version of this scripts. If you're using an older script that tries to handle
# all platforms then call this one. We've moved to using platform specific build
# scripts where possible since most of our Windows builds diverge so far from
# the mac/linux ones. Most Mac/Linux ones can still be shared.
set -e

unameOut="$(uname -s)"
case "${unameOut}" in
Linux*) machine=linux ;;
Darwin*) machine=macosx ;;
*) machine="unhandled:${unameOut}" ;;
esac

source $(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)/${machine}/config_directories.sh
