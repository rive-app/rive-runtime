#!/bin/bash

# build_rive.sh: build any standard Rive sub-project.
#
# To use this script:
#
#   1) add packages/runtime/build to $PATH
#   2) cd to directory with a premake5.lua
#   3) run build_rive.sh with desired flags (in any order)
#
# Build for the host machine:
#
#   build_rive.sh                      # debug build
#   build_rive.sh release              # release build
#   build_rive.sh release clean        # clean, followed by a release build
#   build_rive.sh xcode                # generate and build an xcode workspace
#   build_rive.sh ninja                # use ninja (experimental) for a debug build
#   build_rive.sh ninja release        # use ninja (experimental) for a release build
#   build_rive.sh ninja --with_vulkan  # extra parameters get forwarded to premake
#
# Generate compile_commands.json for code completion engines:
#
#   build_rive.sh compdb                        # code completion for a host build
#   build_rive.sh compdb android --with_vulkan  # code completion for android with vulkan enabled
#
# Specify build targets after "--":
#
#   build_rive.sh -- gms goldens
#   build_rive.sh ninja release -- imagediff
#
# Build for android:
#
#   build_rive.sh android            # debug build, defaults to arm64
#   build_rive.sh ninja android arm  # release arm32 build using ninja
#
# Build for ios:
#
#   build_rive.sh ios          # debug build, arm64 only
#   build_rive.sh ios release  # release build, arm64 only
#   build_rive.sh iossim       # debug build, defaults to "universal" (x64 and arm64) architecture
#
# Build for wasm:
#
#   build_rive.sh ninja release wasm                # release build
#   build_rive.sh ninja release wasm --with-webgpu  # release build, also enable webgpu
#
# Incremental builds:
#
#   build_rive.sh                            # initial build
#   build_rive.sh                            # any repeat command does an incremental build
#   build_rive.sh --with_vulkan <- FAILS!!   # a repeat build with different arguments fails
#   build_rive.sh clean --with_vulkan        # a clean build with different arguments is OK
#   build_rive.sh rebuild out/debug  # use "rebuild" on a specific OUT directory to relaunch the
#                                    # build with whatever args it got configured with initially
#   build_rive.sh rebuild out/debug gms goldens  # args after OUT get forwarded to the buildsystem

set -e
set -o pipefail

# Detect where build_rive.sh is located on disk.
# https://stackoverflow.com/questions/59895/how-do-i-get-the-directory-where-a-bash-script-is-located-from-within-the-script
SOURCE=${BASH_SOURCE[0]}
while [ -L "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
    DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
    SOURCE=$(readlink "$SOURCE")
    # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
    [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE
done
SCRIPT_DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

case "$(uname -s)" in
    Darwin*)
        if [[ $(arch) = "arm64" ]]; then
            HOST_MACHINE="mac_arm64"
        else
            HOST_MACHINE="mac_x64"
        fi
        NUM_CORES=$(($(sysctl -n hw.physicalcpu) + 1))
        ;;
    MINGW*|MSYS*)
        HOST_MACHINE="windows"
        NUM_CORES=$NUMBER_OF_PROCESSORS
        ;;
    Linux*)
        HOST_MACHINE="linux"
        NUM_CORES=$(grep -c processor /proc/cpuinfo)
        ;;
esac
RIVE_NO_BUILD=false
# don't build if all we want to do is re run premake for shader generation for example.
if [[ "$1" = "nobuild" ]]; then
    RIVE_NO_BUILD=true
    shift
fi

if [[ "$1" = "rebuild" ]]; then
    # Load args from an existing build.
    RIVE_OUT=$2
    shift
    shift

    if [ ! -d $RIVE_OUT ]; then
        echo "OUT directory '$RIVE_OUT' not found."
        exit -1
    fi

    ARGS_FILE=$RIVE_OUT/.rive_premake_args
    if [ ! -f $ARGS_FILE ]; then
        echo "Premake args file '$ARGS_FILE' not found."
        exit -1
    fi

    RIVE_PREMAKE_ARGS="$(< $ARGS_FILE)"
    RIVE_BUILD_SYSTEM="$(awk '{print $1}' $ARGS_FILE)" # RIVE_BUILD_SYSTEM is the first word in the args.
    if grep -e "--arch=" $ARGS_FILE > /dev/null; then
        RIVE_ARCH="$(sed -r 's/^.*--arch=([^ ]*).*$/\1/' $ARGS_FILE)"
    fi
