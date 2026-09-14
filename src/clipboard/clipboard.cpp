#include "clipboard/clipboard.hpp"
#include "util.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <poll.h>
#include <unistd.h>
#endif

namespace kadath::clipboard {

namespace {

std::mutex g_lastWrittenMutex;
std::string g_lastWritten;

void setLastWritten(const std::string& t) {
    std::lock_guard<std::mutex> lock(g_lastWrittenMutex);
    g_lastWritten = t;
}

bool isLastWritten(const std::string& t) {
    std::lock_guard<std::mutex> lock(g_lastWrittenMutex);
    return t == g_lastWritten;
}

} // namespace

// ---------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------
#ifdef _WIN32

namespace {

std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), n);
    return out;
}

std::string windowsClipboardNow() {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return {};
    if (!OpenClipboard(nullptr)) return {};
    std::string result;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* data = (const wchar_t*)GlobalLock(h);
        if (data) {
            result = wideToUtf8(data);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return result;
}

} // namespace

std::string read() {
    return windowsClipboardNow();
}

bool write(const std::string& text) {
    std::wstring w = utf8ToWide(text);
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            const size_t bytes = (w.size() + 1) * sizeof(wchar_t);
            HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (!h) {
                CloseClipboard();
                return false;
            }
            void* mem = GlobalLock(h);
            if (!mem) {
                GlobalFree(h);
                CloseClipboard();
                return false;
            }
            std::memcpy(mem, w.c_str(), bytes);
            GlobalUnlock(h);
            SetClipboardData(CF_UNICODETEXT, h);
            CloseClipboard();
            setLastWritten(text);
            return true;
        }
        Sleep(5);
    }
    return false;
}

struct Watcher::Impl {
    std::atomic<bool> running{false};
    std::thread thread;
    ChangeCallback cb;
    HWND hwnd = nullptr;

    static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
        Impl* self = (Impl*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (msg == WM_CLIPBOARDUPDATE) {
            if (self) {
                std::string text = windowsClipboardNow();
                if (!text.empty() && !isLastWritten(text) && self->cb) self->cb(text);
            }
            return 0;
        }
        return DefWindowProcW(h, msg, w, l);
    }

