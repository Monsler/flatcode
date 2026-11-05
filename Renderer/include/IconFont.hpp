#pragma once

#include "IconsLucide.h"
#include "raylib.h"

namespace Renderer {

    // менеджер иконочного шрифта lucide (Assets/lucide.ttf)
    // грузит все codepoints из IconsLucide.h один раз, дальше — draw по имени глифа
    class IconFont {
    public:
        static IconFont& instance();

        IconFont(const IconFont&) = delete;
        IconFont& operator=(const IconFont&) = delete;
        ~IconFont();

        void load();
        [[nodiscard]] bool loaded() const { return loaded_; }

        // рисует иконку (например ICON_LC_FILES) верхним левым углом в (x, y)
        void draw(const char* icon, const float x, const float y, const float size, const Color color) const;

        // рисует иконку, отцентрованную в квадрате (x, y, box, box)
        void drawCentered(const char* icon, const float x, const float y, const float box, const float size, const Color color) const;

        // рисует иконку, отцентрованную в прямоугольнике (x, y, boxW, boxH)
        void drawCentered(const char* icon, const float x, const float y, const float boxW, const float boxH,
                          const float size, const Color color) const;

        [[nodiscard]] Vector2 measure(const char* icon, const float size) const;

    private:
        IconFont() = default;

        Font font_{};
        bool loaded_ = false;
        bool attempted_ = false;
    };
}
