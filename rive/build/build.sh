#!/bin/bash

OPTION=$1

if [ "$OPTION" = 'help' ]
then
    echo build.sh - build debug library
    echo build.sh clean - clean the build
    echo build.sh release - build release library 
elif [ "$OPTION" = "clean" ]
then
    echo Cleaning project ...
    premake5 clean
elif [ "$OPTION" = "release" ]
then
    premake5 gmake && make config=release
else
    premake5 gmake && make
fi