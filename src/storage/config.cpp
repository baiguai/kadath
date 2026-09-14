#include "storage/config.hpp"

#include "util.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

namespace kadath {

using json = nlohmann::json;

void AppConfig::load() {
    dir = dataDir();
    ensureDir(dir);
    ensureDir(dir + "/themes");

    configFile = dir + "/config.json";
    keybindingsFile = dir + "/keybindings.json";
    clipsFile = dir + "/clips.json";
    pinnedFile = dir + "/pinned.json";
    bookmarksFile = dir + "/bookmarks.json";
    themesDir = dir + "/themes";

    std::string text = readFile(configFile);
    if (!text.empty()) {
        try {
            json j = json::parse(text);
            if (j.contains("max_clips")) maxClips = j["max_clips"].get<int>();
            if (j.contains("theme")) theme = j["theme"].get<std::string>();
            if (j.contains("font_px")) fontPx = j["font_px"].get<float>();
            if (j.contains("font_file")) fontFile = j["font_file"].get<std::string>();
            if (j.contains("win")) {
                const auto& w = j["win"];
                if (w.contains("x") && w["x"].is_number()) winX = w["x"].get<int>();
                if (w.contains("y") && w["y"].is_number()) winY = w["y"].get<int>();
                if (w.contains("w") && w["w"].is_number()) winW = w["w"].get<int>();
                if (w.contains("h") && w["h"].is_number()) winH = w["h"].get<int>();
            }
        } catch (...) {
            // Config is corrupt: fall back to defaults.
        }
    }
    if (winW <= 0) winW = 860;
    if (winH <= 0) winH = 560;
}

void AppConfig::save() const {
    json w;
    w["x"] = winX;
    w["y"] = winY;
    w["w"] = winW;
    w["h"] = winH;
    json j;
    j["max_clips"] = maxClips;
    j["theme"] = theme;
    j["font_px"] = fontPx;
    j["font_file"] = fontFile;
    j["win"] = w;
    writeFile(configFile, j.dump(2) + "\n");
}

} // namespace kadath