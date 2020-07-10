#!/bin/bash

cd test
# premake5 clean || exit 1
premake5 gmake || exit 1
make || exit 1
for file in ./build/bin/debug/*
do
  echo testing $file
  $file
done