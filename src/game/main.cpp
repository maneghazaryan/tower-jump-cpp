#include <cstdio>
#include <string>

#include "app.hpp"
#include "raylib.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace {

tj::App g_app;

void frame() { g_app.frame(); }

std::string assetRoot() {
#if defined(__EMSCRIPTEN__)
    return "assets/";  // packaged into the .data file with --preload-file
#else
    // Look next to the executable first, so the game runs from any working directory.
    const std::string exeDir = GetApplicationDirectory();
    if (FileExists((exeDir + "assets/config/gameplay.json").c_str())) return exeDir + "assets/";
    return "assets/";
#endif
}

}  // namespace

int main() {
    if (!g_app.init(assetRoot())) {
        for (const std::string& e : g_app.errors()) std::fprintf(stderr, "%s\n", e.c_str());
        // Keep the window up so the errors are visible on screen too.
    }
#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop(frame, 0, 1);
#else
    while (!g_app.shouldClose()) frame();
    CloseWindow();
#endif
    return 0;
}
