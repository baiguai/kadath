#include "keyboard/keys.hpp"

#include <GLFW/glfw3.h>

#include <cctype>
#include <mutex>

namespace kadath {

namespace {

std::vector<KeyChord> g_queue;
std::mutex g_queueMutex;
GLFWwindow* g_window = nullptr;

const char* namedName(int base) {
    switch (base) {
        case NK_Enter:     return "enter";
        case NK_Escape:    return "esc";
        case NK_Tab:       return "tab";
        case NK_Backspace: return "backspace";
        case NK_Delete:    return "delete";
        case NK_Up:        return "up";
        case NK_Down:      return "down";
        case NK_Left:      return "left";
        case NK_Right:     return "right";
        case NK_Home:      return "home";
        case NK_End:       return "end";
        case NK_PageUp:    return "pageup";
        case NK_PageDown:  return "pagedown";
        case NK_Space:     return "space";
    }
    return nullptr;
}

int namedFromName(const std::string& s) {
    if (s == "enter")            return NK_Enter;
    if (s == "esc" || s == "escape") return NK_Escape;
    if (s == "tab")              return NK_Tab;
    if (s == "backspace" || s == "bs") return NK_Backspace;
    if (s == "delete" || s == "del")   return NK_Delete;
    if (s == "up")               return NK_Up;
    if (s == "down")             return NK_Down;
    if (s == "left")             return NK_Left;
    if (s == "right")            return NK_Right;
    if (s == "home")             return NK_Home;
    if (s == "end")              return NK_End;
    if (s == "pageup" || s == "pgup")   return NK_PageUp;
    if (s == "pagedown" || s == "pgdn") return NK_PageDown;
    if (s == "space")            return NK_Space;
    return NK_None;
}

// GLFW key code with a shift modifier produces a shifted character on US-ish
// layouts; used to keep typed punctuation consistent with keybindings.json.
int shiftedPunct(int glfwKey) {
    switch (glfwKey) {
        case GLFW_KEY_1: return '!'; case GLFW_KEY_2: return '@';
        case GLFW_KEY_3: return '#'; case GLFW_KEY_4: return '$';
        case GLFW_KEY_5: return '%'; case GLFW_KEY_6: return '^';
        case GLFW_KEY_7: return '&'; case GLFW_KEY_8: return '*';
        case GLFW_KEY_9: return '('; case GLFW_KEY_0: return ')';
        case GLFW_KEY_MINUS: return '_'; case GLFW_KEY_EQUAL: return '+';
        case GLFW_KEY_LEFT_BRACKET:  return '{';  case GLFW_KEY_RIGHT_BRACKET: return '}';
        case GLFW_KEY_BACKSLASH: return '|';    case GLFW_KEY_SEMICOLON: return ':';
        case GLFW_KEY_APOSTROPHE: return '"';   case GLFW_KEY_GRAVE_ACCENT: return '~';
        case GLFW_KEY_COMMA: return '<'; case GLFW_KEY_PERIOD: return '>';
        case GLFW_KEY_SLASH: return '?';
    }
    return 0;
}

NamedKey namedFromGlfw(int glfwKey) {
    switch (glfwKey) {
        case GLFW_KEY_ENTER:       return NK_Enter;
        case GLFW_KEY_KP_ENTER:    return NK_Enter;
        case GLFW_KEY_ESCAPE:      return NK_Escape;
        case GLFW_KEY_TAB:         return NK_Tab;
        case GLFW_KEY_BACKSPACE:   return NK_Backspace;
        case GLFW_KEY_DELETE:      return NK_Delete;
        case GLFW_KEY_UP:          return NK_Up;
        case GLFW_KEY_DOWN:        return NK_Down;
        case GLFW_KEY_LEFT:        return NK_Left;
        case GLFW_KEY_RIGHT:       return NK_Right;
        case GLFW_KEY_HOME:        return NK_Home;
        case GLFW_KEY_END:         return NK_End;
        case GLFW_KEY_PAGE_UP:     return NK_PageUp;
        case GLFW_KEY_PAGE_DOWN:   return NK_PageDown;
        case GLFW_KEY_SPACE:       return NK_Space;
    }
    return NK_None;
}

uint8_t modsFrom(int glfwMods) {
    uint8_t m = Mod_None;
    if (glfwMods & GLFW_MOD_CONTROL) m |= Mod_Ctrl;
    if (glfwMods & GLFW_MOD_ALT)     m |= Mod_Alt;
    if (glfwMods & GLFW_MOD_SHIFT)   m |= Mod_Shift;
    return m;
}

void keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods) {
    (void)win; (void)scancode;
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    uint8_t m = modsFrom(mods);
    const bool modded = (mods & GLFW_MOD_CONTROL) || (mods & GLFW_MOD_ALT);

    if (NamedKey nk = namedFromGlfw(key); nk != NK_None) {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_queue.push_back(chord(m, (int)nk));
        return;
    }

    // Printable keys: letters arrive via the char callback unless ctrl/alt is
    // held (the backend still reports the raw key here because ctrl+letter
    // normally produces no character event we care about). Non-letter printable
    // keys (digits/punctuation) also come through the char callback.
    if (!modded) return;

    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        char base = (char)('a' + (key - GLFW_KEY_A));
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_queue.push_back(chord(m, (int)base));
    } else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_queue.push_back(chord(m, (int)('0' + (key - GLFW_KEY_0))));
    }
}

