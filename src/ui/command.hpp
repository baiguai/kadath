#pragma once

#include "ui/render.hpp"

namespace kadath {
struct App;
} // namespace kadath

namespace kadath::ui {

// Ex command input (":quit", ":theme x", ...). enter runs, esc/enter cancel
// via the keymap; the text is drawn on the last row.
void drawCommandView(App& app, Canvas& c);

} // namespace kadath::ui