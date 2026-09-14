#pragma once

#include <mutex>
#include <atomic>
#include <set>
#include <string>
#include <vector>

#include "actions.hpp"
#include "clipboard/clipboard.hpp"
#include "keyboard/keymap.hpp"
#include "platform/hotkey.hpp"
#include "platform/window.hpp"
#include "storage/config.hpp"
#include "storage/store.hpp"
#include "storage/theme.hpp"

namespace kadath {

// For what purpose a prompt (single-line text input) is open.
enum class PromptKind {
    None,
    NewGroup,          // bookmarks: create a group under the current folder
    RenameNode,        // bookmarks: rename the selected node (groups only)
    AddClipToGroup,    // main: add the selected clip to a slash-separated path
};

// Application core: owns all modules, dispatches keymap actions, and renders
// the current context through the UI views.
class App {
public:
    bool init();
    void run();
    void shutdown();

    // --- public state (read/write by views) --------------------------------
    Window window;
    AppConfig config;
    Theme theme;
    Store store;
    Keymap keymap;

    Context context = Context::Main;

    // main list (selection is an index into `filtered`; when filterText is
    // empty, filtered is the identity mapping of store.clips).
    int selection = 0;
    std::string filterText;
    std::vector<size_t> filtered;
    int scroll = 0;
    int repeatPending = 0;

    // command mode
    std::string commandText;
    std::vector<std::string> commandHistory;
    int commandHistIndex = -1;

    // edit mode
    std::string editText;
    int editRow = 0, editCol = 0;
    size_t editTarget = 0;   // index into store.clips being edited

    // prompt
    PromptKind promptKind = PromptKind::None;
    std::string promptLabel;
    std::string promptInput;
    std::string promptSeed;
    size_t promptCursor = 0;
    Context returnContext = Context::Main;

    // bookmarks tree (expand/collapse; all levels drawn at once)
    std::vector<size_t> bmPath;      // path to the selected node (indices from root)
    std::set<std::vector<size_t>> bmExpanded; // paths of currently expanded groups

    // pinned list
    int pinSelection = 0;

    // help
    int helpScroll = 0;

    // status line
    std::string status;
    bool pendingStatus = false;

    bool running = true;

    // Helpers the views call.
    struct FlatNode {
        std::vector<size_t> path;
        int depth = 0;
        bool isGroup = false;
        bool expanded = false;
    };
    std::vector<FlatNode> flattenBookmarks() const;
    size_t visibleCount() const;
    const Clip& visibleClip(size_t i) const;   // flat view -> store clip
    std::string promptInputVisible() const;    // current prompt text to draw
    bool filterActive() const { return !filterText.empty(); }

    // Called from worker threads (kept tiny, just marshals).
    void onClipboardChanged(const std::string& text);
    void onHotkeyToggle();

private:
    clipboard::Watcher watcher_;
    Hotkey hotkey_;

    std::mutex incomingMutex_;
    std::string incomingClip_;
    struct { std::atomic<bool> toggled{false}; } flags_;

    // --- input -------------------------------------------------------------
    void handleChord(const KeyChord& chord);
    void handleAction(Action a);
    void insertChar(char ch);          // filter/command/prompt text append

    // --- per-action helpers ------------------------------------------------
    void setContext(Context c);
    void setStatus(std::string s);
    void rebuildFilter();
    size_t resolveSelected() const;    // index into store.clips from selection
    void clampSelection(int maxIndex);
    void scrollToSelection(bool force = false);

    void hideNow();
    bool toggleVisible();

    void doCopy();
    void bmScrollTo(int flatIndex);

    const Group* nodeGroup(const std::vector<size_t>& full) const;
    const Clip* nodeClip(const std::vector<size_t>& full) const;
    const Group* nodeAtPath(const std::vector<size_t>& path) const;
    std::string nodeNameAt(const std::vector<size_t>& full) const;
    void openEditorForClip(const std::string& text);

    // command mode
    void runCommand(const std::string& cmd);

    // edit mode text ops
    void moveCursor(int dr, int dc);
    void editInsert(char ch);
    void editBackspace();
    void editForwardDelete();
    void editInsertNewline();

    // prompt completion
    void finishPrompt();

    // --- render ------------------------------------------------------------
    void render();

    // pending work pumped once per frame
    void drainIncoming();
};

} // namespace kadath