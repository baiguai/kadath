#pragma once

#include "ui/render.hpp"

namespace kadath {
struct App;
} // namespace kadath

namespace kadath::ui {

// Full-clip editor (single document, wrapped; ctrl+s saves back to the clip).
void drawEditView(App& app, Canvas& c);

} // namespace kadath::ui