else
    # New build. Parse arguments into premake options.

    # Only use default arguments if RIVE_PREMAKE_ARGS is unset (not just empty).
    if [ -z "${RIVE_PREMAKE_ARGS+null_detector_string}" ]; then
        RIVE_PREMAKE_ARGS="--with_rive_text --with_rive_layout"
    fi

    while [[ $# -gt 0 ]]; do
        case "$1" in
            "debug") RIVE_CONFIG="${RIVE_CONFIG:-debug}" ;;
            "release") RIVE_CONFIG="${RIVE_CONFIG:-release}" ;;
            "xros") 
                RIVE_OS="${RIVE_OS:-ios}"
                RIVE_VARIANT="${RIVE_VARIANT:-xros}"
                ;;
            "xrsimulator")
                RIVE_OS=${RIVE_OS:-ios}
                RIVE_VARIANT="${RIVE_VARIANT:-xrsimulator}"
                RIVE_ARCH="${RIVE_ARCH:-universal}" # The simulator requires universal builds.
                ;;
            "appletvos") 
                RIVE_OS="${RIVE_OS:-ios}"
                RIVE_VARIANT="${RIVE_VARIANT:-appletvos}"
                ;;
            "appletvsimulator")
                RIVE_OS=${RIVE_OS:-ios}
                RIVE_VARIANT="${RIVE_VARIANT:-appletvsimulator}"
                RIVE_ARCH="${RIVE_ARCH:-universal}" # The simulator requires universal builds.
                ;;
            "ios") RIVE_OS="${RIVE_OS:-ios}" ;;
            "iossim")
                RIVE_OS=${RIVE_OS:-ios}
                RIVE_VARIANT="${RIVE_VARIANT:-emulator}"
                RIVE_ARCH="${RIVE_ARCH:-universal}" # The simulator requires universal builds.
                ;;
            "android") RIVE_OS="${RIVE_OS:-android}" ;;
            "arm64") RIVE_ARCH="${RIVE_ARCH:-arm64}" ;;
            "arm") RIVE_ARCH="${RIVE_ARCH:-arm}" ;;
            "x64") RIVE_ARCH="${RIVE_ARCH:-x64}" ;;
            "x86") RIVE_ARCH="${RIVE_ARCH:-x86}" ;;
            "universal") RIVE_ARCH="${RIVE_ARCH:-universal}" ;;
            "wasm") RIVE_ARCH="${RIVE_ARCH:-wasm}" ;;
            "js") RIVE_ARCH="${RIVE_ARCH:-js}" ;;
            "ninja") RIVE_BUILD_SYSTEM="${RIVE_BUILD_SYSTEM:-ninja}" ;;
            "xcode") RIVE_BUILD_SYSTEM="${RIVE_BUILD_SYSTEM:-xcode4}" ;;
            "clean") RIVE_CLEAN="${RIVE_CLEAN:-true}" ;;
            "compdb")
                RIVE_BUILD_SYSTEM="${RIVE_BUILD_SYSTEM:-export-compile-commands}"
                RIVE_OUT="${RIVE_OUT:-out/compdb}"
                ;;
            "--")
                shift
                break
                ;;
            *) RIVE_PREMAKE_ARGS="$RIVE_PREMAKE_ARGS $1" ;;
        esac
        shift
    done

    # Android requires an architecture. Default to arm64 if not specified.
    if [[ $RIVE_OS = "android" ]]; then
        RIVE_ARCH="${RIVE_ARCH:-arm64}"
    fi

    RIVE_CONFIG="${RIVE_CONFIG:-debug}"

    if [ -z "$RIVE_OUT" ]; then
        RIVE_OUT="$RIVE_CONFIG"
        if [ ! -z "$RIVE_ARCH" ]; then
            RIVE_OUT="${RIVE_ARCH}_$RIVE_OUT"
        fi
        if [[ $RIVE_VARIANT = "emulator" ]]; then
            RIVE_OUT="${RIVE_OS}sim_$RIVE_OUT"
        elif [ ! -z "$RIVE_OS" ]; then
            RIVE_OUT="${RIVE_OS}_$RIVE_OUT"
        fi
        RIVE_OUT="out/$RIVE_OUT"
    fi

    if [[ $RIVE_OS = "android" ]] || [[ $RIVE_ARCH = "wasm" ]] || [[ $RIVE_ARCH = "js" ]]; then
        RIVE_BUILD_SYSTEM="${RIVE_BUILD_SYSTEM:-ninja}"
    elif [[ "$HOST_MACHINE" = "windows" ]]; then
        RIVE_BUILD_SYSTEM="${RIVE_BUILD_SYSTEM:-vs2022}"
    else
        RIVE_BUILD_SYSTEM="${RIVE_BUILD_SYSTEM:-gmake2}"
    fi

    RIVE_PREMAKE_ARGS="$RIVE_BUILD_SYSTEM --config=$RIVE_CONFIG --out=$RIVE_OUT $RIVE_PREMAKE_ARGS"
    if [[ $RIVE_OS = "android" ]]; then
        # Premake stopped supporting "--os=android".
        # Use our own custom "--for_android" flag instead.
        RIVE_PREMAKE_ARGS="$RIVE_PREMAKE_ARGS --for_android"
    elif [ ! -z "$RIVE_OS" ]; then
        RIVE_PREMAKE_ARGS="$RIVE_PREMAKE_ARGS --os=$RIVE_OS"
    fi
    if [ ! -z "$RIVE_VARIANT" ]; then RIVE_PREMAKE_ARGS="$RIVE_PREMAKE_ARGS --variant=$RIVE_VARIANT"; fi
    if [ ! -z "$RIVE_ARCH" ]; then RIVE_PREMAKE_ARGS="$RIVE_PREMAKE_ARGS --arch=$RIVE_ARCH"; fi

    if [[ "$RIVE_CLEAN" = true ]]; then
        rm -fr "./$RIVE_OUT"
    fi
