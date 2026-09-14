#include "keyboard/keymap.hpp"

#include "util.hpp"

#include <nlohmann/json.hpp>

namespace kadath {

using json = nlohmann::json;

bool Keymap::add(Context ctx, const std::string& chordSpec, Action action) {
    auto chord = chordFromString(chordSpec);
    if (!chord || chord->empty()) return false;
    auto& m = (ctx == Context::Global) ? global : contexts[ctx];
    m[*chord] = action;
    return true;
}

std::map<KeyChord, Action>* Keymap::mapFor(Context ctx) {
    return (ctx == Context::Global) ? &global : &contexts[ctx];
}

Action Keymap::resolve(Context ctx, const KeyChord& chord) const {
    if (auto it = global.find(chord); it != global.end()) return it->second;
    if (auto cit = contexts.find(ctx); cit != contexts.end()) {
        if (auto it = cit->second.find(chord); it != cit->second.end()) return it->second;
    }
    return Action::None;
}

Keymap Keymap::defaults() {
    Keymap k;
    auto a = [&](Context ctx, const char* spec, Action action) { k.add(ctx, spec, action); };

    // global (any context)
    a(Context::Global, "ctrl+q", Action::Quit);
    a(Context::Global, "shift+q", Action::Quit);

    // main clip list
    a(Context::Main, "j", Action::MoveDown);
    a(Context::Main, "down", Action::MoveDown);
    a(Context::Main, "k", Action::MoveUp);
    a(Context::Main, "up", Action::MoveUp);
    a(Context::Main, "pageup", Action::MoveUpPage);
    a(Context::Main, "pagedown", Action::MoveDownPage);
    a(Context::Main, "ctrl+u", Action::MoveUpPage);
    a(Context::Main, "ctrl+d", Action::MoveDownPage);
    a(Context::Main, "g", Action::JumpTop);
    a(Context::Main, "G", Action::JumpBottom);
    a(Context::Main, "home", Action::JumpTop);
    a(Context::Main, "end", Action::JumpBottom);
    a(Context::Main, "enter", Action::CopyItem);
    a(Context::Main, "esc", Action::HideWindow);
    a(Context::Main, "d", Action::DeleteItem);
    a(Context::Main, "i", Action::EditItem);
    a(Context::Main, "p", Action::TogglePin);
    a(Context::Main, "/", Action::EnterFilter);
    a(Context::Main, ":", Action::EnterCommand);
    a(Context::Main, "`", Action::OpenBookmarks);
    a(Context::Main, "'", Action::OpenPinned);
    a(Context::Main, "?", Action::OpenHelp);

    // filter
    a(Context::Filter, "enter", Action::CopyItem);
    a(Context::Filter, "esc", Action::Cancel);
    a(Context::Filter, "j", Action::MoveDown);
    a(Context::Filter, "down", Action::MoveDown);
    a(Context::Filter, "k", Action::MoveUp);
    a(Context::Filter, "up", Action::MoveUp);

    // command
    a(Context::Command, "enter", Action::Confirm);
    a(Context::Command, "esc", Action::Cancel);
    a(Context::Command, "up", Action::CycleUp);
    a(Context::Command, "down", Action::CycleDown);

    // bookmarks tree
    a(Context::Bookmarks, "j", Action::MoveDown);
    a(Context::Bookmarks, "down", Action::MoveDown);
    a(Context::Bookmarks, "k", Action::MoveUp);
    a(Context::Bookmarks, "up", Action::MoveUp);
    a(Context::Bookmarks, "l", Action::EnterChild);
    a(Context::Bookmarks, "right", Action::EnterChild);
    a(Context::Bookmarks, "enter", Action::EnterChild);
    a(Context::Bookmarks, "h", Action::Parent);
    a(Context::Bookmarks, "left", Action::Parent);
    a(Context::Bookmarks, "backspace", Action::Parent);
    a(Context::Bookmarks, "esc", Action::Back);
    a(Context::Bookmarks, "M", Action::NewGroup);
    a(Context::Bookmarks, "R", Action::RenameSelected);
    a(Context::Bookmarks, "D", Action::DeleteSelected);
    a(Context::Bookmarks, "g", Action::JumpTop);
    a(Context::Bookmarks, "G", Action::JumpBottom);

    // pinned
    a(Context::Pinned, "j", Action::MoveDown);
    a(Context::Pinned, "down", Action::MoveDown);
    a(Context::Pinned, "k", Action::MoveUp);
    a(Context::Pinned, "up", Action::MoveUp);
    a(Context::Pinned, "enter", Action::CopyItem);
    a(Context::Pinned, "d", Action::DeleteItem);
    a(Context::Pinned, "esc", Action::Back);

    // help
    a(Context::Help, "j", Action::MoveDown);
    a(Context::Help, "down", Action::MoveDown);
    a(Context::Help, "k", Action::MoveUp);
    a(Context::Help, "up", Action::MoveUp);
    a(Context::Help, "pageup", Action::MoveUpPage);
    a(Context::Help, "pagedown", Action::MoveDownPage);
    a(Context::Help, "g", Action::JumpTop);
    a(Context::Help, "G", Action::JumpBottom);
    a(Context::Help, "esc", Action::Back);
    a(Context::Help, "q", Action::Back);

    // edit (enter/characters handled directly by the editor)
    a(Context::Edit, "esc", Action::Cancel);
    a(Context::Edit, "ctrl+s", Action::SaveEditor);
    a(Context::Edit, "backspace", Action::DeleteCharBack);
    a(Context::Edit, "delete", Action::DeleteCharFwd);
    a(Context::Edit, "left", Action::MoveCharLeft);
    a(Context::Edit, "right", Action::MoveCharRight);
    a(Context::Edit, "up", Action::MoveUp);
    a(Context::Edit, "down", Action::MoveDown);
    a(Context::Edit, "home", Action::JumpTop);
    a(Context::Edit, "end", Action::JumpBottom);

    // prompt
    a(Context::Prompt, "enter", Action::Confirm);
    a(Context::Prompt, "esc", Action::Cancel);
    a(Context::Prompt, "backspace", Action::DeleteCharBack);
    a(Context::Prompt, "delete", Action::DeleteCharFwd);
    a(Context::Prompt, "left", Action::MoveCharLeft);
    a(Context::Prompt, "right", Action::MoveCharRight);

    return k;
}

bool Keymap::toJson(const std::string& path) const {
    json out = json::object();
    json g = json::object();
    for (const auto& [chord, action] : global) {
        g[chordToString(chord)] = actionName(action);
    }
    out["global"] = g;
    for (const auto& [ctx, map] : contexts) {
        if (ctx == Context::Global) continue;
        json cm = json::object();
        for (const auto& [chord, action] : map) {
            cm[chordToString(chord)] = actionName(action);
        }
        out["contexts"][contextName(ctx)] = cm;
    }
    return writeFile(path, out.dump(2));
}

} // namespace kadath