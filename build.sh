#!/bin/bash
set -e
TARGET=${1:-combined_demo}
cmake -B build -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -1
cmake --build build --target "$TARGET" -j$(sysctl -n hw.ncpu)
./build/"$TARGET"
