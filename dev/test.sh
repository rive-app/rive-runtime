#!/bin/bash
pushd test &>/dev/null

OPTION=$1
UTILITY=

if [ "$OPTION" = "help" ]
then
    echo test.sh - run the tests
    echo test.sh clean - clean and run the tests
    exit
elif [ "$OPTION" = "clean" ]
then
    echo Cleaning project ...
    premake5 clean || exit 1
    shift
elif [ "$OPTION" = "memory" ]
then
  echo Will perform memory checks...
  UTILITY='valgrind --leak-check=full'
  shift
elif [ "$OPTION" = "debug" ]
then
  echo Starting debugger...
  UTILITY='lldb'
  shift
fi

premake5 gmake2 || exit 1
make -j7 || exit 1
for file in ./build/bin/debug/*; do
  echo testing $file
  $UTILITY $file "$1"
done

popd &>/dev/null