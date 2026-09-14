#pragma once

#include <string>

#include <imgui.h>

#include "storage/theme.hpp"

namespace kadath::ui {

// Shared layout constants used by all views.
inline constexpr float kPadX = 4.0f;
inline constexpr int kHeaderRows = 1;

// One full-window "virtual terminal" canvas. Wrap a frame in beginCanvas() /
// endCanvas(); text()/fillRect() draw onto the canvas draw list. Uses the
// selected theme; the caller is responsible for PushFont(P canvas.font).
struct Canvas {
    ImDrawList* dl = nullptr;
    ImFont* font = nullptr;
    ImVec2 origin{0, 0};
    float lineH = 0.0f;   // single text row height (px)
    float charW = 0.0f;   // monospaced advance for one character (px)
    int cols = 0;
    int rows = 0;
    const Theme* theme = nullptr;
    bool cursorVisible = false;
    float cursorBlink = 0.0f;
};

// Open the full-screen ImGui window over the given theme and return metrics.
Canvas beginCanvas(const Theme& theme, ImGuiIO& io);
void endCanvas();

// Draw helpers (logical top-left origin, 0 = first text row).
void fillRect(Canvas& c, float x, float y, float w, float h, const ImVec4& col);
void textAt(Canvas& c, float x, float y, const std::string& s, const ImVec4& col);
void textRight(Canvas& c, float y, const std::string& s, const ImVec4& col);

// Truncate a string to fit `maxCols` columns; appends "…" when truncated.
std::string fitCols(const std::string& s, size_t maxCols);

} // namespace kadath::ui