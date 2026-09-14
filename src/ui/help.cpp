#include "ui/help.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "app.hpp"
#include "keyboard/keys.hpp"
#include "keyboard/keymap.hpp"

namespace kadath::ui {

namespace {
constexpr int kColsKey = 18;

std::string ctxName(Context cx) {
    switch (cx) {
        case Context::Main: return "main";
        case Context::Filter: return "filter";
        case Context::Command: return "command";
        case Context::Prompt: return "prompt";
        case Context::Bookmarks: return "bookmarks";
        case Context::Pinned: return "pinned";
        case Context::Edit: return "edit";
        case Context::Help: return "help";
        default: return "global";
    }
}

void appendRow(std::vector<std::string>& rows, const std::string& s) {
    rows.push_back(s);
}
} // namespace

void drawHelpView(App& app, ui::Canvas& c) {
    const Theme& t = *c.theme;
    const float width = (float)c.cols * c.charW;

    // -- header -------------------------------------------------------------
    fillRect(c, 0, 0, width, c.lineH, t.panel);
    textAt(c, kPadX, 0, "help", t.accent);
    textRight(c, 0, "keybindings  (j/k scroll, q/esc back)", t.dim);

    // -- build row text -----------------------------------------------------
    std::vector<std::string> rows;
    rows.reserve(64);
    appendRow(rows, "  [global]");
    for (const auto& [ch, act] : app.keymap.global)
        appendRow(rows, fitCols(chordToString(ch), kColsKey) + " " + actionName(act));

    const Context order[] = {Context::Main, Context::Filter, Context::Command,
                             Context::Prompt, Context::Bookmarks,
                             Context::Pinned, Context::Edit, Context::Help};
    for (Context cx : order) {
        auto it = app.keymap.contexts.find(cx);
        if (it == app.keymap.contexts.end() || it->second.empty()) continue;
        appendRow(rows, "");
        appendRow(rows, "  [" + ctxName(cx) + "]");
        for (const auto& [ch, act] : it->second)
            appendRow(rows, fitCols(chordToString(ch), kColsKey) + " " + actionName(act));
    }

    // -- scroll -------------------------------------------------------------
    const int start = std::max(0, std::min(app.helpScroll, (int)rows.size()));
    for (int i = 0; i < c.rows && (start + i) < (int)rows.size(); ++i) {
        if (start + i >= (int)rows.size()) break;
        if (!rows[(size_t)(start + i)].empty() &&
            rows[(size_t)(start + i)] != "  " &&
            rows[(size_t)(start + i)][0] == '[')
            fillRect(c, 0, (float)i, width, c.lineH, t.panel);
        textAt(c, kPadX, (float)i, rows[(size_t)(start + i)], t.fg);
    }

    // -- footer -------------------------------------------------------------
    fillRect(c, 0, (float)(c.rows - 1), width, c.lineH, t.panel);
    textAt(c, kPadX, (float)(c.rows - 1),
           "bindings live in keybindings.json (edit + reload)", t.dim);
}

} // namespace kadath::ui