# Betsy GPU Compressor

Betsy aims to be a GPU compressor for various modern GPU compression formats such as BC6H,
purposedly written in GLSL so that it can be easily incorporated into OpenGL and Vulkan projects.

Compute Shader support is **required**.

The goal is to achieve both high performance (via efficient GPU usage) and high quality compression.

At the moment it is WIP.

## How do I use it?

Run:
```
betsy input.hdr --codec=etc2_rgb --quality=2 output.ktx
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

Double click on build_Visual_Studio_16_2019_x64.bat

If you want to use another target edit the beginning of the bat script, or manually:

1. Point CMake source to %bety_repo_path%
1. Point CMake generation path to %bety_repo_path%/build

Build tested on VS2019.

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
| ETC1    | Done 			| <br/>Based on [rg-etc1](https://github.com/richgel999/rg-etc1).<br/>AMD Mesa Linux: Requires a very recent Mesa version due to a shader miscompilation issue. See [ticket](https://gitlab.freedesktop.org/mesa/mesa/-/issues/3044#note_515611).|
| EAC     | Done           | Used for R11, RG11 and ETC2_RGBA (for encoding the alpha component).<br/>Quality: Maximum, we use brute force to check all possible combinations.|
| ETC2	  | Done           | T, H and P modes implemented. Based on [etc2_encoder](https://github.com/titilambert/packaging-efl/blob/master/src/static_libs/rg_etc/etc2_encoder.c) for T and H modes with a couple minor bugfixes. P was based on the same implementation but uses a much higher quality fitting algorithm; thus neither of the 3 modes will produce the exact same output as the original C version|
| BC1     | Done           | Based on [stb_dxt](https://github.com/nothings/stb/blob/master/stb_dxt.h).|
| BC3     | Done           | This is just BC1 for RGB + BC4 for alpha |
| BC4, BC5| Done           | Based on [stb_dxt](https://github.com/nothings/stb/blob/master/stb_dxt.h). Supports UNORM & SNORM variants|
| BC6H UF | Done           | Unsigned variation of B6CH. GLSL port of [GPURealTimeBC6H](https://github.com/knarkowicz/GPURealTimeBC6H)|

## FAQ

**When I run betsy, I get a 'Display driver stopped responding and has recovered' error**

Some textures are very big and take too long to encode, thus Windows has no way of
knowing if the GPU is stuck in an infinite loop. This problem is not unique to betsy
and if you've used GPU-based tools before, you may already be familiar with this.

To solve this problem try:

   1. Lowering the --quality setting
   1. Use a smaller resolution texture
   1. [Increase the TdrDelay and TdrDdiDelay](https://docs.substance3d.com/spdoc/gpu-drivers-crash-with-long-computations-128745489.html) to a higher value via Registry.

Alternatively if you're on Linux, you may see your X11 session restarts on its own.

**Can I encode using the CPU and GPU at the same time?**

Betsy does not support CPU encoding. However you can run another 3rd party tool to compress
a set of textures while betsy works on a different set.

Note however this could put a lot of stress on your machine (both CPU and GPU to 100%).
Make sure your PSU can handle the load and your cooling solution keeps your system from overheating.

If you're overclocking your hardware, your system could become unstable or
silently produce incorrect output on a few pixels

*Note:* You can use Mesa's llvmpipe DLLs to run betsy under the CPU.
I did not try this and I don't know what the performance would be like.

**Does betsy produce the same quality as the original implementations they were based on? (or bit-exact output)?**

In theory yes. In practice there could have been bugs during the port/adaptation,
and in some cases there could be precision issues.

For example rg-etc1 used uint64 for storing error comparison, while we use a 32-bit float.
While I don't think this should make a difference, I could be wrong.

Another example is in BC1, in the function RefineBlock the variable `akku` can theoretically reach as high as 9437184, which gets dangerously close to 16777216, aka the last integer number that can be accurately represented using 32-bit floating point.
It doesn't look like this is an issue, as it is later divided by 65535. But I could be wrong.

There could also be compiler/driver/hardware bugs causing the code to misbehave, as is more common with compute shaders.

**Did you write these codecs yourself?**

So far I only wrote the EAC codec from scratch, but I used [etc2_encoder](https://github.com/titilambert/packaging-efl/blob/master/src/static_libs/rg_etc/etc2_encoder.c) for reference, particularly figuring out the bit pattern of the bit output and the idea of just using brute force. Unfortunately this version had several bugs which is why I just wrote it from scratch.

The P mode from ETC2 is also based on [etc2_encoder](https://github.com/titilambert/packaging-efl/blob/master/src/static_libs/rg_etc/etc2_encoder.c) but I rewrote the fitting algorithm because the original was way too basic. The new one produces significantly higher quality output.

The rest of the codecs were originally written for different shading languages or architectures. See the supported formats' table for references to the original implementations.

**Why does it use OpenGL?**

This project has been [contracted by Godot](https://godotengine.org/article/godot-core-budget-meeting-report-1)
to improve its import time process.

One of the main requirements was that the shading language had to be GLSL to ease its integration into Godot.

Vulkan requires a lot of boiler-plate initialization code and dealing with unnecessary complexity that would
have burnt too much of the contract's budget which is better spent on improving the conversion algorithms,
thus OpenGL was a much more reasonable choice.

OpenGL allowed us to get into writing the application right away while also using GLSL as the main shading language.

Note that betsy has abstracted almost all of the API code to
[PlatformGL.cpp](src/PlatformGL.cpp) and [EncoderGL.cpp](src/betsy/EncoderGL.cpp),
meaning that it should be possible to port it to Vulkan (in fact it has been made so,
so that it is easy to integrate into Godot)

**Why does the code have execute01(), execute02(), and so on...?**

Many of the compute shaders can be run in parallel via Async Compute and have multiple steps.

Each step must wait for the completion of the previous step in order to process (e.g. barrier)

This means that you can call (assuming a Vulkan solution):

```
for( int i=0; i < numEncoders; ++i )
    encoder[i].execute01();
for( int i=0; i < numEncoders; ++i )
    encoder[i].barriers01(); // Not implemented in OpenGL

for( int i=0; i < numEncoders; ++i )
    encoder[i].execute02();
for( int i=0; i < numEncoders; ++i )
    encoder[i].barriers02(); // Not implemented in OpenGL

for( int i=0; i < numEncoders; ++i )
    encoder[i].execute03();
```

to have them all run in parallel

## Legal

See [LICENSE.md](LICENSE.md)