fi

mkdir -p "$SCRIPT_DIR/dependencies"
pushd "$SCRIPT_DIR/dependencies" > /dev/null

# Add premake5 to the $PATH.
# Install premake5 to a specific directory based on our current tag, to make
# sure we rebuild if this script runs for a different tag.
RIVE_PREMAKE_TAG="${RIVE_PREMAKE_TAG:-v5.0.0-beta7}"
PREMAKE_INSTALL_DIR="$SCRIPT_DIR/dependencies/premake-core/bin/${RIVE_PREMAKE_TAG}_release"
if [ ! -f "$PREMAKE_INSTALL_DIR/premake5" ]; then
    echo Building Premake...
    rm -fr premake-core # Wipe out a prior checkout if it exists without a premake5 binary.
    git clone --depth 1 --branch $RIVE_PREMAKE_TAG https://github.com/premake/premake-core.git
    pushd premake-core > /dev/null
    case "$HOST_MACHINE" in
        mac_arm64) make -f Bootstrap.mak osx PLATFORM=ARM ;;
        mac_x64) make -f Bootstrap.mak osx ;;
        windows) ./Bootstrap.bat ;;
        *) make -f Bootstrap.mak linux ;;
    esac
    cp -r bin/release $PREMAKE_INSTALL_DIR
    popd > /dev/null
fi
export PATH="$PREMAKE_INSTALL_DIR:$PATH"

# Add Rive's build scripts to the premake path.
export PREMAKE_PATH="$SCRIPT_DIR"

# Setup premake-ninja.
if [[ $RIVE_BUILD_SYSTEM = "ninja" ]]; then
    if [ ! -d premake-ninja ]; then
        git clone --branch rive_modifications https://github.com/rive-app/premake-ninja.git
    fi
    export PREMAKE_PATH="$SCRIPT_DIR/dependencies/premake-ninja:$PREMAKE_PATH"
fi

# Setup premake-export-compile-commands.
if [[ $RIVE_BUILD_SYSTEM = "export-compile-commands" ]]; then
    if [ ! -d premake-export-compile-commands ]; then
        git clone --branch more_cpp_support https://github.com/rive-app/premake-export-compile-commands.git
    fi
    export PREMAKE_PATH="$SCRIPT_DIR/dependencies/premake-export-compile-commands:$PREMAKE_PATH"
