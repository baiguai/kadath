#pragma once

#include <imgui.h>

#include <string>
#include <vector>

namespace kadath {

// A color theme: name + description + 7 palette entries matching the order
// used by the built-in themes and themes/*.json.
struct Theme {
    std::string name;
    std::string desc;
    ImVec4 bg;           // window background
    ImVec4 panel;        // alternate row / prompt area
    ImVec4 fg;           // default text
    ImVec4 dim;          // hints / numbers / non-selected accents
    ImVec4 accent;       // borders, indicators
    ImVec4 selBg;        // selection background
    ImVec4 selFg;        // selection text

    static Theme parse(const std::string& name, const std::string& desc,
                       const std::vector<std::string>& colors);
};

// Parse "#rrggbb" or "#rrggbbaa" into an ImVec4; on failure returns {0,0,0,1}.
ImVec4 imvecFromHex(const std::string& hex);

// Load a theme by name from themes/<name>.json (entries may have an explicit
// "name"/"colors"; a missing file falls back to the built-in "default").
// The last path component, if present, selects the subdirectory to search:
Theme loadTheme(const std::string& themesDir, const std::string& name);

// The built-in theme used when nothing is configured yet.
const std::vector<std::pair<std::string, std::vector<std::string>>>& builtinThemes();

} // namespace kadath