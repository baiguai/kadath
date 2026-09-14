#pragma once

#include "ui/render.hpp"

namespace kadath {
struct App;
} // namespace kadath

namespace kadath::ui {

// Pinned clip list (subset kept across prunes). enter copies.
void drawPinnedView(App& app, Canvas& c);

} // namespace kadath::ui