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
  $PREMAKE --scripts=../../build clean || exit 1
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

$PREMAKE --scripts=../../build gmake2 || exit 1
make -j7 || exit 1

for file in ./build/bin/debug/*; do
  echo testing $file
  $UTILITY $file "$1"
done

popd &>/dev/null
