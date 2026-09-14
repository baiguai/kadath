#pragma once

#include <string>

namespace kadath {

// User configuration. Loaded from <dataDir>/config.json with defaults. All
// paths are resolved relative to the per-user data directory.
struct AppConfig {
    // How many history entries to keep (oldest dropped first).
    int maxClips = 250;

    // Theme name (themes/<name>.json or a built-in theme).
    std::string theme = "default";

    // Font pixel size used for the virtual terminal.
    float fontPx = 16.0f;

    // Optional absolute path to a TTF; empty means the bundled JetBrains Mono.
    std::string fontFile;

    // Window geometry for the frameless window.
    int winX = -1, winY = -1;      // -1 = centered
    int winW = 860, winH = 560;

    // Resolved paths (computed by load()).
    std::string dir;
    std::string configFile;
    std::string keybindingsFile;
    std::string clipsFile;
    std::string pinnedFile;
    std::string bookmarksFile;
    std::string themesDir;

    // Load (creating defaults on first run) and populate resolved paths.
    void load();

    // Persist config.json (used to save window position on exit / :theme).
    void save() const;
};

} // namespace kadath