#pragma once

#include <functional>
#include <memory>
#include <string>

namespace kadath::clipboard {

using ChangeCallback = std::function<void(const std::string&)>;

// Read/write the system clipboard. write() is asynchronous on X11 (ownership
// is handed to the background watcher thread which also serves requests).
std::string read();
bool write(const std::string& text);

// Watches the clipboard for changes made by other programs and reports new
// content through the callback. Also owns the X11 selection while kadath holds
// the clipboard, so write() needs no external tools (xclip & co).
struct Watcher {
    Watcher();
    ~Watcher();
    Watcher(const Watcher&) = delete;
    Watcher& operator=(const Watcher&) = delete;

    void start(ChangeCallback cb);
    void stop();

    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace kadath::clipboard