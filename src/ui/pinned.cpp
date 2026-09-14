#include "ui/pinned.hpp"

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

void drawPinnedView(App& app, Canvas& c) {
    const Theme& t = *c.theme;
    const float width = (float)c.cols * c.charW;
    const int listStart = 1;
    const int listRows = c.rows - listStart - 1;

    fillRect(c, 0, 0, width, c.lineH, t.panel);
    textAt(c, kPadX, 0, "pinned", t.accent);
    {
        char count[64];
        snprintf(count, sizeof(count), "%zu  (enter=copy)", app.store.pinned.size());
        textRight(c, 0, count, t.dim);
    }

    const size_t n = app.store.pinned.size();
    for (int i = 0; i < listRows; ++i) {
        const size_t idx = (size_t)i;
        if (idx >= n) break;
        const bool sel = (size_t)app.pinSelection == idx;
        if (sel) fillRect(c, 0, (float)(listStart + i), width, c.lineH, t.selBg);
        std::string label = fitCols(oneLine(app.store.pinned[idx].text),
                                    (size_t)std::max(1, c.cols - 8));
        std::string ts = fitCols(app.store.pinned[idx].ts.empty() ? "-"
                                                                  : app.store.pinned[idx].ts,
                                 16);
        textRight(c, (float)(listStart + i), ts, t.dim);
        textAt(c, kPadX, (float)(listStart + i),
               sel ? "> " + label : "  " + label,
               sel ? t.selFg : t.fg);
    }

    if (n == 0)
        textAt(c, kPadX, (float)listStart, "nothing pinned yet (p in main)",
               t.dim);

    fillRect(c, 0, (float)(c.rows - 1), width, c.lineH, t.panel);
    std::string foot;
    if (app.pendingStatus) {
        foot = app.status;
        app.pendingStatus = false;
    } else {
        foot = "j/k:move enter:copy d:unpin esc:back";
    }
    textAt(c, kPadX, (float)(c.rows - 1), foot, t.dim);
}

} // namespace kadath::ui