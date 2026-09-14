#include "ui/main_view.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

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

} // namespace

void drawMainView(App& app, Canvas& c) {
    const Theme& t = *c.theme;
    const float width = (float)c.cols * c.charW;
    const int listStart = kHeaderRows;
    const int listRows = c.rows - listStart - 1; // reserve footer

    // -- header -----------------------------------------------------------
    fillRect(c, 0, 0, width, c.lineH, t.panel);
    textAt(c, kPadX, 0, "kadath", t.accent);
    textAt(c, kPadX + 8.0f * c.charW, 0, "clips", t.dim);
    {
        char count[96];
        snprintf(count, sizeof(count), "%zu/%zu  %s", app.visibleCount(),
                 app.store.clips.size(),
                 app.filterActive() ? "filter" : "history");
        textRight(c, 0, count, t.dim);
    }

    // -- clip list --------------------------------------------------------
    const size_t n = app.visibleCount();
    bool pinnedMarked = false;
    for (int i = 0; i < listRows; ++i) {
        const size_t idx = (size_t)app.scroll + (size_t)i;
        if (idx >= n) break;
        const bool sel = (app.context == Context::Main || app.context == Context::Filter) &&
                         (size_t)app.selection == idx;
        if (sel) fillRect(c, 0, (float)(listStart + i), width, c.lineH, t.selBg);

        const Clip& clip = app.visibleClip(idx);
        pinnedMarked = false;
        for (const Clip& p : app.store.pinned)
            if (p.text == clip.text) { pinnedMarked = true; break; }

        std::string line = oneLine(clip.text);
        if (!line.empty() && line.back() == ' ') line.pop_back();
        std::string label = fitCols(std::move(line), (size_t)std::max(1, c.cols - 8));

        // Right-aligned timestamp (most recent first).
        std::string ts = fitCols(clip.ts.empty() ? "-" : clip.ts, 16);
        textRight(c, (float)(listStart + i), ts, t.dim);

        if (sel) {
            textAt(c, kPadX, (float)(listStart + i),
                   (pinnedMarked ? "* " : "> ") + label, t.selFg);
        } else {
            textAt(c, kPadX, (float)(listStart + i),
                   (pinnedMarked ? "* " : "  ") + label, t.fg);
        }
    }

    if (n == 0) {
        textAt(c, kPadX, (float)listStart, "no clips yet - copy something",
               t.dim);
    }

    // -- footer -----------------------------------------------------------
    fillRect(c, 0, (float)(c.rows - 1), width, c.lineH, t.panel);
    std::string foot;
    if (app.context == Context::Filter) {
        foot = "/ " + app.filterText + "  (enter=copy  esc=cancel)";
    } else if (app.pendingStatus) {
        foot = app.status;
        app.pendingStatus = false;
    } else {
        foot = "j/k:move enter:copy d:delete e:edit p:pin m:group /:find ::cmd b:bookmarks P:pinned ?:help";
    }
    textAt(c, kPadX, (float)(c.rows - 1), foot, t.dim);

    // scroll indicator (right edge of footer).
    if (n > (size_t)listRows) {
        char sb[48];
        snprintf(sb, sizeof(sb), "%d%%", (int)(100 * (double)app.scroll /
                                               (double)std::max<size_t>(1, n)));
        textRight(c, (float)(c.rows - 1), sb, t.accent);
    }
}

} // namespace kadath::ui