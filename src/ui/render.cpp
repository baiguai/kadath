#include "ui/render.hpp"

#include <algorithm>
#include <cmath>

namespace kadath::ui {

Canvas beginCanvas(const Theme& theme, ImGuiIO& io) {
    Canvas c;
    c.theme = &theme;

    // Optional slight padding around the whole virtual terminal.
    const ImVec2 pad(0, 0);

    ImGui::SetNextWindowPos(ImVec2(pad.x, pad.y));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - pad.x * 2, io.DisplaySize.y - pad.y * 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme.bg);
    ImGui::Begin("kadath", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    c.dl = ImGui::GetWindowDrawList();
    c.origin = ImGui::GetWindowPos();
    c.font = ImGui::GetFont();
    c.lineH = std::max(10.0f, std::ceil(ImGui::GetFontSize() * 1.45f));
    c.charW = std::max(4.0f, ImGui::GetFontSize() * 0.60f);
    c.cols = io.DisplaySize.x > 1 ? (int)(io.DisplaySize.x / c.charW) : 0;
    c.rows = io.DisplaySize.y > 1 ? (int)(io.DisplaySize.y / c.lineH) : 0;
    c.cursorBlink = ImGui::GetTime();
    c.cursorVisible = std::fmod(c.cursorBlink, 1.0) < 0.6;

    return c;
}

void endCanvas() {
    ImGui::End();
}

// Fill a rectangle: x,y are pixels (origin at canvas top-left); h in px.
void fillRect(Canvas& c, float x, float y, float w, float h, const ImVec4& col) {
    ImU32 u = ImGui::ColorConvertFloat4ToU32(col);
    c.dl->AddRectFilled(ImVec2(c.origin.x + x, c.origin.y + y),
                        ImVec2(c.origin.x + x + w, c.origin.y + y + h), u);
}

// Draw text at pixel x, row y (y in line units).
void textAt(Canvas& c, float x, float y, const std::string& s, const ImVec4& col) {
    ImU32 u = ImGui::ColorConvertFloat4ToU32(col);
    c.dl->AddText(ImVec2(c.origin.x + x, c.origin.y + y * c.lineH), u, s.c_str());
}

void textRight(Canvas& c, float y, const std::string& s, const ImVec4& col) {
    const float w = ImGui::CalcTextSize(s.c_str()).x;
    const float x = std::max(0.0f, c.cols * c.charW - w);
    textAt(c, x, y, s, col);
}

std::string fitCols(const std::string& s, size_t maxCols) {
    if (s.empty() || maxCols == 0) return {};
    const unsigned char* p = (const unsigned char*)s.data();
    const size_t n = s.size();
    size_t cols = 0;
    size_t cut = 0;
    size_t shown = 0;
    const unsigned char* q = p;
    while (cols < maxCols && size_t(q - p) < n) {
        unsigned char ch = *q;
        size_t len = 1;
        if (ch >= 0xF0) len = 4;
        else if (ch >= 0xE0) len = 3;
        else if (ch >= 0xC0) len = 2;
        if ((size_t)(q - p) + len > n) break;
        if (len == 1 && (ch == '\n' || ch == '\r')) {
            q += 1;
            continue; // do not count as a column
        }
        cols += 1;
        q += len;
        shown = (size_t)(q - p);
        cut = shown;
    }
    if (cols >= maxCols && cut < n) {
        return s.substr(0, cut) + "\u2026";
    }
    return s.substr(0, cut);
}

} // namespace kadath::ui