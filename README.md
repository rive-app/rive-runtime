# Rive Renderer

The Rive Renderer is a vector and raster graphics renderer custom-built for Rive content, for animation, and for runtime.

This repository contains the renderer code and an example for how to interface with it directly. The code within this repository does not contain a Rive runtime. It contains the best in class concrete implementation of Rive's rendering abstraction layer, which we call the Rive Renderer.

## Clone this repo

Make sure to clone this repo with submodules!

```
git clone --recurse-submodules git@github.com:rive-app/rive-renderer.git
```

## Build GLFW

```
cd submodules/rive-cpp/skia/dependencies
./make_glfw.sh
```

## Install Premake5

Grab the binary from the premake5 GitHub releases: https://github.com/premake/premake-core/releases/ or you can build your own for Apple Silicon:

```
git clone --depth 1 --branch master https://github.com/premake/premake-core.git
cd premake-core
make -f Bootstrap.mak osx PLATFORM=ARM
# binaries will be in bin/release/* put them in your path
```

## Call Premake & Build

```
PREMAKE_PATH="submodules/rive-cpp/build" premake5 gmake2 --with_rive_text --config=release --out=out/release
cd out/release
make
# run path_fiddle
./path_fiddle --metal
```
