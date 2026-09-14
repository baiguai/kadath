#!/bin/bash
#
# setup.sh - checks prerequisites and prepares dependencies for kadath.
#
# Safe to run at any time; it never creates scaffolding. It can also install
# the required dev packages on supported distros (apt/dnf/pacman).
#
# Usage:
#   ./setup.sh            check prerequisites (report only)
#   ./setup.sh --install  also attempt to install missing dev packages
#   ./setup.sh --prefetch fetch/git dependencies now so the first ./build.sh
#                         works without network access
#
# Flags may be combined, e.g. ./setup.sh -i -p -w
#   -i / --install    install missing system deps via the distro package manager
#   -w / --windows    also install the MinGW-w64 cross-compiler
#   -v / --valgrind   also install valgrind
#   -p / --prefetch   prefetch FetchContent deps into build/
#
# kadath needs:
#   - cmake 3.20+
#   - a C++20 compiler (g++ or clang++)
#   - git            (FetchContent clones imgui/glfw/json from GitHub)
#   - make           (build.sh compiles with cmake --build)
#   - X11/GL/GLFW dev headers (Linux): libx11-dev libx11-xcb-dev libxext-dev
#     libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
#     libglu1-mesa-dev (libxfixes-dev is pulled automatically but listed too)
#   - Wayland dev headers (optional, GLFW feature detection): libwayland-dev
#     wayland-protocols libxkbcommon-dev
#   - mingw-w64 (optional, for ./build-windows.sh)
#   - network to github.com (to download deps on first build)

set -e

INSTALL=0
PREFETCH=0
WINDOWS=0
VALGRIND=0

for arg in "$@"; do
    case "$arg" in
        -i|--install)   INSTALL=1 ;;
        -p|--prefetch)  PREFETCH=1 ;;
        -w|--windows)   WINDOWS=1 ;;
        -v|--valgrind)  VALGRIND=1 ;;
        *) echo "usage: ./setup.sh [-i|--install] [-p|--prefetch] [-w|--windows] [-v|--valgrind]"; exit 2 ;;
    esac
done

echo "=== kadath setup ==="

# --- package manager --------------------------------------------------------
PKG_NONE=0
install_pkgs() {
    [ "$INSTALL" -eq 1 ] || { echo "    (skipping install; rerun with -i/--install)"; return 0; }
    echo "    Installing: $*"
    if command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update && sudo apt-get install -y "$@"
    elif command -v dnf >/dev/null 2>&1; then
        sudo dnf install -y "$@"
    elif command -v pacman >/dev/null 2>&1; then
        sudo pacman -S --needed --noconfirm "$@"
    else
        echo "    ERROR: no supported package manager (apt/dnf/pacman) - install these manually." >&2
        PKG_NONE=1
    fi
}

# Split on a PATH-like separator so the call sites stay readable.
install_pkgs_split() {
    IFS=';' read -r -a _p <<< "$1"
    install_pkgs "${_p[@]}"
}

# --- cmake ------------------------------------------------------------------
if command -v cmake >/dev/null 2>&1; then
    CMAKE_VER=$(cmake --version | head -1 | grep -oP '\d+\.\d+')
    if [ "$(printf '%s\n3.20\n' "$CMAKE_VER" | sort -V | head -1)" = "3.20" ] || [ "$CMAKE_VER" = "$(printf '%s\n3.20\n' "$CMAKE_VER" | sort -V | head -1)" ]; then
        echo "[OK] cmake $CMAKE_VER"
    else
        echo "cmake $CMAKE_VER is too old (3.20+ required); installing newer..."
        if command -v apt-get >/dev/null 2>&1; then
            sudo apt-get install -y cmake ninja-build || true
        fi
        echo "      If apt provided an old cmake, install a newer one (pip install cmake) and rerun." >&2
    fi
else
    echo "cmake not found; installing..."
    install_pkgs_split "cmake;ninja-build"
fi

