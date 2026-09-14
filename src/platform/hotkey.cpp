#include "platform/hotkey.hpp"

#include "keyboard/keys.hpp"

#include <atomic>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <poll.h>
#endif

namespace kadath {

#ifdef _WIN32

// ---------------------------------------------------------------------------
// Windows: RegisterHotKey + message pump on a dedicated thread
// ---------------------------------------------------------------------------

namespace {

unsigned translateVk(const KeyChord& c) {
    if (c.base >= 'a' && c.base <= 'z') return (unsigned)toupper(c.base);
    if (c.base >= '0' && c.base <= '9') return (unsigned)c.base;
    switch (c.base) {
        case NK_Enter:     return VK_RETURN;
        case NK_Escape:    return VK_ESCAPE;
        case NK_Tab:       return VK_TAB;
        case NK_Backspace: return VK_BACK;
        case NK_Delete:    return VK_DELETE;
        case NK_Up:        return VK_UP;
        case NK_Down:      return VK_DOWN;
        case NK_Left:      return VK_LEFT;
        case NK_Right:     return VK_RIGHT;
        case NK_Home:      return VK_HOME;
        case NK_End:       return VK_END;
        case NK_PageUp:    return VK_PRIOR;
        case NK_PageDown:  return VK_NEXT;
        case NK_Space:     return VK_SPACE;
    }
    return 0;
}

} // namespace

struct Hotkey::Impl {
    static constexpr int ID = 0x4B44; // "KD"
    std::atomic<bool> running{false};
    std::thread thread;
    std::function<void()> cb;

    void loop(unsigned vk, unsigned mods) {
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (msg.message == WM_HOTKEY && msg.wParam == ID) {
                if (cb) cb();
            }
        }
    }
};

bool Hotkey::start(const KeyChord& chord, std::function<void()> onTrigger) {
    if (impl->running) return true;
    unsigned vk = translateVk(chord);
    if (!vk) return false;

    unsigned mods = 0;
    if (chord.mods & Mod_Ctrl)  mods |= MOD_CONTROL;
    if (chord.mods & Mod_Alt)   mods |= MOD_ALT;
    if (chord.mods & Mod_Shift) mods |= MOD_SHIFT;
#ifdef MOD_NOREPEAT
    mods |= MOD_NOREPEAT;
#endif

    impl->cb = std::move(onTrigger);
    impl->running = true;
    impl->thread = std::thread([this, vk, mods] {
        if (RegisterHotKey(nullptr, Hotkey::Impl::ID, mods, vk)) {
            impl->loop(vk, mods);
            UnregisterHotKey(nullptr, Hotkey::Impl::ID);
        }
        impl->running = false;
    });
    return true;
}

void Hotkey::stop() {
    if (!impl->running) return;
    impl->running = false;
    if (impl->thread.joinable()) {
        // Loop is blocked in GetMessageW; post WM_QUIT to unblock it.
        PostThreadMessageW(GetThreadId(impl->thread.native_handle()), WM_QUIT, 0, 0);
        impl->thread.join();
    }
}

#else

// ---------------------------------------------------------------------------
// X11: XGrabKey on the root window + event loop on a dedicated thread
// ---------------------------------------------------------------------------

namespace {

// Keysym for a KeyChord base. Letters/digits use their ASCII value; named keys
// map through lookups. Returns NoSymbol for unsupported keys.
KeySym keysymForBase(int base) {
    if (base >= 32 && base <= 126) {
        char s[2] = {(char)base, '\0'};
        return XStringToKeysym(s[0] == ' ' ? "space" : s);
    }
    switch (base) {
        case NK_Enter:     return XK_Return;
        case NK_Escape:    return XK_Escape;
        case NK_Tab:       return XK_Tab;
        case NK_Backspace: return XK_BackSpace;
        case NK_Delete:    return XK_Delete;
        case NK_Up:        return XK_Up;
        case NK_Down:      return XK_Down;
        case NK_Left:      return XK_Left;
        case NK_Right:     return XK_Right;
        case NK_Home:      return XK_Home;
        case NK_End:       return XK_End;
        case NK_PageUp:    return XK_Page_Up;
        case NK_PageDown:  return XK_Page_Down;
        case NK_Space:     return XK_space;
    }
    return NoSymbol;
}

} // namespace

struct Hotkey::Impl {
    std::atomic<bool> running{false};
    std::thread thread;
    std::function<void()> cb;
};

bool Hotkey::start(const KeyChord& chord, std::function<void()> onTrigger) {
    if (impl->running) return true;

    impl->cb = std::move(onTrigger);
    impl->running = true;

    impl->thread = std::thread([this, chord] {
        Display* d = XOpenDisplay(nullptr);
        if (!d) {
            impl->running = false;
            return;
        }
        KeySym sym = keysymForBase(chord.base);
        if (sym == NoSymbol) {
            XCloseDisplay(d);
            impl->running = false;
            return;
        }
        KeyCode keycode = XKeysymToKeycode(d, sym);
        Window root = DefaultRootWindow(d);
        if (!keycode) {
            XCloseDisplay(d);
            impl->running = false;
            return;
        }

        unsigned baseMask = 0;
        if (chord.mods & Mod_Ctrl)  baseMask |= ControlMask;
        if (chord.mods & Mod_Alt)   baseMask |= Mod1Mask;
        if (chord.mods & Mod_Shift) baseMask |= ShiftMask;

        // Grab with every combination of the common "lock" modifiers so the
        // hotkey still fires with NumLock/CapsLock/ScrollLock active.
        static const unsigned lockMods[] = {0, LockMask, Mod2Mask, Mod5Mask};
        for (unsigned lm : lockMods) {
            XGrabKey(d, keycode, baseMask | lm, root, True, GrabModeAsync, GrabModeAsync);
        }
        XFlush(d);

        bool grabbed = true;
        const int fd = ConnectionNumber(d);
        while (impl->running) {
            struct pollfd p;
            p.fd = fd;
            p.events = POLLIN;
            int r = poll(&p, 1, 100);
            if (r <= 0) continue;
            if (p.revents & POLLHUP) break;
            while (XPending(d)) {
                XEvent ev;
                XNextEvent(d, &ev);
                if (ev.type == KeyPress) {
                    if (impl->cb) impl->cb();
                }
            }
        }

        if (grabbed) {
            for (unsigned lm : lockMods) {
                XUngrabKey(d, keycode, baseMask | lm, root);
            }
            XFlush(d);
        }
        XCloseDisplay(d);
        impl->running = false;
    });
    return true;
}

void Hotkey::stop() {
    if (!impl->running) return;
    impl->running = false;
    // Wake the blocked XNextEvent by synthesizing a no-op key grab+release.
    if (impl->thread.joinable()) impl->thread.join();
}

#endif // _WIN32 / X11

Hotkey::Hotkey() : impl(std::make_unique<Impl>()) {}
Hotkey::~Hotkey() { stop(); }

} // namespace kadath