#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace kadath {

// Modifier bits used in KeyChord.
enum : uint8_t {
    Mod_None   = 0,
    Mod_Ctrl   = 1 << 0,
    Mod_Alt    = 1 << 1,
    Mod_Shift  = 1 << 2,
};

// Named (non-printable) keys. Printable characters use their ASCII value as
// the KeyChord base instead.
enum NamedKey : int {
    NK_None      = 0,
    NK_Enter     = 1,
    NK_Escape    = 2,
    NK_Tab       = 3,
    NK_Backspace = 4,
    NK_Delete    = 5,
    NK_Up        = 6,
    NK_Down      = 7,
    NK_Left      = 8,
    NK_Right     = 9,
    NK_Home      = 10,
    NK_End       = 11,
    NK_PageUp    = 12,
    NK_PageDown  = 13,
    NK_Space     = 14,
};

// A single key chord, e.g. "ctrl+alt+c" or "shift+d" or just "j".
struct KeyChord {
    uint8_t mods = Mod_None;
    int base = NK_None; // ASCII ('a'..'z' lowercased, or punctuation) or NamedKey

    bool operator==(const KeyChord& o) const { return mods == o.mods && base == o.base; }
    bool operator!=(const KeyChord& o) const { return !(*this == o); }
    bool operator<(const KeyChord& o) const {
        if (mods != o.mods) return mods < o.mods;
        return base < o.base;
    }
    bool empty() const { return base == NK_None; }
};

inline KeyChord chord(uint8_t mods, int base) { KeyChord c; c.mods = mods; c.base = base; return c; }

// The printable character a chord represents when typed for input (0 if none).
inline char chordChar(const KeyChord& c) {
    if (c.base >= 32 && c.base <= 126) {
        char ch = (char)c.base;
        if ((c.mods & Mod_Shift) && std::isalpha((unsigned char)ch)) {
            return (char)std::toupper((unsigned char)ch);
        }
        return ch;
    }
    return 0;
}

// Canonical text form, e.g. "j", "D", "ctrl+alt+c", "enter", "up". This is the
// format used in keybindings.json.
std::string chordToString(const KeyChord& c);

// Parse a canonical keybinding string. Returns std::nullopt on malformed input.
std::optional<KeyChord> chordFromString(const std::string& s);

// Install GLFW key + char callbacks that collect raw keyboard input into an
// internal queue. Call once after creating the window.
void installKeyCallbacks(GLFWwindow* win);

// Drain the collected input as a chord list (in chronological order). This is
// the single source of keyboard input for the app. The queue data is sourced
// directly from GLFW, independent of ImGui state.
std::vector<KeyChord> pollChords();

} // namespace kadath