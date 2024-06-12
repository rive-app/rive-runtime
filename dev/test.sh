#!/bin/bash
set -e

unameOut="$(uname -s)"
case "${unameOut}" in
Linux*) machine=linux ;;
Darwin*) machine=macosx ;;
MINGW*) machine=windows ;;
*) machine="unhandled:${unameOut}" ;;
esac

CONFIG=debug

for var in "$@"; do
  if [[ $var = "release" ]]; then
    CONFIG=release
  elif [ "$var" = "memory" ]; then
    echo Will perform memory checks...
    UTILITY='leaks --atExit --'
    shift
  elif [ "$var" = "lldb" ]; then
    echo Starting debugger...
    UTILITY='lldb'
    shift
  fi
done

if [[ ! -f "dependencies/bin/premake5" ]]; then
  mkdir -p dependencies/bin
  pushd dependencies
  if [[ $machine = "macosx" ]]; then
    # v5.0.0-beta2 doesn't support apple silicon properly, update the branch
    # once a stable one is avaialble that supports it
    git clone --depth 1 --branch master https://github.com/premake/premake-core.git
    pushd premake-core
    if [[ $LOCAL_ARCH == "arm64" ]]; then
      PREMAKE_MAKE_ARCH=ARM
    else
      PREMAKE_MAKE_ARCH=x86
    fi
    make -f Bootstrap.mak osx PLATFORM=$PREMAKE_MAKE_ARCH
    cp bin/release/* ../bin
    popd
  elif [[ $machine = "windows" ]]; then
    pushd bin
    curl https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-windows.zip -L -o premake_windows.zip
    unzip premake_windows.zip
    rm premake_windows.zip
    popd
  fi
  popd
fi

export PREMAKE=$PWD/dependencies/bin/premake5

pushd test

for var in "$@"; do
  if [[ $var = "clean" ]]; then
    echo 'Cleaning...'
    rm -fR out
  fi
done

mkdir -p out

if [[ $machine = "macosx" ]]; then
  TARGET=gmake2
elif [[ $machine = "windows" ]]; then
  TARGET=vs2022
fi

pushd ../../
RUNTIME=$PWD
popd

export PREMAKE_PATH="$RUNTIME/dependencies/export-compile-commands":"$RUNTIME/build":"$PREMAKE_PATH"
PREMAKE_COMMANDS="--with_rive_text --with_rive_audio=external --with_rive_layout --config=$CONFIG"

out_dir() {
  echo "out/$CONFIG"
}
if [[ $machine = "macosx" ]]; then
  OUT_DIR="$(out_dir)"
  $PREMAKE $TARGET $PREMAKE_COMMANDS --out=$OUT_DIR
  pushd $OUT_DIR
  make -j$(($(sysctl -n hw.physicalcpu) + 1))
  popd
  $UTILITY $OUT_DIR/tests
elif [[ $machine = "windows" ]]; then
  if [[ -f "$PROGRAMFILES/Microsoft Visual Studio/2022/Enterprise/Msbuild/Current/Bin/MSBuild.exe" ]]; then
    export MSBUILD="$PROGRAMFILES/Microsoft Visual Studio/2022/Enterprise/Msbuild/Current/Bin/MSBuild.exe"
  elif [[ -f "$PROGRAMFILES/Microsoft Visual Studio/2022/Community/Msbuild/Current/Bin/MSBuild.exe" ]]; then
    export MSBUILD="$PROGRAMFILES/Microsoft Visual Studio/2022/Community/Msbuild/Current/Bin/MSBuild.exe"
  fi
  OUT_DIR="$(out_dir)"
  echo $PREMAKE $TARGET $PREMAKE_COMMANDS --out=$OUT_DIR
  ls -l
  $PREMAKE $TARGET $PREMAKE_COMMANDS --out=$OUT_DIR
  pushd $OUT_DIR
  "$MSBUILD" rive.sln -m:$NUMBER_OF_PROCESSORS
  popd
  $OUT_DIR/tests.exe
fi

popd
