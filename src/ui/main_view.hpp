#pragma once

#include "ui/render.hpp"

namespace kadath {
struct App;
} // namespace kadath

namespace kadath::ui {

// Main clip-list / filter view (also used while typing a filter).
void drawMainView(App& app, Canvas& c);

} // namespace kadath::ui