    static void loop(HWND h) {
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
};

void Watcher::start(ChangeCallback cb) {
    if (impl->running) return;
    impl->cb = std::move(cb);
    impl->running = true;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &Watcher::Impl::wndProc;
    wc.lpszClassName = L"KadathClipboardWatcher";
    RegisterClassExW(&wc);

    impl->hwnd = CreateWindowExW(0, wc.lpszClassName, L"kadath-watcher", 0,
                                 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    SetWindowLongPtrW(impl->hwnd, GWLP_USERDATA, (LONG_PTR)impl.get());
    AddClipboardFormatListener(impl->hwnd);

    impl->thread = std::thread(&Watcher::Impl::loop, impl->hwnd);
}

void Watcher::stop() {
    if (!impl->running) return;
    impl->running = false;
    if (impl->hwnd) {
        RemoveClipboardFormatListener(impl->hwnd);
        PostMessageW(impl->hwnd, WM_CLOSE, 0, 0);
    }
    if (impl->thread.joinable()) impl->thread.join();
    if (impl->hwnd) {
        DestroyWindow(impl->hwnd);
        impl->hwnd = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Linux / X11
// ---------------------------------------------------------------------------
#else

struct Watcher::Impl {
    std::atomic<bool> running{false};
    std::thread thread;
    ChangeCallback cb;
};

namespace {

struct XShared {
    std::mutex mu;
    std::deque<std::string> jobs; // pending write() payloads
    std::string owned;            // content while we own the selection
    bool hasOwned = false;
    ChangeCallback cb;
    int wakePipeR = -1; // poll wakeup for write()/stop()
    int wakePipeW = -1;
};

XShared* g_shared = nullptr;
std::mutex g_sharedMutex;

// Wake the watcher thread so it picks up pending jobs promptly.
void pokeWatcher() {
    std::lock_guard<std::mutex> lock(g_sharedMutex);
    if (g_shared && g_shared->wakePipeW >= 0) {
        char one = 'x';
        ssize_t r = ::write(g_shared->wakePipeW, &one, 1);
        (void)r;
    }
}

} // namespace

std::string read() {
    // On Linux the watcher thread owns the request/response exchange; this
    // returns our own last-written value (what the app itself put there).
    std::lock_guard<std::mutex> lock(g_lastWrittenMutex);
    return g_lastWritten;
}

bool write(const std::string& text) {
    {
        std::lock_guard<std::mutex> lock(g_sharedMutex);
        if (!g_shared) return false;
    }
    {
        std::lock_guard<std::mutex> lock(g_shared->mu);
        g_shared->jobs.push_back(text);
    }
    pokeWatcher();
    setLastWritten(text);
    return true;
}

void Watcher::start(ChangeCallback cb) {
    if (impl->running) return;
    impl->cb = std::move(cb);

    auto sh = std::make_unique<XShared>();
    {
        std::lock_guard<std::mutex> lock(g_sharedMutex);
        g_shared = sh.get();
    }

    impl->running = true;
    impl->thread = std::thread([this, sh = std::move(sh)]() mutable {
        // --- X11 init (this thread owns the Display) ----------------------
        XInitThreads();
        Display* d = XOpenDisplay(nullptr);
        if (!d) {
            impl->running = false;
            std::lock_guard<std::mutex> lock(g_sharedMutex);
            g_shared = nullptr;
            return;
        }
        const int screen = DefaultScreen(d);
        Window win = XCreateSimpleWindow(d, RootWindow(d, screen), 0, 0, 1, 1, 0,
                                         BlackPixel(d, screen), BlackPixel(d, screen));

        const Atom clipboard = XInternAtom(d, "CLIPBOARD", False);
        const Atom utf8 = XInternAtom(d, "UTF8_STRING", False);
        const Atom targets = XInternAtom(d, "TARGETS", False);
        const Atom property = XInternAtom(d, "KADATH_CLIP", False);

        int xfixesEvBase = 0, xfixesErrBase = 0;
        XFixesQueryExtension(d, &xfixesEvBase, &xfixesErrBase); // ensure linked
        {
            int major = 0, minor = 0;
            if (XFixesQueryExtension(d, &major, &minor)) {
                XFixesSelectSelectionInput(d, win, clipboard, XFixesSetSelectionOwnerNotifyMask);
            }
        }

        bool havePipe = false;
        {
            int p[2];
            if (pipe(p) == 0) {
                havePipe = true;
                std::lock_guard<std::mutex> lock(sh->mu);
                sh->wakePipeR = p[0];
                sh->wakePipeW = p[1];
            }
        }

        auto applyJob = [&]() {
            std::string text;
            {
                std::lock_guard<std::mutex> lock(sh->mu);
                if (sh->jobs.empty()) return;
                text = std::move(sh->jobs.front());
                sh->jobs.pop_front();
            }
            std::lock_guard<std::mutex> lock(sh->mu);
            sh->owned = text;
            sh->hasOwned = true;
            XSetSelectionOwner(d, win, clipboard, CurrentTime);
            XFlush(d);
        };

        auto serveRequest = [&](XSelectionRequestEvent* req) {
            XEvent respond{};
            respond.xselection.type = SelectionNotify;
            respond.xselection.display = req->display;
            respond.xselection.requestor = req->requestor;
            respond.xselection.selection = req->selection;
            respond.xselection.target = req->target;
            respond.xselection.time = req->time;
            respond.xselection.property = req->property;

            bool ok = false;
            if (req->target == utf8 || req->target == XA_STRING) {
                std::lock_guard<std::mutex> lock(sh->mu);
                XChangeProperty(d, req->requestor, req->property, req->target, 8,
                                PropModeReplace,
                                (const unsigned char*)sh->owned.data(),
                                (int)sh->owned.size());
                ok = true;
            } else if (req->target == targets) {
                Atom list[2] = {utf8, XA_STRING};
                XChangeProperty(d, req->requestor, req->property, XA_ATOM, 32,
                                PropModeReplace, (const unsigned char*)list, 2);
                ok = true;
            }
            respond.xselection.property = ok ? req->property : None;
            XSendEvent(d, req->requestor, False, 0, &respond);
            XFlush(d);
        };

        // Request the current owner's text and wait (bounded) for the result.
        auto fetchForeignClip = [&]() {
            Window owner = XGetSelectionOwner(d, clipboard);
            if (owner == win) return;         // we own it - nothing to fetch
            if (owner == None) return;

            XConvertSelection(d, clipboard, utf8, property, win, CurrentTime);
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(800);
            const int fd = ConnectionNumber(d);
            while (std::chrono::steady_clock::now() < deadline) {
                struct pollfd p;
                p.fd = fd;
                p.events = POLLIN;
                int r = poll(&p, 1, 50);
                if (r <= 0) continue;
                while (XPending(d)) {
                    XEvent ev;
                    XNextEvent(d, &ev);
                    if (ev.type == SelectionRequest) {
                        serveRequest(&ev.xselectionrequest);
                    } else if (ev.type == SelectionNotify) {
                        XSelectionEvent* se = &ev.xselection;
                        if (se->property != property) continue;
                        Atom type;
                        int format;
                        unsigned long nitems, after;
                        unsigned char* data = nullptr;
                        if (XGetWindowProperty(d, win, property, 0, (long)(4 * 1024 * 1024),
                                               True, AnyPropertyType, &type, &format,
                                               &nitems, &after, &data) == Success && data) {
                            std::string text((const char*)data, nitems);
                            XFree(data);
                            if (!text.empty() && !isLastWritten(text) && sh->cb) {
                                ChangeCallback cb2 = sh->cb;
                                cb2(text);
                            }
                        }
                        return;
                    }
                }
            }
        };

        // --- event loop ----------------------------------------------------
        while (impl->running) {
            const int fd = ConnectionNumber(d);
            struct pollfd p;
            p.fd = fd;
            p.events = POLLIN;

            int r = havePipe ? poll(&p, 1, 100) : poll(&p, 1, 100);

            // Drain the wake pipe first so a job wakes us even if X has nothing.
            if (havePipe) {
                char buf[64];
                while (::read(sh->wakePipeR, buf, sizeof(buf)) > 0) {}
            }

            if (r > 0 && (p.revents & (POLLIN | POLLHUP))) {
                while (XPending(d)) {
                    XEvent ev;
                    XNextEvent(d, &ev);
                    switch (ev.type) {
                        case SelectionRequest:
                            serveRequest(&ev.xselectionrequest);
                            break;
                        case SelectionNotify: {
                            XSelectionEvent* se = &ev.xselection;
                            if (se->selection != clipboard) break;
                            if (se->property != None) {
                                // Result of one of our XConvertSelection calls.
                                Atom type;
                                int format;
                                unsigned long nitems, after;
                                unsigned char* data = nullptr;
                                if (XGetWindowProperty(d, win, property, 0, (long)(4 * 1024 * 1024),
                                                       True, AnyPropertyType, &type, &format,
                                                       &nitems, &after, &data) == Success && data) {
                                    std::string text((const char*)data, nitems);
                                    XFree(data);
                                    if (!text.empty() && !isLastWritten(text) && sh->cb) {
                                        ChangeCallback cb2 = sh->cb;
                                        cb2(text);
                                    }
                                }
                            } else {
                                // Ownership changed (XFixes). Skip our own gain.
                                fetchForeignClip();
                            }
                            break;
                        }
                        default:
                            break;
                    }
                }
            }

            // Pending write() jobs (the pump above may also have processed them).
            bool job;
            {
                std::lock_guard<std::mutex> lock(sh->mu);
                job = !sh->jobs.empty();
            }
            if (job) applyJob();
        }

        // --- teardown ------------------------------------------------------
        {
            std::lock_guard<std::mutex> lock(sh->mu);
            if (sh->wakePipeR >= 0) ::close(sh->wakePipeR);
            if (sh->wakePipeW >= 0) ::close(sh->wakePipeW);
            sh->wakePipeR = sh->wakePipeW = -1;
        }
        XDestroyWindow(d, win);
        XCloseDisplay(d);
        {
            std::lock_guard<std::mutex> lock(g_sharedMutex);
            g_shared = nullptr;
        }
    });
}

void Watcher::stop() {
    if (!impl->running) return;
    impl->running = false;
    pokeWatcher();
    if (impl->thread.joinable()) impl->thread.join();
}

#endif // _WIN32 / X11

Watcher::Watcher() : impl(std::make_unique<Impl>()) {}
Watcher::~Watcher() { stop(); }

} // namespace kadath::clipboard