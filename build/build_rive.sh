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
    MINGW*)
        HOST_MACHINE="windows"
        NUM_CORES=$NUMBER_OF_PROCESSORS
        # Try to find MSBuild.exe
        export PATH="$PATH:$PROGRAMFILES/Microsoft Visual Studio/2022/Enterprise/Msbuild/Current/Bin"
        export PATH="$PATH:$PROGRAMFILES/Microsoft Visual Studio/2022/Community/Msbuild/Current/Bin"
        ;;
    Linux*)
        HOST_MACHINE="linux"
        NUM_CORES=$(grep -c processor /proc/cpuinfo)
        ;;
esac

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
    RIVE_PREMAKE_FILE="${RIVE_PREMAKE_FILE:-./premake5.lua}"
    if [ ! -f "$RIVE_PREMAKE_FILE" ]; then
        echo "Premake file "$RIVE_PREMAKE_FILE" not found"
        exit -1
    fi

    # Only use default arguments if RIVE_PREMAKE_ARGS is unset (not just empty).
    if [ -z "${RIVE_PREMAKE_ARGS+null_detector_string}" ]; then
        RIVE_PREMAKE_ARGS="--with_rive_text --with_rive_layout"
    fi

    while [[ $# -gt 0 ]]; do
        case "$1" in
            "release") RIVE_CONFIG="${RIVE_CONFIG:-release}" ;;
            "ios") RIVE_OS="${RIVE_OS:-ios}" ;;
            "iossim")
                RIVE_OS=${RIVE_OS:-ios}
                RIVE_VARIANT="${RIVE_VARIANT:-emulator}"
                RIVE_ARCH="${RIVE_ARCH:-universal}" # The simulator requires universal builds.
                ;;
            "android") RIVE_OS="${RIVE_OS:-android}" ;;
            "arm") RIVE_ARCH="${RIVE_ARCH:-arm}" ;;
            "arm64") RIVE_ARCH="${RIVE_ARCH:-arm64}" ;;
            "x64") RIVE_ARCH="${RIVE_ARCH:-x64}" ;;
            "universal") RIVE_ARCH="${RIVE_ARCH:-universal}" ;;
            "wasm") RIVE_ARCH="${RIVE_ARCH:-wasm}" ;;
            "ninja") RIVE_BUILD_SYSTEM="${RIVE_BUILD_SYSTEM:-ninja}" ;;
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

    if [[ "$HOST_MACHINE" = "windows" ]]; then
        RIVE_BUILD_SYSTEM="${RIVE_BUILD_SYSTEM:-vs2022}"
    else
        RIVE_BUILD_SYSTEM="${RIVE_BUILD_SYSTEM:-gmake2}"
    fi

    RIVE_PREMAKE_ARGS="$RIVE_BUILD_SYSTEM --file=$RIVE_PREMAKE_FILE --config=$RIVE_CONFIG --out=$RIVE_OUT $RIVE_PREMAKE_ARGS"
    if [ ! -z "$RIVE_OS" ]; then RIVE_PREMAKE_ARGS="$RIVE_PREMAKE_ARGS --os=$RIVE_OS"; fi
    if [ ! -z "$RIVE_VARIANT" ]; then RIVE_PREMAKE_ARGS="$RIVE_PREMAKE_ARGS --variant=$RIVE_VARIANT"; fi
    if [ ! -z "$RIVE_ARCH" ]; then RIVE_PREMAKE_ARGS="$RIVE_PREMAKE_ARGS --arch=$RIVE_ARCH"; fi

    if [[ "$RIVE_CLEAN" = true ]]; then
        rm -fr "./$RIVE_OUT"
    fi
fi

mkdir -p "$SCRIPT_DIR/dependencies"
pushd "$SCRIPT_DIR/dependencies" > /dev/null

# Setup premake5.
if [ ! -d premake-core ]; then
    echo Building Premake...
    git clone https://github.com/premake/premake-core.git
    pushd premake-core > /dev/null
    case "$HOST_MACHINE" in
        mac_arm64) make -f Bootstrap.mak osx PLATFORM=ARM ;;
        mac_x64) make -f Bootstrap.mak osx ;;
        windows) ./Bootstrap.bat ;;
        *) make -f Bootstrap.mak linux ;;
    esac
    popd > /dev/null
fi
export PATH="$SCRIPT_DIR/dependencies/premake-core/bin/release/:$PATH"
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
if [[ $RIVE_ARCH = "wasm" ]]; then
    if [ ! -d emsdk ]; then
        git clone https://github.com/emscripten-core/emsdk.git
        emsdk/emsdk install 3.1.61
        emsdk/emsdk activate 3.1.61
    fi
    source emsdk/emsdk_env.sh
fi

popd > /dev/null # leave "$SCRIPT_DIR/dependencies"

if [ -d "$RIVE_OUT" ]; then
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
    vs2022)
        for TARGET in $@; do
            RIVE_MSVC_TARGETS="$RIVE_MSVC_TARGETS -t:$TARGET"
        done
        echo msbuild.exe "./$RIVE_OUT/rive.sln" -p:UseMultiToolTask=true -m:$NUM_CORES $RIVE_MSVC_TARGETS
        msbuild.exe "./$RIVE_OUT/rive.sln" -p:UseMultiToolTask=true -m:$NUM_CORES $RIVE_MSVC_TARGETS
        ;;
    *)
        print "Unsupported buildsystem $RIVE_BUILD_SYSTEM"
        exit -1
        ;;
esac
