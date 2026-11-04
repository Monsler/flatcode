#include "Display.hpp"
#include <raylib.h>

namespace Renderer {
    Display::Display() {
        SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
        InitWindow(800, 500, "FlatCode");
    }

    void Display::setWindowDim(const int width, const int height) {
        SetWindowSize(width, height);
    }

    void Display::setWindowTitle(const char* title) {
        SetWindowTitle(title);
    }

    // основной цикл дисплея
    void Display::mainLoop() {
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            EndDrawing();
        }
    }
}