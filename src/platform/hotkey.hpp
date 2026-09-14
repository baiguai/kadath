#pragma once

#include <functional>
#include <memory>

namespace kadath {

struct KeyChord;

// Global hotkey (works while the app window is hidden). Implemented with
// XGrabKey on X11 / RegisterHotKey on Windows. The callback runs on a
// background thread: keep it cheap (e.g. set a flag read by the main loop).
struct Hotkey {
    Hotkey();
    ~Hotkey();
    Hotkey(const Hotkey&) = delete;
    Hotkey& operator=(const Hotkey&) = delete;

    // Start watching for `chord`. Returns false if the platform backend could
    // not be initialized (e.g. no X display) or the chord is invalid.
    bool start(const KeyChord& chord, std::function<void()> onTrigger);
    void stop();

    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace kadath