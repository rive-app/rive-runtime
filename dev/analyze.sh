#!/bin/bash

# install scan-build
if ! command -v scan-build &> /dev/null
then
    if ! command -v pip &> /dev/null
    then
        curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
        python3 get-pip.py
    fi
    pip install scan-build
fi


cd test
premake5 clean || exit 1
premake5 gmake || exit 1
scan-build -o ../analysis_report/ make -j7 || exit 1
premake5 clean