fi

# Setup emscripten.
if [[ $RIVE_ARCH = "wasm" ]] || [[ $RIVE_ARCH = "js" ]]; then
    RIVE_EMSDK_VERSION="${RIVE_EMSDK_VERSION:-3.1.61}"
    # An RIVE_EMSDK_VERSION value of "none" means to just use whatever
    # environment is already set up.
    if [[ "$RIVE_EMSDK_VERSION" != "none" ]]; then
        if [ ! -d "emsdk_${RIVE_EMSDK_VERSION}" ]; then
            echo Installing emsdk ${RIVE_EMSDK_VERSION}...
            git clone https://github.com/emscripten-core/emsdk.git emsdk_${RIVE_EMSDK_VERSION}
            "emsdk_${RIVE_EMSDK_VERSION}/emsdk" install ${RIVE_EMSDK_VERSION}
            "emsdk_${RIVE_EMSDK_VERSION}/emsdk" activate ${RIVE_EMSDK_VERSION}
        fi
        source "emsdk_${RIVE_EMSDK_VERSION}/emsdk_env.sh"
    fi
fi

popd > /dev/null # leave "$SCRIPT_DIR/dependencies"

if [[ -d "$RIVE_OUT" && "$RIVE_NO_BUILD" = false ]]; then
    if [[ "$RIVE_PREMAKE_ARGS" != "$(< $RIVE_OUT/.rive_premake_args)" ]]; then
        echo "error: premake5 arguments for current build do not match previous arguments"
        echo "  previous command: premake5 $(< $RIVE_OUT/.rive_premake_args)"
        echo "   current command: premake5 $RIVE_PREMAKE_ARGS"
        echo "If you wish to overwrite the existing build, please use 'clean'"
        exit -1
    fi
else
    mkdir -p "$RIVE_OUT"
    echo "$RIVE_PREMAKE_ARGS" > "$RIVE_OUT/.rive_premake_args"
fi

echo premake5 $RIVE_PREMAKE_ARGS
premake5 $RIVE_PREMAKE_ARGS | grep -v '^Done ([1-9]*ms).$'

if [[ "$RIVE_NO_BUILD" = true ]]; then
    echo "Not building as nobuild was specified"
    exit 0
fi

case "$RIVE_BUILD_SYSTEM" in
    export-compile-commands)
        rm -f ./compile_commands.json
        cp $RIVE_OUT/compile_commands/default.json compile_commands.json
        wc ./compile_commands.json
        ;;
    gmake2)
        echo make -C $RIVE_OUT -j$NUM_CORES $@
        make -C $RIVE_OUT -j$NUM_CORES $@
        ;;
    ninja)
        echo ninja -C $RIVE_OUT $@
        ninja -C $RIVE_OUT $@
        ;;
    xcode4)
        if [[ $# = 0 ]]; then
            echo 'No targets specified for xcode: Attempting to grok them from "xcodebuild -list".'
            XCODE_SCHEMES=$(for f in $(xcodebuild -list -workspace $RIVE_OUT/rive.xcworkspace | grep '^        '); do printf " $f"; done)
            echo "  -> grokked:$XCODE_SCHEMES"
        else
            XCODE_SCHEMES="$@"
        fi
        for SCHEME in $XCODE_SCHEMES; do
            echo xcodebuild -workspace $RIVE_OUT/rive.xcworkspace -scheme $SCHEME
            xcodebuild -workspace $RIVE_OUT/rive.xcworkspace -scheme $SCHEME
        done
        ;;
    vs2022)
        for TARGET in $@; do
            MSVC_TARGETS="$MSVC_TARGETS -t:$TARGET"
        done
        echo msbuild.exe "./$RIVE_OUT/rive.sln" $MSVC_TARGETS
        msbuild.exe "./$RIVE_OUT/rive.sln" $MSVC_TARGETS
        ;;
    *)
        echo "Unsupported buildsystem $RIVE_BUILD_SYSTEM"
        exit -1
        ;;
esac
