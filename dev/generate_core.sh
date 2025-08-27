#!/usr/bin/env bash
set -e

if [[ ! -f "./bin/core_generator" || "$1" == "build" ]]; then
    mkdir -p ./bin
    dart compile exe ./core_generator/lib/main.dart -o ./bin/core_generator
fi
./bin/core_generator