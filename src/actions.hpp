#pragma once

#include <optional>
#include <string>

namespace kadath {

// Interaction contexts. The keymap dispatches per context; "global" bindings
// (e.g. quit) are checked first regardless of the active context.
enum class Context {
    Main,      // clip history list
    Filter,    // typing a search filter over the clip list
    Command,   // ":" command line
    Bookmarks, // nested bookmark group tree
    Pinned,    // pinned clips
    Help,      // help browser
    Edit,      // in-place clip editor
    Prompt,    // single-line input (new group / rename / add-to-group)
    Global,    // bindings active in every context (resolved first)
};

const char* contextName(Context c);
std::optional<Context> contextFromString(const std::string& s);

// Actions. One enum shared across contexts; each context reacts to the subset
// that is meaningful to it.
enum class Action {
    None = 0,

    // app / window
    Quit,
    HideWindow,

    // selection movement
    MoveDown,
    MoveUp,
    MoveDownPage,
    MoveUpPage,
    JumpTop,
    JumpBottom,

    // clip / item operations (apply to the selected item)
    CopyItem,     // copy to clipboard (and hide, per old-app behaviour)
    DeleteItem,
    EditItem,
    TogglePin,

    // mode entry
    EnterFilter,
    EnterCommand,
    OpenBookmarks,
    OpenPinned,
    OpenHelp,

    // navigation within modes
    Back,         // leave the current context (up a level / cancel)
    EnterChild,   // bookmarks: descend into a group
    Parent,       // bookmarks: ascend one level

    // typing / editing
    Confirm,      // execute / accept the current input
    Cancel,       // abort the current input
    MoveCharLeft,
    MoveCharRight,
    MoveWordLeft,
    MoveWordRight,
    DeleteCharBack,
    DeleteCharFwd,
    DeleteWordBack,
    SaveEditor,
    CycleUp,      // command history: previous
    CycleDown,    // command history: next

    // bookmark tree structure
    NewGroup,
    RenameSelected,
    DeleteSelected,
};

const char* actionName(Action a);
std::optional<Action> actionFromString(const std::string& s);

} // namespace kadath