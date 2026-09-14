#include "app.hpp"

int main() {
    kadath::App app;
    if (!app.init()) return 1;
    app.run();
    app.shutdown();
    return 0;
}