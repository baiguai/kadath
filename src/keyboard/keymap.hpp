#pragma once

#include <map>
#include <string>

#include "actions.hpp"
#include "keyboard/keys.hpp"

namespace kadath {

// Maps key chords to actions. "global" bindings (checked first) live in the
// Global context map so the same binding can be active in every context.
struct Keymap {
    std::map<KeyChord, Action> global;
    std::map<Context, std::map<KeyChord, Action>> contexts;

    // Add via canonical string form, e.g. add(Context::Main, "ctrl+s", Action::SaveEditor).
    bool add(Context ctx, const std::string& chordSpec, Action action);

    // active==false actions are written but never dispatched (legacy entries).
    std::map<KeyChord, Action>* mapFor(Context ctx);

    Action resolve(Context ctx, const KeyChord& chord) const;

    // The built-in defaults, always merged under user-defined bindings.
    static Keymap defaults();

    // Serialize the merged map as JSON to the given path.
    bool toJson(const std::string& path) const;
};

} // namespace kadath