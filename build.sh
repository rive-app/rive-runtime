#!/bin/bash
set -e

source dependencies/config_directories.sh
pushd build &>/dev/null

while getopts p: flag; do
    case "${flag}" in
    p)
        shift 2
        platform=${OPTARG}
        ;;
    \?) help ;;
    esac
done

help() {
    echo build.sh - build debug library
    echo build.sh clean - clean the build
    echo build.sh release - build release library
    echo build.sh -p ios release - build release ios library
    echo build.sh -p ios_sim release - build release ios simulator library
    echo build.sh -p android release - build release android library
    exit 1
}

# make sure argument is lowercase
OPTION="$(echo "$1" | tr '[A-Z]' '[a-z]')"

if [ "$OPTION" = 'help' ]; then
    help
else
    build() {
        echo "Building Rive for platform=$platform option=$OPTION"
        echo premake5 gmake2 --with_rive_text --with_rive_audio=system "$1"
        PREMAKE="premake5 gmake2 --with_rive_text --with_rive_audio=system $1"
        eval "$PREMAKE"
        if [ "$OPTION" = "clean" ]; then
            make clean
            make clean config=release
        elif [ "$OPTION" = "release" ]; then
            make config=release -j7
        else
            make config=debug -j7
        fi
    }

    case $platform in
    ios)
        echo "Building for iOS"
        export IOS_SYSROOT=$(xcrun --sdk iphoneos --show-sdk-path)
        build "--os=ios"
        if [ "$OPTION" = "clean" ]; then
            exit
        fi
        ;;
    ios_sim)
        export IOS_SYSROOT=$(xcrun --sdk iphonesimulator --show-sdk-path)
        build "--os=ios --variant=emulator"
        if [ "$OPTION" = "clean" ]; then
            exit
        fi
        ;;
    # Android supports ABIs via a custom platform format:
    #   e.g. 'android.x86', 'android.x64', etc.
    android*)
        echo "Building for ${platform}"
        # Extract ABI from this opt by splitting on '.' character
        #   e.g. android.x86
        IFS="." read -ra strarr <<<"$platform"
        ARCH=${strarr[1]}
        build "--os=android --arch=${ARCH}"
        ;;
    macosx)
        echo "Building for macos"
        export MACOS_SYSROOT=$(xcrun --sdk macosx --show-sdk-path)
        build "--os=macosx --variant=runtime"
        if [ "$OPTION" = "clean" ]; then
            exit
        fi
        ;;
    *)
        build
        ;;
    esac
fi

popd &>/dev/null
