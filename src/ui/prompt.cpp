#include "ui/prompt.hpp"

#include <string>

#include "app.hpp"

namespace kadath::ui {

namespace {
} // namespace

void drawPromptView(App& app, Canvas& c) {
    const Theme& t = *c.theme;
    const float width = (float)c.cols * c.charW;
    const int y = c.rows - 1;

    fillRect(c, 0, 0, width, (float)y * c.lineH, t.bg);
    fillRect(c, 0, (float)y, width, c.lineH, t.panel);
    textAt(c, kPadX, (float)y, app.promptLabel, t.accent);

    const float baseX = kPadX + (float)app.promptLabel.size() * c.charW;
    textAt(c, baseX, (float)y, app.promptInput, t.fg);
    float cx = baseX + (float)app.promptCursor * c.charW;
    if (cx >= width - c.charW) cx = width - c.charW;
    if (c.cursorVisible)
        fillRect(c, cx, (float)y + 1.0f, c.charW, c.lineH - 2.0f, t.accent);

    // Hint line (dim) just above.
    std::string hint;
    if (app.promptLabel.find("path") != std::string::npos || app.promptLabel.find("clip to") != std::string::npos)
        hint = "slash-separated group path, e.g. /work/server";
    else if (app.promptLabel.find("rename") != std::string::npos)
        hint = "enter to confirm";
    else
        hint = "enter to confirm";
    textAt(c, kPadX, (float)(y - 1), hint, t.dim);
}

} // namespace kadath::ui