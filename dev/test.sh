#!/bin/bash
set -e

source ../dependencies/config_directories.sh
source setup_premake.sh

pushd test &>/dev/null

OPTION=$1
UTILITY=

if [ "$OPTION" = "help" ]; then
  echo test.sh - run the tests
  echo test.sh clean - clean and run the tests
  exit
elif [ "$OPTION" = "clean" ]; then
  echo Cleaning project ...
  rm -fR out
  shift
elif [ "$OPTION" = "memory" ]; then
  echo Will perform memory checks...
  UTILITY='leaks --atExit --'
  shift
elif [ "$OPTION" = "debug" ]; then
  echo Starting debugger...
  UTILITY='lldb'
  shift
fi

unameOut="$(uname -s)"
case "${unameOut}" in
Linux*) NUM_CORES=$(grep -c processor /proc/cpuinfo) ;;
Darwin*) NUM_CORES=$(($(sysctl -n hw.physicalcpu) + 1)) ;;
MINGW*) NUM_CORES=$NUMBER_OF_PROCESSORS ;;
*) NUM_CORES=4 ;;
esac

$PREMAKE --scripts=../../build gmake2 --with_rive_tools --with_rive_text --with_rive_audio=external --config=debug --out=out/debug
make -C out/debug -j$NUM_CORES

$UTILITY out/debug/tests "$1"

popd &>/dev/null
