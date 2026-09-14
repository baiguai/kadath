#!/bin/bash

# Central configuration - edit values here, all scripts pick them up

APP_NAME="kadath"

SOURCES=(
    "src/main.cpp"
    "src/app.cpp"
    "src/actions.cpp"
    "src/util.cpp"
    "src/keyboard/keys.cpp"
    "src/keyboard/keymap.cpp"
    "src/storage/config.cpp"
    "src/storage/theme.cpp"
    "src/storage/store.cpp"
    "src/clipboard/clipboard.cpp"
    "src/platform/window.cpp"
    "src/platform/hotkey.cpp"
    "src/ui/main_view.cpp"
    "src/ui/bookmarks.cpp"
    "src/ui/pinned.cpp"
    "src/ui/help.cpp"
    "src/ui/edit.cpp"
    "src/ui/command.cpp"
    "src/ui/prompt.cpp"
    "src/ui/render.cpp"
)

LIBS=(
)

HEADERS=(
    "src/actions.hpp"
    "src/util.hpp"
    "src/keyboard/keys.hpp"
    "src/keyboard/keymap.hpp"
    "src/storage/config.hpp"
    "src/storage/theme.hpp"
    "src/storage/store.hpp"
    "src/clipboard/clipboard.hpp"
    "src/platform/window.hpp"
    "src/platform/hotkey.hpp"
    "src/ui/render.hpp"
)

# Core modules shared with the headless test binary (no imgui/glfw/clipboard).
TEST_SOURCES=(
    "src/actions.cpp"
    "src/util.cpp"
    "src/keyboard/keys.cpp"
    "src/keyboard/keymap.cpp"
    "src/storage/config.cpp"
    "src/storage/theme.cpp"
    "src/storage/store.cpp"
    "tests/test_main.cpp"
)

TEST_HEADERS=(
    "src/actions.hpp"
    "src/util.hpp"
    "src/keyboard/keys.hpp"
    "src/keyboard/keymap.hpp"
    "src/storage/config.hpp"
    "src/storage/theme.hpp"
    "src/storage/store.hpp"
)