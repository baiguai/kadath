#include "ui/command.hpp"

#include <string>

#include "app.hpp"

namespace kadath::ui {

namespace {
} // namespace

void drawCommandView(App& app, Canvas& c) {
    const Theme& t = *c.theme;
    const float width = (float)c.cols * c.charW;
    const int y = c.rows - 1;

    // Dim everything above the command line (full-window panel).
    fillRect(c, 0, 0, width, (float)y * c.lineH, t.bg);

    fillRect(c, 0, (float)y, width, c.lineH, t.panel);
    textAt(c, kPadX, (float)y, ":" + app.commandText, t.fg);
    float cx = std::min(kPadX + (float)app.commandText.size() * c.charW,
                        width - c.charW);
    if (c.cursorVisible)
        fillRect(c, cx, (float)y + 1.0f, c.charW, c.lineH - 2.0f, t.accent);

    textAt(c, kPadX, 0, "kadath", t.dim);
    textRight(c, 0, "command", t.dim);
}

} // namespace kadath::ui