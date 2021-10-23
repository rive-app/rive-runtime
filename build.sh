#!/bin/bash

set -e

pushd build &>/dev/null


while getopts p: flag
do
    case "${flag}" in
        p) 
            shift 2
            platform=${OPTARG}
            ;;
    esac
done

OPTION=$1

PREMAKE=(premake5 gmake2)

case $platform in 
    ios)
    echo "Building for iOS"
    export IOS_SYSROOT=$(xcrun --sdk iphoneos --show-sdk-path)
    PREMAKE+=( --os=ios )
    ;;
esac

if [ "$OPTION" = 'help' ]
then
    echo build.sh - build debug library
    echo build.sh clean - clean the build
    echo build.sh release - build release library 
    echo build.sh release -p ios - build release ios library 
elif [ "$OPTION" = "clean" ]
then
    echo Cleaning project ...
    "${PREMAKE[@]}"
    make clean
    make clean config=release
elif [ "$OPTION" = "release" ]
then
echo "RELEASE!"
    "${PREMAKE[@]}"
    CFLAGS="$CFLAGS" make config=release -j7
else
    "${PREMAKE[@]}"
    CFLAGS="$CFLAGS" make -j7
fi

popd &>/dev/null
