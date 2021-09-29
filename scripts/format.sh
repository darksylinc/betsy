#!/bin/bash
echo "Formatting source code"
find src/ -name *.cpp -or -name *.hpp | xargs clang-format -i
echo "Formatting shader code"
find shaders/ -name *.glsl | xargs clang-format -i