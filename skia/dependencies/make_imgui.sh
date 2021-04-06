#!/bin/sh

set -e

IMGUI_REPO=https://github.com/ocornut/imgui
IMGUI_STABLE_BRANCH=master

if [ ! -d imgui ]; then
	echo "Cloning ImGui."
    git clone $IMGUI_REPO
fi

cd imgui && git checkout $IMGUI_STABLE_BRANCH && git fetch && git pull
