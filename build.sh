#!/bin/bash

# Build Script (Linux)

set -e  # Exit on any error
cd "$(dirname "$0")"
source ./config.sh
source ./generate-cmake.sh

# Determine build type
BUILD_TYPE="Debug"
for arg in "$@"; do
    case "$arg" in
        r) BUILD_TYPE="Release";;
        w) BUILD_WINDOWS=1;;
    esac
done

echo "Performing $BUILD_TYPE build."

# Optional Windows cross-build (add 'w' as the second argument)
if [ "$BUILD_WINDOWS" == "1" ]; then
    echo "Cross-building Windows EXE..."
    ./build-windows.sh r
fi

generate_cmake CMakeLists.txt

# Configure with CMake
cmake -S . -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE

# Build the project
cmake --build build -j"$(nproc)"

# Check if build was successful
BIN="build/bin/$APP_NAME"
if [ -f "$BIN" ]; then
    echo "-- Build successful --"
    echo "Executable: $(pwd)/$BIN"
    echo ""
    echo "To run $APP_NAME:"
    echo "  ./build/bin/$APP_NAME"
else
    echo "! failed !"
    exit 1
fi