# Rive Viewer
This is a desktop utility for previewing .riv files, Rive experiments, and different renderers. It also serves as a reference implementation for how to interface the Rive C++ Runtime (rive-cpp) with different renderers and factories for fonts, images, etc. 

## Abstraction
Rive is built to be platform and subsystem agnostic so you can plug-in any renderer to draw all of or only portions of your Rive animations. For example a simple WebGL renderer could only draw Rive meshes and drop all the vector content if it wanted to. Similarly image and font loading can be deferred to platform decoders. We provide some example fully fledged implementations that support all of the Rive features.

## Building
We currently provide build files for MacOS but we will be adding Windows and others soon too.
### MacOS
All the build scripts are in viewer/build/macosx.
```
cd viewer/build/macosx
```
You can tell the build script to build and run a Viewer that's backed by a Metal view with our Skia renderer:
```
./build_viewer.sh metal skia run
```
An OpenGL example using a tessellating renderer:
```
./build_viewer.sh gl tess run
```
Both the Skia and Tess renderers work with either gl or metal options.