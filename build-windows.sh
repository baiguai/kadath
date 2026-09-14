#!/bin/bash

# Build Script (Windows cross-compilation via MinGW-w64)
# Requirements:
# sudo apt install -y mingw-w64

cd "$(dirname "$0")"
source ./config.sh
source ./generate-cmake.sh

set -e

echo "Building $APP_NAME for Windows..."

BUILD_TYPE="Debug"
if [ "$1" == "r" ]; then
    BUILD_TYPE="Release"
fi
echo "Performing $BUILD_TYPE build."

TOOLCHAIN="$(pwd)/cmake/mingw-x86_64.cmake"

generate_cmake CMakeLists.txt

cmake -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -B build-windows \
      -S . \
      -DCMAKE_SUPPRESS_DEBUG_LABELS=OFF

cmake --build build-windows -j"$(nproc)"

echo "Build complete: build-windows/bin/$APP_NAME.exe"

# The cross-compiled exe depends on the MinGW-w64 runtime DLLs, which are not
# present on a stock Windows machine. Resolve each DLL through the compiler
# driver and copy it next to the exe.
MINGW_CC="${MINGW_CC:-x86_64-w64-mingw32-gcc}"
DEST="build-windows/bin"
for dll in libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
    src="$("$MINGW_CC" -print-file-name="$dll")"
    if [ -f "$src" ] && [ "$src" != "$dll" ]; then
        cp -f "$src" "$DEST/"
        echo "Copied $dll next to $APP_NAME.exe"
    else
        echo "Warning: could not locate $dll (got: $src) - $APP_NAME.exe may not run on a clean Windows machine" >&2
    fi
done