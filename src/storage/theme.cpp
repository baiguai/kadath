#include "storage/theme.hpp"

#include "util.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <algorithm>
#include <cctype>

namespace kadath {

using json = nlohmann::json;

namespace {

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

ImVec4 imvecFromHex(const std::string& hex) {
    ImVec4 out{0.0f, 0.0f, 0.0f, 1.0f};
    std::string h = hex;
    if (!h.empty() && h[0] == '#') h.erase(0, 1);
    if (h.size() != 6 && h.size() != 8) return out;
    std::array<int, 8> v{};
    for (size_t i = 0; i < h.size(); ++i) {
        v[i] = hexVal(h[i]);
        if (v[i] < 0) return out;
    }
    out.x = (float)((v[0] << 4) | v[1]) / 255.0f;
    out.y = (float)((v[2] << 4) | v[3]) / 255.0f;
    out.z = (float)((v[4] << 4) | v[5]) / 255.0f;
    if (h.size() == 8) {
        out.w = (float)((v[6] << 4) | v[7]) / 255.0f;
    }
    return out;
}

Theme Theme::parse(const std::string& name, const std::string& desc,
                   const std::vector<std::string>& colors) {
    Theme t;
    t.name = name;
    t.desc = desc;
    auto colorAt = [&](size_t i, ImVec4 fallback) {
        if (i < colors.size()) return imvecFromHex(colors[i]);
        return fallback;
    };
    t.bg     = colorAt(0, {0.04f, 0.06f, 0.10f, 1.0f});
    t.panel  = colorAt(1, {0.08f, 0.11f, 0.16f, 1.0f});
    t.fg     = colorAt(2, {0.82f, 0.85f, 0.90f, 1.0f});
    t.dim    = colorAt(3, {0.45f, 0.50f, 0.58f, 1.0f});
    t.accent = colorAt(4, {0.30f, 0.62f, 0.90f, 1.0f});
    t.selBg  = colorAt(5, {0.22f, 0.38f, 0.62f, 1.0f});
    t.selFg  = colorAt(6, {1.00f, 1.00f, 1.00f, 1.0f});
    return t;
}

const std::vector<std::pair<std::string, std::vector<std::string>>>& builtinThemes() {
    // name is the key; the description lives in the second element (unused
    // here, the themes dir carries real metadata).
    static const std::vector<std::pair<std::string, std::vector<std::string>>> themes = {
        {"default", {"#08131f", "#0e1e2c", "#a8c0d8", "#5b7085", "#3f7fb8", "#2c4f82", "#e8f0f8"}},
        {"midnight", {"#0a0a12", "#141420", "#c8c8d8", "#5a5a70", "#7a5af0", "#2a2a4a", "#e8e8f0"}},
        {"paper", {"#f5f0e8", "#ece1d0", "#3b3340", "#8a7f6f", "#b07a4f", "#e0c090", "#2a2320"}},
        {"matrix", {"#020a02", "#041604", "#43f43b", "#1e7a1a", "#55ff55", "#0a3a0a", "#c8ffc0"}},
        {"solarized-dark", {"#002b36", "#073642", "#839496", "#657b83", "#2aa198", "#005f6e", "#fdf6e3"}},
        {"dracula", {"#1a1b26", "#24283b", "#a9b1d6", "#565f89", "#7aa2f7", "#414868", "#c0caf5"}},
    };
    return themes;
}

Theme loadTheme(const std::string& themesDir, const std::string& name) {
    std::string wanted = name.empty() ? "default" : name;
    for (const auto& [n, colors] : builtinThemes()) {
        if (n == wanted) {
            return Theme::parse(wanted, "", colors);
        }
    }
    std::string path = themesDir + "/" + wanted + ".json";
    std::string text = readFile(path);
    if (!text.empty()) {
        try {
            json j = json::parse(text);
            std::string desc = j.value("desc", "");
            std::vector<std::string> colors;
            if (j.contains("colors") && j["colors"].is_array()) {
                for (const auto& c : j["colors"]) colors.push_back(c.get<std::string>());
            }
            if (j.contains("name")) wanted = j["name"].get<std::string>();
            return Theme::parse(wanted, desc, colors);
        } catch (...) {
            // fall through to default below
        }
    }
    // Unknown name: fall back to the built-in default but keep the requested
    // name so the user can see what was configured.
    const auto& colors = builtinThemes().front().second;
    return Theme::parse(name.empty() ? "default" : name, "fallback to built-in default", colors);
}

} // namespace kadath