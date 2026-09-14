#include "keyboard/keys.hpp"
#include "keyboard/keymap.hpp"
#include <cstdio>
using namespace kadath;
int main() {
    Keymap k = Keymap::defaults();
    auto q  = chord(Mod_Ctrl, 'q');
    auto j  = chord(Mod_None, 'j');
    auto ent= chord(Mod_None, NK_Enter);
    std::printf("ctrl+q -> %s\n", actionName(k.resolve(Context::Main, q)));
    std::printf("j      -> %s\n", actionName(k.resolve(Context::Main, j)));
    std::printf("enter  -> %s\n", actionName(k.resolve(Context::Main, ent)));
    return 0;
}
