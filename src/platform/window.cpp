#include "platform/window.hpp"

#include "util.hpp"

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace kadath {

bool Window::init(const std::string& fontFile, float fontPx, int width, int height,
                  const std::string& title) {
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE); // macOS
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // terminal look: no title bar
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    win = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!win) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    (void)io;

    // HiDPI: scale the base font so text stays crisp on scaled monitors.
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);
    int wndW = 0, wndH = 0;
    glfwGetWindowSize(win, &wndW, &wndH);
    if (fbW > 0 && wndW > 0) fontScale = (float)fbW / (float)wndW;

    if (!fontFile.empty() && fileExists(fontFile)) {
        font = io.Fonts->AddFontFromFileTTF(fontFile.c_str(), fontPx * fontScale);
    } else {
        font = io.Fonts->AddFontDefault();
    }

    if (!ImGui_ImplGlfw_InitForOpenGL(win, true) ||
        !ImGui_ImplOpenGL3_Init("#version 330 core")) {
        return false;
    }
    return true;
}

void Window::shutdown() {
    if (ctx) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(ctx);
        ctx = nullptr;
    }
    if (win) {
        glfwDestroyWindow(win);
        win = nullptr;
    }
    glfwTerminate();
}

void Window::beginFrame() {
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
}

void Window::endFrame() {
    ImGui::Render();
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);
    ImVec4 clear = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    glClearColor(clear.x, clear.y, clear.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(win);
}

void Window::pollEvents() { glfwPollEvents(); }

void Window::show() {
    if (!visible()) glfwShowWindow(win);
    glfwFocusWindow(win);
    glfwMakeContextCurrent(win);
}

void Window::hide() {
    if (visible()) glfwHideWindow(win);
    glfwMakeContextCurrent(win);
}

bool Window::visible() const {
    if (!win) return false;
    int x = 0, y = 0;
    glfwGetWindowAttrib(win, GLFW_VISIBLE); // returns int; treat != 0 as visible
    (void)x; (void)y;
    return glfwGetWindowAttrib(win, GLFW_VISIBLE) != 0;
}

int Window::width() const {
    int w = 0, h = 0;
    glfwGetWindowSize(win, &w, &h);
    (void)h;
    return w;
}

int Window::height() const {
    int w = 0, h = 0;
    glfwGetWindowSize(win, &w, &h);
    (void)w;
    return h;
}

int Window::posX() const {
    int x = 0, y = 0;
    glfwGetWindowPos(win, &x, &y);
    (void)y;
    return x;
}

int Window::posY() const {
    int x = 0, y = 0;
    glfwGetWindowPos(win, &x, &y);
    (void)x;
    return y;
}

void focusWindow(GLFWwindow* win) {
    glfwShowWindow(win);
    glfwFocusWindow(win);
}

} // namespace kadath