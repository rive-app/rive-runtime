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

## Install Python PLY

```
python3 -m pip install ply
```

## Add build_rive.sh to $PATH

```
export PATH="$PATH:$(pwd)/submodules/rive-cpp/build"
```

## Build & run

```
build_rive.sh release
out/release/path_fiddle [/path/to/my.riv]
```

## Build & serve for WebGL2

```
build_rive.sh ninja wasm release
cd out/wasm_release
python3 -m http.server 5555
```

## Helpful keys

- `h`/`H`: add/subtract copies to the left and right (only when a .riv is provided)
- `j`/`J`: add/subtract copies below (only when a .riv is provided)
- `k`/`K`: add/subtract copies above (only when a .riv is provided)
- `p`: pause runtime (for benchmarking the renderer in isolation)
- `a`: toggle "atomic" mode
