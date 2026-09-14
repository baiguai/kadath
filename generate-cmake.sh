#!/bin/bash
#
# generate-cmake.sh - render CMakeLists.txt from CMakeLists.txt.in + config.sh
#
# Shared by build.sh, build-windows.sh, setup.sh and upgrade.sh so the
# templating logic lives in exactly one place.
#
# Usage (from the repo root):
#   source ./config.sh
#   source ./generate-cmake.sh
#   generate_cmake [destination]

set -e

generate_cmake() {
    local DST="${1:-CMakeLists.txt}"

    [ -f CMakeLists.txt.in ] || { echo "generate-cmake: CMakeLists.txt.in not found (run from repo root)" >&2; return 1; }
    [ -f config.sh ] || { echo "generate-cmake: config.sh not found (run from repo root)" >&2; return 1; }
    [ -n "$APP_NAME" ] || { echo "generate-cmake: config.sh must be sourced first (APP_NAME is empty)" >&2; return 1; }

    cp CMakeLists.txt.in "$DST"

    sed -i "s/<<TARGET_NAME>>/$APP_NAME/g" "$DST"

    local TMP
    TMP=$(mktemp)
    for s in "${SOURCES[@]}"; do
        echo "    $s" >> "$TMP"
    done
    sed -i "/^<<SOURCES>>$/{
        r $TMP
        d
    }" "$DST"
    rm -f "$TMP"

    for lib in "${LIBS[@]}"; do
        if [[ "$lib" == *::* ]]; then
            echo "target_link_libraries($APP_NAME PRIVATE $lib)" >> "$DST"
        else
            echo "target_link_libraries($APP_NAME PRIVATE \${CMAKE_SOURCE_DIR}/$lib)" >> "$DST"
        fi
    done

    # Tests target (headless; shared pure-logic modules only).
    if [[ -n "$TEST_SOURCES" ]]; then
        local TMP2
        TMP2=$(mktemp)
        for t in "${TEST_SOURCES[@]}"; do
            echo "    $t" >> "$TMP2"
        done
        sed -i "/^<<TESTSOURCES>>$/{
            r $TMP2
            d
        }" "$DST"
        rm -f "$TMP2"
    fi

    echo "generate-cmake: wrote $DST (target: $APP_NAME)"
}