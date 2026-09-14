#include "app.hpp"

#include "keyboard/keys.hpp"
#include "storage/theme.hpp"
#include "ui/command.hpp"
#include "ui/edit.hpp"
#include "ui/help.hpp"
#include "ui/main_view.hpp"
#include "ui/pinned.hpp"
#include "ui/prompt.hpp"
#include "ui/render.hpp"
#include "ui/bookmarks.hpp"
#include "util.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace kadath {

using json = nlohmann::json;

namespace {

int flatIndexOf(const std::vector<App::FlatNode>& flat, const std::vector<size_t>& path) {
    for (size_t i = 0; i < flat.size(); ++i) {
        if (flat[i].path == path) return (int)i;
    }
    return -1;
}

struct LineBounds {
    size_t start;
    size_t end;      // one past last byte of the line (before '\n')
};

LineBounds lineBounds(const std::string& s, size_t row) {
    size_t start = 0;
    size_t r = 0;
    while (r < row) {
        size_t nl = s.find('\n', start);
        if (nl == std::string::npos) return {s.size(), s.size()};
        start = nl + 1;
        ++r;
    }
    size_t nl = s.find('\n', start);
    size_t end = (nl == std::string::npos) ? s.size() : nl;
    return {start, end};
}

size_t lineCount(const std::string& s) {
    if (s.empty()) return 1;
    size_t n = 1;
    for (char ch : s) if (ch == '\n') ++n;
    return n;
}

std::string toLower(const std::string& s) {
    std::string out = s;
    for (auto& ch : out) ch = (char)std::tolower((unsigned char)ch);
    return out;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

bool isTextContext(Context c) {
    return c == Context::Filter || c == Context::Command ||
           c == Context::Prompt || c == Context::Edit;
}

} // namespace

// ---------------------------------------------------------------------------
// Init / teardown
// ---------------------------------------------------------------------------

bool App::init() {
    config.load();
    theme = loadTheme(config.themesDir, config.theme);
    store.load(config);
    keymap = Keymap::defaults();

    // User overrides from keybindings.json (also selects the global hotkey).
    {
        std::string text = readFile(config.keybindingsFile);
        if (!text.empty()) {
            try {
                json j = json::parse(text);
                if (j.contains("global") && j["global"].is_object()) {
                    const auto& g = j["global"];
                    if (g.contains("toggle_window") && g["toggle_window"].is_array()) {
                        auto bind = g["toggle_window"].get<std::vector<std::string>>();
                        if (!bind.empty()) {
                            if (auto ch = chordFromString(bind[0]); ch && !ch->empty()) {
                                if (hotkey_.start(*ch, [this] { onHotkeyToggle(); })) {
                                    /* global hotkey armed */
                                }
                            }
                        }
                    }
                    for (auto it = g.begin(); it != g.end(); ++it) {
                        if (it.key() == "toggle_window") continue;
                        for (const auto& spec : it.value().get<std::vector<std::string>>()) {
                            keymap.add(Context::Global, it.key(), Action::None); // placeholder (handled below)
                        }
                    }
                }
                if (j.contains("contexts") && j["contexts"].is_object()) {
                    const auto& ctxs = j["contexts"];
                    for (auto it = ctxs.begin(); it != ctxs.end(); ++it) {
                        auto ctx = contextFromString(it.key());
                        if (!ctx || *ctx == Context::Global) continue;
                        if (!it.value().is_object()) continue;
                        for (auto b = it.value().begin(); b != it.value().end(); ++b) {
                            auto action = actionFromString(b.key());
                            if (!action) continue;
                            if (b.value().is_array()) {
                                for (const auto& spec : b.value()) {
                                    if (spec.is_string()) keymap.add(*ctx, spec.get<std::string>(), *action);
                                }
                            }
                        }
                    }
                }
            } catch (...) {
            }
        }
    }

    std::string fontFile = config.fontFile.empty()
                               ? exeDir() + "/assets/fonts/JetBrainsMono-Regular.ttf"
                               : config.fontFile;
    if (!window.init(fontFile, config.fontPx, config.winW, config.winH, "kadath")) {
        return false;
    }
    installKeyCallbacks(window.win);
    if (config.winX >= 0 && config.winY >= 0) {
        glfwSetWindowPos(window.win, config.winX, config.winY);
    }
    window.show();

    watcher_.start([this](const std::string& t) { onClipboardChanged(t); });

    rebuildFilter();
    return true;
}

void App::shutdown() {
    watcher_.stop();
    hotkey_.stop();
    window.shutdown();
}

void App::run() {
    while (running && !window.shouldClose()) {
        window.pollEvents();
        window.beginFrame();
        if (flags_.toggled.exchange(false)) toggleVisible();
        drainIncoming();

        std::vector<KeyChord> chords = pollChords();
        for (const auto& chord : chords) handleChord(chord);

        if (window.visible()) {
            // page* actions need the current row count
            ImGuiIO& io = ImGui::GetIO();
            (void)io;
            render();
        }
        window.endFrame();
    }

    // Persist on clean exit.
    config.winX = window.posX();
    config.winY = window.posY();
    config.winW = window.width();
    config.winH = window.height();
    config.save();
    store.save(config);
}

// ---------------------------------------------------------------------------
// Owned-thread marshalling
// ---------------------------------------------------------------------------

void App::onClipboardChanged(const std::string& text) {
    std::lock_guard<std::mutex> lock(incomingMutex_);
    incomingClip_ = text;
}

void App::drainIncoming() {
    std::string clip;
    {
        std::lock_guard<std::mutex> lock(incomingMutex_);
        clip.swap(incomingClip_);
    }
    if (!clip.empty()) {
        store.sinkClip(clip);
        rebuildFilter();
        store.save(config);
        setStatus("captured clipboard (" + std::to_string(store.clips.size()) + " items)");
    }
}

void App::onHotkeyToggle() {
    flags_.toggled.store(true);
}

bool App::toggleVisible() {
    if (window.visible()) {
        config.winX = window.posX();
        config.winY = window.posY();
        config.save();
        hideNow();
        return false;
    }
    window.show();
    return true;
}

void App::hideNow() {
    // Save state so a crash later costs less.
    store.save(config);
    window.hide();
}

// ---------------------------------------------------------------------------
// Input dispatch
// ---------------------------------------------------------------------------

void App::handleChord(const KeyChord& chord) {
    const bool modded = (chord.mods & (Mod_Ctrl | Mod_Alt)) != 0;

    // Repeat-count prefix (vim-style) for list contexts.
    if (!modded && chord.base >= '1' && chord.base <= '9' &&
        (context == Context::Main || context == Context::Bookmarks || context == Context::Pinned)) {
        repeatPending = chord.base - '0';
        setStatus("escape/number pending: " + std::to_string(repeatPending));
        return;
    }

    // Typed characters in text contexts.
    if (isTextContext(context) && !modded) {
        char ch = chordChar(chord);
        if (ch != 0 && chord.base != NK_Enter && chord.base != NK_Escape &&
            chord.base != NK_Backspace && chord.base != NK_Tab && chord.base != NK_Delete) {
            if (context == Context::Edit) {
                editInsert(ch);
            } else if (context == Context::Prompt) {
                promptInput.insert(promptInput.begin() + (ptrdiff_t)promptCursor, ch);
                ++promptCursor;
            } else {
                insertChar(ch);
            }
            return;
        }
    }

    // Newline in the editor is Insert-mode's fundamental action.
    if (context == Context::Edit && !modded && chord.base == NK_Enter) {
        editInsertNewline();
        return;
    }

    Action a = keymap.resolve(context, chord);
    if (a != Action::None) handleAction(a);
}

void App::handleAction(Action a) {
    const int count = std::max(1, repeatPending);

    auto pageSize = [&]() {
        return std::max(1, (int)window.height() / 18);
    };

    switch (a) {
        case Action::Quit:
            running = false;
            return;

        case Action::HideWindow:
            hideNow();
            return;

        case Action::MoveDown: {
            if (context == Context::Edit) { moveCursor(count, 0); return; }
            if (context == Context::Help) { helpScroll += count * 2; clampSelection(1000000); return; }
            if (context == Context::Main || context == Context::Filter) {
                size_t max = visibleCount() ? visibleCount() - 1 : 0;
                selection = (int)std::min<long>((long)max, (long)selection + count);
                scrollToSelection();
            } else if (context == Context::Bookmarks) {
                auto flat = flattenBookmarks();
                if (!flat.empty()) {
                    int idx = flatIndexOf(flat, bmPath);
                    if (idx < 0) idx = 0;
                    idx = std::min<int>((int)flat.size() - 1, idx + count);
                    bmPath = flat[(size_t)idx].path;
                    bmScrollTo(idx);
                }
            } else if (context == Context::Pinned) {
                pinSelection = (int)std::min<long>(
                    store.pinned.empty() ? 0 : (long)store.pinned.size() - 1, pinSelection + count);
                scrollToSelection();
            }
            break;
        }

        case Action::MoveUp: {
            if (context == Context::Edit) { moveCursor(-count, 0); return; }
            if (context == Context::Help) {
                helpScroll = std::max(0, helpScroll - count * 2);
                return;
            }
            if (context == Context::Main || context == Context::Filter) {
                selection = std::max(0, selection - count);
                scrollToSelection();
            } else if (context == Context::Bookmarks) {
                auto flat = flattenBookmarks();
                if (!flat.empty()) {
                    int idx = flatIndexOf(flat, bmPath);
                    if (idx < 0) idx = 0;
                    idx = std::max(0, idx - count);
                    bmPath = flat[(size_t)idx].path;
                    bmScrollTo(idx);
                }
            } else if (context == Context::Pinned) {
                pinSelection = std::max(0, pinSelection - count);
                scrollToSelection();
            }
            break;
        }

        case Action::MoveDownPage: {
            repeatPending = 0;
            int pg = pageSize();
            for (int i = 0; i < pg; ++i) handleAction(Action::MoveDown);
            return;
        }
        case Action::MoveUpPage: {
            repeatPending = 0;
            int pg = pageSize();
            for (int i = 0; i < pg; ++i) handleAction(Action::MoveUp);
            return;
        }

        case Action::JumpTop: {
            if (context == Context::Bookmarks) {
                auto flat = flattenBookmarks();
                if (!flat.empty()) { bmPath = flat.front().path; bmScrollTo(0); }
                scroll = 0;
            } else if (context == Context::Pinned) {
                pinSelection = 0; scroll = 0;
            } else if (context == Context::Help) {
                helpScroll = 0;
            } else if (context == Context::Edit) {
                editCol = 0;
            } else {
                selection = 0; scroll = 0;
            }
            break;
        }

        case Action::JumpBottom: {
            size_t n = visibleCount();
            if (context == Context::Bookmarks) {
                auto flat = flattenBookmarks();
                if (!flat.empty()) { bmPath = flat.back().path; bmScrollTo((int)flat.size() - 1); }
                scroll = (int)1e9;
            } else if (context == Context::Pinned) {
                pinSelection = store.pinned.empty() ? 0 : (int)store.pinned.size() - 1;
                scroll = (int)1e9;
            } else if (context == Context::Help) {
                helpScroll = (int)1e9;
            } else if (context == Context::Edit) {
                auto b = lineBounds(editText, (size_t)editRow);
                editCol = (int)(b.end - b.start);
            } else {
                selection = n ? (int)n - 1 : 0;
                scrollToSelection(true);
            }
            break;
        }

        case Action::CopyItem:
            doCopy();
            break;

        case Action::DeleteItem: {
            if (context == Context::Main || context == Context::Filter) {
                if (visibleCount() == 0) { setStatus("no items"); break; }
                size_t idx = resolveSelected();
                store.eraseClip(idx);
                rebuildFilter();
                clampSelection((int)visibleCount() - 1);
                store.save(config);
                setStatus("deleted");
            } else if (context == Context::Pinned) {
                if (pinSelection < (int)store.pinned.size()) {
                    store.pinned.erase(store.pinned.begin() + pinSelection);
                    if (pinSelection > (int)store.pinned.size() - 1) pinSelection = (int)store.pinned.size() - 1;
                    if (pinSelection < 0) pinSelection = 0;
                    store.save(config);
                    setStatus("unpinned");
                }
            }
            break;
        }

        case Action::EditItem: {
            if (context == Context::Main || context == Context::Filter) {
                if (visibleCount() == 0) { setStatus("no items"); break; }
                openEditorForClip(visibleClip((size_t)selection).text);
            }
            break;
        }

        case Action::TogglePin: {
            if (context != Context::Main && context != Context::Filter) break;
            if (visibleCount() == 0) { setStatus("no items"); break; }
            const Clip& clip = visibleClip((size_t)selection);
            for (size_t i = 0; i < store.pinned.size(); ++i) {
                if (store.pinned[i].text == clip.text) {
                    store.pinned.erase(store.pinned.begin() + (ptrdiff_t)i);
                    store.save(config);
                    setStatus("unpinned");
                    return;
                }
            }
            store.pinned.insert(store.pinned.begin(), clip);
            store.save(config);
            setStatus("pinned");
            break;
        }

        case Action::EnterFilter:
            setContext(Context::Filter);
            filterText.clear();
            rebuildFilter();
            setStatus("/filter: type to search, enter to copy");
            break;

        case Action::EnterCommand:
            setContext(Context::Command);
            commandText.clear();
            commandHistIndex = -1;
            break;

        case Action::OpenBookmarks:
            bmPath.clear();
            bmExpanded.clear();
            scroll = 0;
            {
                auto flat = flattenBookmarks();
                if (!flat.empty()) bmPath = flat.front().path;
            }
            setContext(Context::Bookmarks);
            break;

        case Action::OpenPinned:
            pinSelection = 0;
            scroll = 0;
            setContext(Context::Pinned);
            break;

        case Action::OpenHelp:
            helpScroll = 0;
            setContext(Context::Help);
            break;

        case Action::NewGroup: {
            if (context == Context::Main || context == Context::Filter) {
                if (visibleCount() == 0) { setStatus("no items"); break; }
                returnContext = Context::Main;
                promptKind = PromptKind::AddClipToGroup;
                promptLabel = " bookmarks > add clip to: ";
                promptInput = "/";
                promptCursor = 1;
                promptSeed = visibleClip((size_t)selection).text;
            } else if (context == Context::Bookmarks) {
                returnContext = Context::Bookmarks;
                promptKind = PromptKind::NewGroup;
                promptLabel = " bookmarks > new group: ";
                promptInput.clear();
                promptCursor = 0;
                promptSeed = ""; // parent path recorded at finish time from bmPath
            }
            setContext(Context::Prompt);
            break;
        }

        case Action::RenameSelected: {
            if (context != Context::Bookmarks) break;
            if (bmPath.empty()) { setStatus("nothing selected"); break; }
            if (nodeGroup(bmPath) == nullptr) { setStatus("only groups can be renamed"); break; }
            returnContext = Context::Bookmarks;
            promptKind = PromptKind::RenameNode;
            promptLabel = " bookmarks > rename to: ";
            promptInput = nodeNameAt(bmPath);
            promptCursor = promptInput.size();
            setContext(Context::Prompt);
            break;
        }

        case Action::DeleteSelected: {
            if (context != Context::Bookmarks) break;
            if (bmPath.empty()) { setStatus("nothing selected"); break; }
            auto before = flattenBookmarks();
            int idx = flatIndexOf(before, bmPath);
            if (idx < 0) break;
            if (store.deleteNode(bmPath)) {
                auto after = flattenBookmarks();
                if (after.empty()) {
                    bmPath.clear();
                } else {
                    int ni = std::min<int>((int)after.size() - 1, idx);
                    bmPath = after[(size_t)ni].path;
                    bmScrollTo(ni);
                }
                store.save(config);
                setStatus("deleted node");
            }
            break;
        }

        case Action::Back: {
            if (context == Context::Bookmarks) {
                setContext(Context::Main);
            } else {
                setContext(Context::Main);
            }
            break;
        }

        case Action::EnterChild: {
            if (context != Context::Bookmarks) break;
            if (bmPath.empty()) {
                auto flat = flattenBookmarks();
                if (flat.empty()) { setStatus("bookmarks empty"); break; }
                bmPath = flat.front().path;
                break;
            }
            if (nodeGroup(bmPath) != nullptr) {
                // "l"/enter: expand or collapse a group (vim file tree).
                auto it = bmExpanded.find(bmPath);
                if (it != bmExpanded.end()) bmExpanded.erase(it);
                else bmExpanded.insert(bmPath);
                auto flat = flattenBookmarks();
                bmScrollTo(flatIndexOf(flat, bmPath));
            } else if (nodeClip(bmPath) != nullptr) {
                doCopy();
            }
            break;
        }

        case Action::Parent: {
            if (context != Context::Bookmarks) break;
            // "h": collapse an expanded group, otherwise move up to its parent.
            if (bmPath.empty()) break;
            if (nodeGroup(bmPath) != nullptr && bmExpanded.count(bmPath)) {
                bmExpanded.erase(bmPath);
            } else {
                bmPath.pop_back();
            }
            auto flat = flattenBookmarks();
            bmScrollTo(flatIndexOf(flat, bmPath));
            break;
        }

        case Action::Confirm: {
            if (context == Context::Command) {
                std::string cmd = trim(commandText);
                if (!cmd.empty()) {
                    commandHistory.erase(
                        std::remove(commandHistory.begin(), commandHistory.end(), cmd),
                        commandHistory.end());
                    commandHistory.insert(commandHistory.begin(), cmd);
                    if (commandHistory.size() > 50) commandHistory.resize(50);
                }
                runCommand(cmd);
                setContext(Context::Main);
            } else if (context == Context::Prompt) {
                finishPrompt();
                setContext(returnContext);
            }
            break;
        }

        case Action::Cancel: {
            if (context == Context::Filter) {
                filterText.clear();
                rebuildFilter();
                setContext(Context::Main);
                setStatus("filter cancelled");
            } else if (context == Context::Command) {
                setContext(Context::Main);
            } else if (context == Context::Prompt) {
                setContext(returnContext);
            } else if (context == Context::Edit) {
                setContext(Context::Main);
            } else if (context == Context::Bookmarks) {
                // esc collapses an expanded group before quitting the tree.
                if (!bmPath.empty() && nodeGroup(bmPath) != nullptr && bmExpanded.count(bmPath)) {
                    bmExpanded.erase(bmPath);
                } else {
                    setContext(Context::Main);
                }
            }
            break;
        }

        case Action::MoveCharLeft: {
            if (context == Context::Edit) { moveCursor(0, -1); break; }
            if (context == Context::Prompt && promptCursor > 0) --promptCursor;
            break;
        }
        case Action::MoveCharRight: {
            if (context == Context::Edit) { moveCursor(0, +1); break; }
            if (context == Context::Prompt && promptCursor < promptInput.size()) ++promptCursor;
            break;
        }
        case Action::DeleteCharBack: {
            if (context == Context::Filter) {
                if (!filterText.empty()) { filterText.pop_back(); rebuildFilter(); clampSelection((int)visibleCount() - 1); }
            } else if (context == Context::Command) {
                if (!commandText.empty()) commandText.pop_back();
                commandHistIndex = -1;
            } else if (context == Context::Prompt) {
                if (promptCursor > 0 && !promptInput.empty()) {
                    promptInput.erase(promptInput.begin() + (ptrdiff_t)promptCursor - 1);
                    --promptCursor;
                }
            } else if (context == Context::Edit) {
                editBackspace();
            }
            break;
        }
        case Action::DeleteCharFwd: {
            if (context == Context::Edit) editForwardDelete();
            break;
        }

        case Action::SaveEditor: {
            if (context == Context::Edit) {
                if (editTarget < store.clips.size()) {
                    store.clips[editTarget].text = editText;
                    store.save(config);
                }
                setContext(Context::Main);
                setStatus("saved");
            }
            break;
        }

        case Action::CycleUp: {
            if (context == Context::Command && !commandHistory.empty()) {
                if (commandHistIndex < 0) commandHistIndex = (int)commandHistory.size() - 1;
                else if (commandHistIndex > 0) --commandHistIndex;
                commandText = commandHistory[(size_t)commandHistIndex];
            }
            break;
        }
        case Action::CycleDown: {
            if (context == Context::Command && commandHistIndex >= 0) {
                --commandHistIndex;
                commandText = commandHistIndex >= 0 ? commandHistory[(size_t)commandHistIndex] : "";
            }
            break;
        }

        case Action::None:
            break;
    }
    repeatPending = 0;
}

// ---------------------------------------------------------------------------
// List helpers
// ---------------------------------------------------------------------------

size_t App::visibleCount() const {
    return filterActive() ? filtered.size() : store.clips.size();
}

const Clip& App::visibleClip(size_t i) const {
    size_t idx = filterActive() ? filtered[i % filtered.size()] : i;
    return store.clips[idx];
}

size_t App::resolveSelected() const {
    if (visibleCount() == 0) return 0;
    return filterActive() ? filtered[(size_t)selection % filtered.size()]
                          : (size_t)selection;
}

void App::rebuildFilter() {
    if (filterActive()) {
        const std::string needle = toLower(filterText);
        filtered.clear();
        for (size_t i = 0; i < store.clips.size(); ++i) {
            if (toLower(store.clips[i].text).find(needle) != std::string::npos) {
                filtered.push_back(i);
            }
        }
    } else {
        filtered.clear();
    }
    clampSelection((int)visibleCount() - 1);
}

void App::clampSelection(int maxIndex) {
    if (selection > maxIndex) selection = std::max(0, maxIndex);
    if (selection < 0) selection = 0;
}

void App::scrollToSelection(bool force) {
    (void)force;
    // Coarse approach: keep selection inside the current scroll window.
    int visibleNow = std::max(1, std::min(window.height() / 18, 40));
    if (scroll > selection) scroll = selection;
    if (selection >= scroll + visibleNow) scroll = selection - visibleNow + 1;
    if (scroll < 0) scroll = 0;
}

std::vector<App::FlatNode> App::flattenBookmarks() const {
    std::vector<FlatNode> out;
    auto walk = [&](auto&& self, const std::vector<size_t>& path, int depth) -> void {
        const Group* g = store.groupAt(path);
        const std::vector<Group>& gs = g ? g->groups : store.groups;
        const std::vector<Clip>& cs = g ? g->clips : store.clips;
        for (size_t i = 0; i < gs.size(); ++i) {
            std::vector<size_t> p = path;
            p.push_back(i);
            const bool exp = bmExpanded.find(p) != bmExpanded.end();
            out.push_back({p, depth, true, exp});
            if (exp) self(self, p, depth + 1);
        }
        for (size_t i = 0; i < cs.size(); ++i) {
            std::vector<size_t> p = path;
            p.push_back(gs.size() + i);
            out.push_back({p, depth, false, false});
        }
    };
    walk(walk, {}, 0);
    return out;
}



void App::bmScrollTo(int flatIndex) {
    int visibleNow = std::max(1, std::min(window.height() / 18, 40));
    if (scroll > flatIndex) scroll = flatIndex;
    if (flatIndex >= scroll + visibleNow) scroll = flatIndex - visibleNow + 1;
    if (scroll < 0) scroll = 0;
}

const Group* App::nodeGroup(const std::vector<size_t>& full) const {
    if (full.empty()) return nullptr;
    std::vector<size_t> parent = full;
    size_t last = parent.back();
    parent.pop_back();
    const Group* g = store.groupAt(parent);
    const auto& gs = g ? g->groups : store.groups;
    return (last < gs.size()) ? &gs[last] : nullptr;
}

const Clip* App::nodeClip(const std::vector<size_t>& full) const {
    if (full.empty()) return nullptr;
    std::vector<size_t> parent = full;
    size_t last = parent.back();
    parent.pop_back();
    const Group* g = store.groupAt(parent);
    const auto& gs = g ? g->groups : store.groups;
    if (last < gs.size()) return nullptr;
    size_t ci = last - gs.size();
    const auto& cs = g ? g->clips : store.clips;
    return (ci < cs.size()) ? &cs[ci] : nullptr;
}

const Group* App::nodeAtPath(const std::vector<size_t>& path) const {
    // Any group in the path (including the final node if it is a group).
    const Group* g = store.groupAt(path);
    if (g) return g;
    // The final element may be a clip inside the parent group.
    return nodeGroup(path);
}

std::string App::nodeNameAt(const std::vector<size_t>& full) const {
    if (const Group* g = nodeGroup(full)) return g->name;
    if (const Clip* c = nodeClip(full)) return c->text;
    return {};
}

// ---------------------------------------------------------------------------
// Copy / editor / command / prompt
// ---------------------------------------------------------------------------

void App::doCopy() {
    size_t idx;
    if (context == Context::Pinned) {
        if (store.pinned.empty() || pinSelection >= (int)store.pinned.size()) { setStatus("no pinned items"); return; }
        idx = (size_t)pinSelection;
        clipboard::write(store.pinned[idx].text);
    } else {
        if (visibleCount() == 0) { setStatus("no items"); return; }
        idx = resolveSelected();
        clipboard::write(store.clips[idx].text);
    }
    setStatus("copied");
    hideNow();
}

void App::openEditorForClip(const std::string& text) {
    editTarget = resolveSelected();
    editText = text;
    editRow = (int)lineCount(text) - 1;
    editCol = (int)(lineBounds(text, (size_t)editRow).end - lineBounds(text, (size_t)editRow).start);
    setContext(Context::Edit);
}

void App::runCommand(const std::string& cmd) {
    if (cmd.empty()) { setStatus(":"); return; }
    std::string lower = toLower(cmd);
    if (lower == "quit" || lower == "q") {
        running = false;
        setStatus("quitting");
    } else if (lower == "clear" || lower == "cls") {
        store.clips.clear();
        rebuildFilter();
        store.save(config);
        setStatus("history cleared");
    } else if (lower.rfind("theme ", 0) == 0) {
        std::string name = trim(cmd.substr(6));
        Theme t = loadTheme(config.themesDir, name);
        // Check whether it resolved to the real theme (name matches) or default.
        bool ok = false;
        for (const auto& [n, _] : builtinThemes()) if (n == name) ok = true;
        if (!ok) ok = fileExists(config.themesDir + "/" + name + ".json");
        theme = t;
        if (ok) {
            config.theme = name;
            config.save();
            setStatus("theme: " + name);
        } else {
            theme = loadTheme(config.themesDir, config.theme);
            setStatus("unknown theme: " + name);
        }
    } else if (lower == "themes") {
        std::string list;
        for (const auto& [n, _] : builtinThemes()) {
            if (!list.empty()) list += ", ";
            list += n;
        }
        setStatus("themes: " + list);
    } else if (lower == "help") {
        setContext(Context::Help);
    } else if (lower == "pins" || lower == "pinned") {
        pinSelection = 0;
        setContext(Context::Pinned);
    } else {
        setStatus("unknown command: " + cmd);
    }
}

void App::finishPrompt() {
    const std::string input = trim(promptInput);
    switch (promptKind) {
        case PromptKind::NewGroup: {
            if (input.empty()) { setStatus("aborted: empty name"); break; }
            std::vector<size_t> parent = bmPath;
            // Create the group under the selected group, or under root when the
            // selection is a clip (or nothing is selected).
            if (!parent.empty() && nodeGroup(parent) == nullptr) parent.pop_back();
            if (store.addGroup(parent, input)) {
                store.save(config);
                setStatus("group created: " + input);
            } else {
                setStatus("could not create group (exists?)");
            }
            break;
        }
        case PromptKind::RenameNode: {
            if (store.renameNode(bmPath, input)) {
                store.save(config);
                setStatus("renamed");
            } else {
                setStatus("rename failed");
            }
            break;
        }
        case PromptKind::AddClipToGroup: {
            if (input.empty() || input == "/") { setStatus("aborted: empty path"); break; }
            Clip c;
            c.text = promptSeed;
            c.ts = store.clips.empty() ? "0" : store.clips.front().ts;
            if (store.addClipToPath(input, c)) {
                store.save(config);
                setStatus("added to: " + input);
            } else {
                setStatus("add failed");
            }
            break;
        }
        case PromptKind::None:
            break;
    }
    promptKind = PromptKind::None;
}

void App::setContext(Context c) {
    context = c;
}

void App::setStatus(std::string s) {
    status = std::move(s);
    pendingStatus = true;
}

void App::insertChar(char ch) {
    if (context == Context::Filter && filterText.size() < 256) {
        filterText += ch;
        rebuildFilter();
    } else if (context == Context::Command && commandText.size() < 512) {
        commandText += ch;
        commandHistIndex = -1;
    }
}

// --- editor text ops --------------------------------------------------------

void App::moveCursor(int dr, int dc) {
    int rows = (int)lineCount(editText);
    editRow = std::clamp(editRow + dr, 0, rows - 1);
    size_t len = lineBounds(editText, (size_t)editRow).end - lineBounds(editText, (size_t)editRow).start;
    if (dc != 0) {
        if (dc < 0) {
            if (editCol > 0) {
                editCol -= 1;
            } else if (editRow > 0) {
                editRow -= 1;
                size_t plen = lineBounds(editText, (size_t)editRow).end -
                              lineBounds(editText, (size_t)editRow).start;
                editCol = (long)plen;
            }
        } else {
            if (editCol < (long)len) {
                editCol += 1;
            } else if (editRow < rows - 1) {
                editRow += 1;
                editCol = 0;
            }
        }
    }
    editCol = (int)std::clamp<long>(editCol, 0, (long)len);
}

void App::editInsert(char ch) {
    size_t b = lineBounds(editText, (size_t)editRow).start;
    editText.insert(editText.begin() + (ptrdiff_t)(b + (size_t)std::clamp<long>(editCol, 0, (long)editText.size())), ch);
    editCol = (int)std::min<long>(editCol + 1, (long)lineBounds(editText, (size_t)editRow).end);
    (void)b;
}

void App::editInsertNewline() {
    size_t b = lineBounds(editText, (size_t)editRow).start;
    size_t col = (size_t)std::clamp<long>(editCol, 0, (long)(lineBounds(editText, (size_t)editRow).end - b));
    editText.insert(editText.begin() + (ptrdiff_t)(b + col), '\n');
    editRow += 1;
    editCol = 0;
}

void App::editBackspace() {
    if (editCol > 0) {
        size_t b = lineBounds(editText, (size_t)editRow).start;
        editText.erase(editText.begin() + (ptrdiff_t)(b + (size_t)editCol - 1));
        editCol -= 1;
    } else if (editRow > 0) {
        editRow -= 1;
        size_t plen = lineBounds(editText, (size_t)editRow).end - lineBounds(editText, (size_t)editRow).start;
        size_t nl = lineBounds(editText, (size_t)editRow).end;
        editText.erase(editText.begin() + (ptrdiff_t)nl); // remove the '\n'
        editCol = (long)plen;
    }
}

void App::editForwardDelete() {
    size_t b = lineBounds(editText, (size_t)editRow).start;
    size_t len = lineBounds(editText, (size_t)editRow).end - b;
    if (editCol < (long)len) {
        editText.erase(editText.begin() + (ptrdiff_t)(b + (size_t)editCol));
    } else if (editRow < (int)lineCount(editText) - 1) {
        editText.erase(editText.begin() + (ptrdiff_t)(b + len));
    }
}

std::string App::promptInputVisible() const {
    return promptInput;
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void App::render() {
    ImGui::PushFont(window.font);
    ui::Canvas c = ui::beginCanvas(theme, ImGui::GetIO());

    switch (context) {
        case Context::Main:
        case Context::Filter:
            drawMainView(*this, c);
            break;
        case Context::Bookmarks:
            drawBookmarksView(*this, c);
            break;
        case Context::Pinned:
            drawPinnedView(*this, c);
            break;
        case Context::Help:
            drawHelpView(*this, c);
            break;
        case Context::Edit:
            drawEditView(*this, c);
            break;
        case Context::Command:
            drawCommandView(*this, c);
            break;
        case Context::Prompt:
            drawPromptView(*this, c);
            break;
        case Context::Global:
            break;
    }

    ui::endCanvas();
    ImGui::PopFont();
}

} // namespace kadath