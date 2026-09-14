#!/bin/bash
#
# upgrade.sh - bump any FetchContent dependency to a newer release.
#
# Dependencies and their canonical variable names in CMakeLists.txt.in:
#   imgui   -> IMGUI_VERSION
#   glfw    -> GLFW_VERSION
#   json    -> NLOHMANN_VERSION
#
# Usage:
#   ./upgrade.sh                  upgrade ALL deps to their latest GitHub release
#   ./upgrade.sh <dep>            upgrade <dep> to its latest release
#   ./upgrade.sh <dep> <tag>      pin <dep> to a specific tag, e.g. ./upgrade.sh glfw 3.5.1
#
# What it does:
#   - reads the current pin from CMakeLists.txt.in (the template scripts use)
#   - updates it to the chosen/latest tag
#   - clears the cached FetchContent checkouts so the new tag is fetched next build
#   - runs a configure pass to verify the new tag still fetches/compiles metadata
#   - on failure it rolls the template back to the previous pin

set -euo pipefail

SH_REPO="https://github.com/ArthurSonzogni/FTXUI.git"
IMGUI_REPO="https://github.com/ocornut/imgui.git"
GLFW_REPO="https://github.com/glfw/glfw.git"
JSON_REPO="https://github.com/nlohmann/json.git"
TEMPLATE="CMakeLists.txt.in"

command -v git >/dev/null 2>&1 || { echo "ERROR: git is required."; exit 1; }
[ -f "$TEMPLATE" ] || { echo "ERROR: $TEMPLATE not found. Run from the repo root."; exit 1; }

TAG_SEP='[vV]?[0-9]+(\.[0-9]+)+$'

latest_tag() { # $1 repo
    git ls-remote --tags --refs "$1" 2>/dev/null \
        | grep -oP "refs/tags/\K${TAG_SEP}" | sort -V | tail -1
}

# Name -> "VAR|repo|tag". Derived from the TEMPLATE variable even when the caller
# passed an explicit tag, so we can report and roll back accurately.
resolve_dep() { # $1 dep-name
    local var repo
    case "$1" in
        imgui) var="IMGUI_VERSION"; repo="$IMGUI_REPO" ;;
        glfw)  var="GLFW_VERSION";  repo="$GLFW_REPO" ;;
        json)  var="NLOHMANN_VERSION"; repo="$JSON_REPO" ;;
        *) echo "ERROR: unknown dependency '$1'. Use one of: imgui glfw json" >&2; exit 1 ;;
    esac
    echo "$var|$repo"
}

# Collect the deps to upgrade.
DEPS=()
if [ $# -ge 1 ]; then
    DEPS+=("$1")
else
    DEPS=("imgui" "glfw" "json")
fi

REQUESTED_TAG="${2:-}"

source ./config.sh
source ./generate-cmake.sh

for dep in "${DEPS[@]}"; do
    IFS='|' read -r VAR REPO <<< "$(resolve_dep "$dep")"
    CURRENT=$(grep -oP "^\s*set\(\Q$VAR\E\s+\K[^)]+" "$TEMPLATE" || true)
    CURRENT="${CURRENT:-unknown}"

    if [ -n "$REQUESTED_TAG" ]; then
        TARGET="$REQUESTED_TAG"
        git ls-remote --tags --refs "$REPO" | grep -q "refs/tags/$TARGET\$" \
            || { echo "ERROR: tag '$TARGET' not found in $REPO." >&2; continue; }
    else
        echo "Querying latest $dep release from GitHub..."
        TARGET=$(latest_tag "$REPO")
        [ -n "$TARGET" ] || { echo "ERROR: could not determine the latest $dep tag (network?)." >&2; continue; }
    fi

    echo ""
    echo "$dep currently pinned at: $CURRENT"
    if [ "$TARGET" = "$CURRENT" ]; then
        echo "$dep is already at $TARGET - nothing to do."
        continue
    fi
    echo "Upgrading $dep: $CURRENT -> $TARGET"

    sed -i "s/^set($VAR .*/set($VAR $TARGET)/" "$TEMPLATE"

    # Clear cached FetchContent checkouts so the new tag is fetched next build
    for b in build build-windows; do
        rm -rf "$b/_deps/${dep}-src" "$b/_deps/${dep}-build" 2>/dev/null || true
    done

    # Regenerate CMakeLists.txt (same as build.sh) and run a configure pass to
    # verify the new tag fetches and has the metadata we expect.
    echo ""
    echo "Verifying $dep $TARGET fetches..."
    generate_cmake CMakeLists.txt
    if ! cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1; then
        echo ""
        echo "ERROR: configure failed for $dep $TARGET."
        echo "  Rolling back the pin to $CURRENT..."
        sed -i "s/^set($VAR .*/set($VAR $CURRENT)/" "$TEMPLATE"
        generate_cmake CMakeLists.txt
        rm -rf "build/_deps/${dep}-src" "build/_deps/${dep}-build"
        echo "Rolled back. Nothing was changed."
        continue
    fi
    echo "Done: $dep pinned to $TARGET ($TEMPLATE)."
done

echo ""
echo "All requested upgrades processed. Run ./build.sh to rebuild."