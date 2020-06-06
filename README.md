# Betsy GPU Compressor

Betsy aims to be a GPU compressor for various modern GPU compression formats such as BC6H,
purposedly written in GLSL so that it can be easily incorporated into OpenGL and Vulkan projects.

Compute Shader support is **required**.

The goal is to achieve both high performance (via efficient GPU usage) and high quality compression.

At the moment it is WIP.

## How do I use it?

Run:
```
betsy input.hdr --codec=etc2 --quality=2 output.ktx
```

Run `betsy --help` for full description

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

### Python scripts

We use [EnumIterator.py](https://github.com/darksylinc/EnumIterator) to generate strings out of enums like in C#.

We keep the generated cpp files in the repo up to date but if you want to generate them yourself, run:

```
cd scripts/EnumIterator
python2 Run.py
```

## Supported formats:

| Format  | State          |Status|
|---------|----------------|------|
| ETC1    | WIP - 95% done | It's already usable. It's missing a couple quality improvements to match the original rg_etc1 codec.<br/>Based on [rg-etc1](https://github.com/richgel999/rg-etc1).<br/>AMD Mesa Linux: Requires a very recent Mesa version due to a shader miscompilation issue. See [ticket](https://gitlab.freedesktop.org/mesa/mesa/-/issues/3044#note_515611).|
| EAC     | Done           | Used for R11, RG11 and ETC2_RGBA (for encoding the alpha component).<br/>Quality: Maximum, we use brute force to check all possible combinations.|
| BC6H UF | Done           | Unsigned variation of B6CH. GLSL port of [GPURealTimeBC6H](https://github.com/knarkowicz/GPURealTimeBC6H)|