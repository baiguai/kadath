#pragma once

#include "ui/render.hpp"

namespace kadath {
struct App;
} // namespace kadath

namespace kadath::ui {

// Help / keybinding reference (scrollable).
void drawHelpView(App& app, Canvas& c);

} // namespace kadath::ui