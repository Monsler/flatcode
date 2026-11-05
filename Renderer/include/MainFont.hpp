#pragma once

#include "raylib.h"
#include <string>

namespace Renderer {

    // основной ui-шрифт программы: семейство Sora (Assets/fonts/Sora)
    // грузит несколько начертаний один раз, дальше — draw текста любым весом
    // sora содержит только латиницу, поэтому глифы грузим для ascii + latin-1
    class MainFont {
    public:
        enum class Weight {
            Regular,
            Medium,
            Bold,
        };

        static MainFont& instance();

        MainFont(const MainFont&) = delete;
        MainFont& operator=(const MainFont&) = delete;
        ~MainFont();

        void load();
        [[nodiscard]] bool loaded() const { return loaded_; }

        // рисует строку верхним левым углом в (x, y); fontSize — высота в пикселях
        void draw(const std::string& text, const float x, const float y, const float fontSize,
                  const Color color, const Weight weight = Weight::Regular) const;

        // центрирует строку по горизонтали в полосе [x, x + width]
        void drawCenteredX(const std::string& text, const float x, const float width, const float y,
                           const float fontSize, const Color color, const Weight weight = Weight::Regular) const;

        // обрезает строку с "..." если она шире maxWidth
        void drawEllipsized(const std::string& text, const float x, const float y, const float maxWidth,
                            const float fontSize, const Color color, const Weight weight = Weight::Regular) const;

        [[nodiscard]] Vector2 measure(const std::string& text, const float fontSize,
                                      const Weight weight = Weight::Regular) const;

        [[nodiscard]] const Font& font(const Weight weight) const;

    private:
        MainFont() = default;

        Font loadFace(const char* path) const;

        Font regular_{};
        Font medium_{};
        Font bold_{};
        bool loaded_ = false;
        bool attempted_ = false;
    };
}