void charCallback(GLFWwindow* win, unsigned int codepoint) {
    if (codepoint < 32 || codepoint > 0x7E) return;
    if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(win, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
        glfwGetKey(win, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) {
        return; // handled by keyCallback (combo chords)
    }

    char ch = (char)codepoint;
    uint8_t m = Mod_None;
    if (std::isalpha((unsigned char)ch)) {
        if (std::isupper((unsigned char)ch)) {
            m |= Mod_Shift;
            ch = (char)std::tolower((unsigned char)ch);
        }
    }
    std::lock_guard<std::mutex> lock(g_queueMutex);
    g_queue.push_back(chord(m, (int)ch));
}

} // namespace

std::string chordToString(const KeyChord& c) {
    std::string out;
    if (c.mods & Mod_Ctrl)  out += "ctrl+";
    if (c.mods & Mod_Alt)   out += "alt+";
    if (c.mods & Mod_Shift) out += "shift+";
    if (const char* n = namedName(c.base)) {
        out += n;
    } else if (c.base >= 32 && c.base <= 126) {
        char ch = (char)c.base;
        if ((c.mods & Mod_Shift) && std::isalpha((unsigned char)ch)) {
            out += (char)std::toupper((unsigned char)ch);
        } else {
            out += ch;
        }
    } else {
        out += "?";
    }
    return out;
}

std::optional<KeyChord> chordFromString(const std::string& s) {
    KeyChord c;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t plus = s.find('+', pos);
        size_t end = (plus == std::string::npos) ? s.size() : plus;
        std::string part = s.substr(pos, end - pos);
        if (part == "ctrl") {
            c.mods |= Mod_Ctrl;
        } else if (part == "alt") {
            c.mods |= Mod_Alt;
        } else if (part == "shift") {
            c.mods |= Mod_Shift;
        } else {
            if (part.size() == 1) {
                char ch = part[0];
                if (ch >= 'A' && ch <= 'Z') {
                    c.mods |= Mod_Shift;
                    c.base = (int)std::tolower(ch) ;
                } else {
                    c.base = (int)ch;
                }
            } else {
                int nk = namedFromName(part);
                if (nk == NK_None) return std::nullopt;
                c.base = nk;
            }
        }
        if (plus == std::string::npos) break;
        pos = plus + 1;
    }
    return c;
}

void installKeyCallbacks(GLFWwindow* win) {
    g_window = win;
    glfwSetKeyCallback(win, keyCallback);
    glfwSetCharCallback(win, charCallback);
}

std::vector<KeyChord> pollChords() {
    std::vector<KeyChord> out;
    std::lock_guard<std::mutex> lock(g_queueMutex);
    out.swap(g_queue);
    return out;
}

} // namespace kadath