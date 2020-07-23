#!/bin/bash
cd test
# premake5 clean || exit 1
premake5 gmake || exit 1
make -j7 || exit 1
for file in ./build/bin/debug/*; do
  echo testing $file
  $file "$1"
done
