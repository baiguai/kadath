#include "actions.hpp"
#include "keyboard/keys.hpp"
#include "keyboard/keymap.hpp"
#include "storage/config.hpp"
#include "storage/store.hpp"

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

using namespace kadath;

namespace {
int g_failures = 0     ;;
void check(bool ok) { if (!ok) { ++g_failures; std::printf("check failed (line %d)\n", __LINE__); } }
void checkEq(Action got, Action want) {
    if (got != want) { ++g_failures; std::printf("action mismatch: %s vs %s\n", actionName(got), actionName(want)); }
}
void checkStrEq(const std::string& got, const std::string& want) {
    if (got != want) { ++g_failures; std::printf("string mismatch: %s vs %s\n", got.c_str(), want.c_str()); }
}
} 

static int testChordFromString() {
    auto esc = chordFromString("esc");
    check(esc.has_value());
    if (esc) { check(esc->mods == Mod_None); check(esc->base == NK_Escape); }
    auto j = chordFromString("j");
    check(j.has_value());
    if (j) { check(j->mods == Mod_None); check(j->base == 'j'); }
    auto cx = chordFromString("ctrl+x");
    check(cx.has_value());
    if (cx) { check(cx->mods == Mod_Ctrl); check(cx->base == 'x'); }
    check(!chordFromString("").has_value());
    return 0;
}

static int testChordToString() {
    checkStrEq(chordToString(chord(Mod_None, NK_Escape)), "esc");
    checkStrEq(chordToString(chord(Mod_None, 'j')), "j");
    checkStrEq(chordToString(chord(Mod_Ctrl, 'x')), "ctrl+x");
    return 0;
}

static int testKeymapDefaults() {
    Keymap k = Keymap::defaults();
    checkEq(k.resolve(Context::Main, chord(Mod_Ctrl, 'q')), Action::Quit);
    checkEq(k.resolve(Context::Main, chord(Mod_None, 'j')), Action::MoveDown);
    checkEq(k.resolve(Context::Main, chord(Mod_None, 'k')), Action::MoveUp);
    checkEq(k.resolve(Context::Main, chord(Mod_None, NK_Enter)), Action::CopyItem);
    return 0;
}

static int testKeymapOverride() {
    Keymap k = Keymap::defaults();
    Keymap::defaults();
    k.add(Context::Main, "ctrl+t", Action::TogglePin);
    checkEq(k.resolve(Context::Main, chord(Mod_Ctrl, 't')), Action::TogglePin);
    checkEq(k.resolve(Context::Main, chord(Mod_None, 'j')), Action::MoveDown);
    return 0;
}

static int testActionNames() {
    checkStrEq(actionName(Action::Quit), "quit");
    checkStrEq(actionName(Action::MoveDown), "move_down");
    checkStrEq(actionName(Action::CopyItem), "copy");
    checkStrEq(actionName(Action::TogglePin), "toggle_pin");
    return 0;
}

static int testConfigDefaults() {
    AppConfig c;
    c.load();
    check(c.maxClips == 250);
    check(c.theme == "default");
    check(c.fontPx == 16.0f);
    return 0;
}

static int testStore() {
    Store s;
    s.sinkClip("alpha");
    if (!s.clips.empty()) check(s.clips[0].text == "alpha");
    s.sinkClip("beta");
    if (!s.clips.empty()) check(s.clips[0].text == "beta");
    return 0;
}

int main() {
    testChordFromString();
    testChordToString();
    testKeymapDefaults();
    testKeymapOverride();
    testActionNames();
    testConfigDefaults();
    testStore();
    if (g_failures == 0) { std::printf("all tests passed\n"); return 0; }
    std::printf("%d failures\n", g_failures);
    return 1;
}
