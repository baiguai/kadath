#pragma once

#include "ui/render.hpp"

namespace kadath {
struct App;
} // namespace kadath

namespace kadath::ui {

// Prompt input (new group name, rename-to, add-clip path). enter confirms via
// the keymap; the text is drawn on the last row.
void drawPromptView(App& app, Canvas& c);

} // namespace kadath::ui