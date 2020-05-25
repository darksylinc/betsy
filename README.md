# Betsy GPU Compressor

Betsy aims to be a GPU compressor for various modern GPU compression formats such as BC6H,
purposedly written in GLSL so that it can be easily incorporated into OpenGL and Vulkan projects.

Compute Shader support is **required**.

At the moment it is very WIP.

## Build Instructions

### Ubuntu

```
# Debug
sudo apt install libsdl2-dev ninja-build
mkdir -p build/Debug
cd build/Debug
cmake ../.. -DCMAKE_BUILD_TYPE=Debug -GNinja
ninja
cd ../../bin/Debug
./betsy

# Release
sudo apt install libsdl2-dev ninja-build
mkdir -p build/Release
cd build/Release
cmake ../.. -DCMAKE_BUILD_TYPE=Release -GNinja
ninja
cd ../../bin/Release
./betsy
```

### Windows

TBD.
Build works on VS2019. CMake generation, but it will complain about SDL2 due to poorly setup sdl2-config.cmake.
You'll need to setup the paths to SDL2 by hand.