#!/usr/bin/env bash
set -e

if [[ ! -f "./bin/core_generator" || "$1" == "build" ]]; then
    # .dart_tool/ is gitignored, so a fresh clone has no package_config.json
    # and `package:core_generator/...` imports fail to resolve. Cheap when it
    # is already resolved, and the difference between working locally and
    # failing on a clean CI checkout.
    (cd ./core_generator && dart pub get)
    mkdir -p ./bin
    dart compile exe ./core_generator/lib/main.dart -o ./bin/core_generator
fi
./bin/core_generator