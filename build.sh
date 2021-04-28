#!/bin/bash

pushd build &>/dev/null

OPTION=$1

if [ "$OPTION" = 'help' ]
then
    echo build.sh - build debug library
    echo build.sh clean - clean the build
    echo build.sh release - build release library 
elif [ "$OPTION" = "clean" ]
then
    echo Cleaning project ...
    premake5 gmake2 && make clean && make clean config=release

elif [ "$OPTION" = "release" ]
then
    premake5 gmake2 && make config=release -j7
else
    premake5 gmake2 && make -j7
fi

popd &>/dev/null
