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

## Install Python PLY

```
python3 -m pip install ply
```

## Call Premake & Build

```
PREMAKE_PATH="$(pwd)/submodules/rive-cpp/build" premake5 gmake2 --with_rive_text --config=release --out=out/release
cd out/release
make -j20
# run path_fiddle
./path_fiddle [/path/to/my.riv]
```

## Build and serve for WebGL2

```
PREMAKE_PATH="$(pwd)/submodules/rive-cpp/build" premake5 gmake2 --with_rive_text --config=release --arch=wasm --out=out/wasm_release
cd out/wasm_release
# Ensure your emsdk_env is configured.
make -j20 path_fiddle
# serve to http://localhost:5555/
python3 -m http.server 5555
```

## Helpful keys

- `h`/`H`: add/subtract copies to the left and right (only when a .riv is provided)
- `j`/`J`: add/subtract copies below (only when a .riv is provided)
- `k`/`K`: add/subtract copies above (only when a .riv is provided)
- `p`: pause runtime (for benchmarking the renderer in isolation)
- `a`: toggle "atomic" mode
