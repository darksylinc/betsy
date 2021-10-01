#!/bin/bash
echo "Formatting source code"
find include/ -name *.cpp -or -name *.h | xargs clang-format -i
find src/ -name *.cpp -or -name *.h | xargs clang-format -i
find tools/ -name *.cpp -or -name *.h | xargs clang-format -i
echo "Formatting shader code"
find shaders/ -name *.glsl | xargs clang-format -i