#pragma once

#include "ui/render.hpp"

namespace kadath {
struct App;
} // namespace kadath

namespace kadath::ui {

// Nested bookmark tree (vim file-tree style: l/enter expand, h collapses,
// j/k move, M new group, R rename, D delete, esc back to main).
void drawBookmarksView(App& app, Canvas& c);

} // namespace kadath::ui