# --- compiler ---------------------------------------------------------------
if command -v g++ >/dev/null 2>&1; then
    echo "[OK] g++ $(g++ -dumpversion)"
elif command -v clang++ >/dev/null 2>&1; then
    echo "[OK] clang++ ($(clang++ --version | head -1))"
else
    echo "No C++20 compiler found; installing g++..."
    if command -v apt-get >/dev/null 2>&1; then
        install_pkgs_split "g++;g++-12;build-essential"
    else
        install_pkgs_split "gcc-c++;g++"
    fi
fi

# --- git (deps are fetched from GitHub) -------------------------------------
if command -v git >/dev/null 2>&1; then
    echo "[OK] git $(git --version | cut -d' ' -f3)"
else
    echo "git not found; installing..."
    install_pkgs_split "git"
fi

# --- make -------------------------------------------------------------------
if command -v make >/dev/null 2>&1; then
    echo "[OK] make $(make --version | head -1 | sed 's/^GNU Make //')"
else
    echo "make not found; installing..."
    install_pkgs_split "make"
fi

# --- Linux build headers (X11 + OpenGL + Wayland) ---------------------------
if [ "$(uname -s)" = "Linux" ]; then
    missing=""
    for h in X11/Xlib.h X11/extensions/Xfixes.h GL/gl.h GL/glx.h; do
        printf '#include <%s>\nint main(){return 0;}\n' "$h" | g++ -x c++ - -o /dev/null >/dev/null 2>&1 || missing="$missing $h"
    done
    for h in wayland-client.h xkbcommon.h; do
        printf '#include <%s>\nint main(){return 0;}\n' "$h" | g++ -x c++ - -o /dev/null >/dev/null 2>&1 || missing="$missing $h"
    done
    if [ -n "$missing" ]; then
        echo "Missing dev headers:$missing"
        install_pkgs_split "libx11-dev;libx11-xcb-dev;libxext-dev;libxrandr-dev;libxinerama-dev;libxcursor-dev;libxi-dev;libxfixes-dev;libgl1-mesa-dev;libwayland-dev;wayland-protocols;libxkbcommon-dev"
    else
        echo "[OK] X11/OpenGL/Wayland dev headers present"
    fi
fi

# --- FetchContent metadata (fetch only, no compile) -------------------------
if [ "$PREFETCH" -eq 1 ]; then
    echo "Prefetching dependencies (imgui/glfw/json)..."
    source ./config.sh
    source ./generate-cmake.sh
    generate_cmake CMakeLists.txt
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
    _deps="build/_deps"
    for d in imgui glfw json; do
        if [ -f "$_deps/${d}-src/CMakeLists.txt" ]; then
            echo "[OK] $d fetched into $_deps/${d}-src"
        else
            echo "WARNING: $d did not land in $_deps/${d}-src - check network." >&2
        fi
    done
else
    echo "Dependencies (imgui/glfw/json) will be downloaded on first ./build.sh (needs network)."
    echo "  To fetch them now:  ./setup.sh --prefetch"
fi

# --- Windows cross-build (optional) -----------------------------------------
if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
    echo "[OK] MinGW-w64 cross-compiler (Windows builds supported)"
elif [ "$WINDOWS" -eq 1 ]; then
    echo "Installing MinGW-w64 cross-compiler..."
    install_pkgs_split "mingw-w64"
    if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        echo "[OK] MinGW-w64 installed"
    fi
else
    echo "MinGW-w64 not found - Windows builds (./build-windows.sh) will fail."
    echo "  Install it with:  sudo apt install -y mingw-w64  (or ./setup.sh -w)"
fi

# --- valgrind (optional) ----------------------------------------------------
if command -v valgrind >/dev/null 2>&1; then
    echo "[OK] valgrind"
elif [ "$VALGRIND" -eq 1 ]; then
    echo "Installing valgrind..."
    install_pkgs_split "valgrind"
fi

echo ""
echo "=== Setup complete ==="
echo "Run ./build.sh to build."