#include "ui/edit.hpp"

#include <algorithm>
#include <string>

#include "app.hpp"

namespace kadath::ui {

namespace {

// Number of logical (newline-separated) lines in the editor document.
size_t logicalLines(const std::string& s) {
    if (s.empty()) return 1;
    size_t n = 1;
    for (char ch : s)
        if (ch == '\n') ++n;
    return n;
}
} // namespace

void drawEditView(App& app, Canvas& c) {
    const Theme& t = *c.theme;
    const float width = (float)c.cols * c.charW;
    const int listStart = 1;
    const int listRows = c.rows - listStart - 1;

    fillRect(c, 0, 0, width, c.lineH, t.panel);
    textAt(c, kPadX, 0, "EDIT", t.accent);
    {
        size_t firstNl = app.editText.find('\n');
        std::string head = app.editText.substr(0, firstNl == std::string::npos ? app.editText.size() : firstNl);
        char info[96];
        snprintf(info, sizeof(info), "row %d/%zu  (ctrl+s save  esc cancel)",
                 app.editRow + 1, logicalLines(app.editText));
        textRight(c, 0, info, t.dim);
    }

    // Vertical scroll so the cursor row stays visible.
    int top = std::clamp(app.editRow, 0, std::max(0, app.editRow - listRows + 2));
    top = std::max(0, std::min<int>(top, (int)logicalLines(app.editText) - listRows));

    for (int i = 0; i < listRows; ++i) {
        const int logicalRow = top + i;
        if (logicalRow >= (int)logicalLines(app.editText)) break;
        std::string line;
        {   // split at newline
            size_t start = 0, r = 0;
            while (r < (size_t)logicalRow) {
                size_t nl = app.editText.find('\n', start);
                if (nl == std::string::npos) { start = app.editText.size(); break; }
                start = nl + 1;
                ++r;
            }
            size_t nl = app.editText.find('\n', start);
            line = app.editText.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        }
        bool cursorRow = (logicalRow == app.editRow);
        if (cursorRow) {
            fillRect(c, 0, (float)(listStart + i), width, c.lineH, t.panel);
            // cursor block
            float cx = kPadX + (float)app.editCol * c.charW;
            if (cx >= width - 2.0f) cx = width - 2.0f - c.charW;
            fillRect(c, std::max(kPadX, cx), (float)(listStart + i), c.charW,
                     c.lineH, t.accent);
            textAt(c, kPadX, (float)(listStart + i), fitCols(line, (size_t)c.cols - 2),
                   t.selFg);
        } else {
            textAt(c, kPadX, (float)(listStart + i), fitCols(line, (size_t)c.cols - 2), t.fg);
        }
    }

    fillRect(c, 0, (float)(c.rows - 1), width, c.lineH, t.panel);
    textAt(c, kPadX, (float)(c.rows - 1), "edit mode   ctrl+s: save   esc: cancel   arrows/home/end: move", t.dim);
}

} // namespace kadath::ui