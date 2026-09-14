#include "ui/bookmarks.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "app.hpp"
#include "storage/store.hpp"

namespace kadath::ui {

namespace {


std::string oneLine(const std::string& s) {
    std::string out = s;
    for (auto& ch : out)
        if (ch == '\n' || ch == '\r') ch = ' ';
    return out;
}

// Selected node name for the list row.
std::string labelFor(App& app, const App::FlatNode& n) {
    if (n.isGroup) {
        const Group* g = app.store.groupAt(n.path);
        if (!g) return "?";
        char sizeInfo[64];
        snprintf(sizeInfo, sizeof(sizeInfo), "%zuG/%zuC", g->groups.size(),
                 g->clips.size());
        return g->name.empty() ? "?" : g->name + "  [" + sizeInfo + "]";
    }
    // Clip node: parent path = path minus last index; a clip index in the
    // parent is encoded relative to the parent's subgroup count.
    std::vector<size_t> parentPath = n.path;
    if (parentPath.empty()) return "?";
    parentPath.pop_back();
    const Group* parent = app.store.groupAt(parentPath);
    const std::vector<Clip>* cs;
    size_t clipIdx;
    if (parent) {
        cs = &parent->clips;
        clipIdx = n.path.back() >= parent->groups.size() ? n.path.back() - parent->groups.size() : 0;
        if (clipIdx >= cs->size()) return "?";
    } else {
        cs = &app.store.clips;
        clipIdx = n.path.back();
        if (clipIdx >= cs->size()) return "?";
    }
    return oneLine((*cs)[clipIdx].text);
}

} // namespace

void drawBookmarksView(App& app, Canvas& c) {
    const Theme& t = *c.theme;
    const float width = (float)c.cols * c.charW;
    const int listStart = 1;
    const int listRows = c.rows - listStart - 1;

    // -- header -----------------------------------------------------------
    fillRect(c, 0, 0, width, c.lineH, t.panel);
    textAt(c, kPadX, 0, "bookmarks", t.accent);
    {
        auto flat = app.flattenBookmarks();
        char count[64];
        snprintf(count, sizeof(count), "%zu visible  (l=expand h=collapse)",
                 flat.size());
        textRight(c, 0, count, t.dim);
    }

    // -- tree -------------------------------------------------------------
    std::vector<App::FlatNode> flat = app.flattenBookmarks();
    int row = 0;
    for (const App::FlatNode& n : flat) {
        if (row >= listRows) break;
        const bool sel = n.path == app.bmPath;
        if (sel) fillRect(c, 0, (float)(listStart + row), width, c.lineH, t.selBg);

        std::string prefix;
        if (n.isGroup) {
            prefix = n.expanded ? "v " : "> ";
            for (int d = 0; d < n.depth; ++d) prefix += "  ";
        } else {
            prefix = "  ";
            for (int d = 0; d < n.depth; ++d) prefix += "  ";
        }
        std::string label = fitCols(prefix + labelFor(app, n), (size_t)c.cols - 4);
        textAt(c, kPadX, (float)(listStart + row), label,
               sel ? t.selFg : (n.isGroup ? t.accent : t.fg));
        ++row;
    }

    if (flat.empty()) {
        textAt(c, kPadX, (float)listStart, "bookmarks empty  (M = new group)",
               t.dim);
    }

    // -- footer -----------------------------------------------------------
    fillRect(c, 0, (float)(c.rows - 1), width, c.lineH, t.panel);
    std::string foot;
    if (app.pendingStatus) {
        foot = app.status;
        app.pendingStatus = false;
    } else {
        foot = "j/k:move l:expand h:collapse M:group R:rename D:delete enter:copy esc:back";
    }
    textAt(c, kPadX, (float)(c.rows - 1), foot, t.dim);
}

} // namespace kadath::ui