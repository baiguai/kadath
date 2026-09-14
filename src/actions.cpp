#include "actions.hpp"

#include <cassert>

namespace kadath {

const char* contextName(Context c) {
    switch (c) {
        case Context::Main:      return "main";
        case Context::Filter:    return "filter";
        case Context::Command:   return "command";
        case Context::Bookmarks: return "bookmarks";
        case Context::Pinned:    return "pinned";
        case Context::Help:      return "help";
        case Context::Edit:      return "edit";
        case Context::Prompt:    return "prompt";
        case Context::Global:    return "global";
    }
    return "main";
}

std::optional<Context> contextFromString(const std::string& s) {
    if (s == "main") return Context::Main;
    if (s == "filter") return Context::Filter;
    if (s == "command") return Context::Command;
    if (s == "bookmarks") return Context::Bookmarks;
    if (s == "pinned") return Context::Pinned;
    if (s == "help") return Context::Help;
    if (s == "edit") return Context::Edit;
    if (s == "prompt") return Context::Prompt;
    if (s == "global") return Context::Global;
    return std::nullopt;
}

const char* actionName(Action a) {
    switch (a) {
        case Action::None:             return "none";
        case Action::Quit:             return "quit";
        case Action::HideWindow:       return "hide";
        case Action::MoveDown:         return "move_down";
        case Action::MoveUp:           return "move_up";
        case Action::MoveDownPage:     return "move_down_page";
        case Action::MoveUpPage:       return "move_up_page";
        case Action::JumpTop:          return "jump_top";
        case Action::JumpBottom:       return "jump_bottom";
        case Action::CopyItem:         return "copy";
        case Action::DeleteItem:       return "delete";
        case Action::EditItem:         return "edit";
        case Action::TogglePin:        return "toggle_pin";
        case Action::EnterFilter:      return "filter";
        case Action::EnterCommand:     return "command";
        case Action::OpenBookmarks:    return "bookmarks";
        case Action::OpenPinned:       return "pinned";
        case Action::OpenHelp:         return "help";
        case Action::Back:             return "back";
        case Action::EnterChild:       return "enter_child";
        case Action::Parent:           return "parent";
        case Action::Confirm:          return "confirm";
        case Action::Cancel:           return "cancel";
        case Action::MoveCharLeft:     return "move_char_left";
        case Action::MoveCharRight:    return "move_char_right";
        case Action::MoveWordLeft:     return "move_word_left";
        case Action::MoveWordRight:    return "move_word_right";
        case Action::DeleteCharBack:   return "delete_char_back";
        case Action::DeleteCharFwd:    return "delete_char_fwd";
        case Action::DeleteWordBack:   return "delete_word_back";
        case Action::SaveEditor:       return "save_editor";
        case Action::CycleUp:          return "history_up";
        case Action::CycleDown:        return "history_down";
        case Action::NewGroup:         return "new_group";
        case Action::RenameSelected:   return "rename";
        case Action::DeleteSelected:   return "delete_selected";
    }
    return "none";
}

std::optional<Action> actionFromString(const std::string& s) {
    for (int i = (int)Action::None + 1; i <= (int)Action::DeleteSelected; ++i) {
        Action a = (Action)i;
        if (actionName(a) == s) return a;
    }
    return std::nullopt;
}

} // namespace kadath