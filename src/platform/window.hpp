#pragma once

#include <string>

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace kadath {

// Frameless GLFW window hosting the ImGui "terminal". Owns the GLFW window,
// the ImGui context/backends and the JetBrains Mono font.
struct Window {
    GLFWwindow* win = nullptr;
    ImGuiContext* ctx = nullptr;
    ImFont* font = nullptr;
    float fontScale = 1.0f;

    bool init(const std::string& fontFile, float fontPx, int width, int height,
              const std::string& title);
    void shutdown();

    bool shouldClose() const { return glfwWindowShouldClose(win) != 0; }

    void beginFrame();         // new ImGui frame (call once per loop iteration)
    void endFrame();           // render + swap buffers
    void pollEvents();

    void show();
    void hide();
    bool visible() const;

    // Query GLFW window geometry (logical pixels).
    int width() const;
    int height() const;
    int posX() const;
    int posY() const;
};

// Ask the WM to raise + focus the window (used when the global hotkey shows it).
void focusWindow(GLFWwindow* win);

} // namespace kadath