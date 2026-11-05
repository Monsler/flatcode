#pragma once

#include "raylib.h"
#include <string>

namespace Renderer {

    // моноширинный шрифт редактора кода: JetBrains Mono (Assets/fonts/JetBrainsMono,
    // лицензия OFL — в той же папке). в отличие от sora умеет кириллицу,
    // поэтому код с русскими комментариями не превращается в "?"
    class EditorFont {
    public:
        static EditorFont& instance();

        EditorFont(const EditorFont&) = delete;
        EditorFont& operator=(const EditorFont&) = delete;
        ~EditorFont();

        void load();
        [[nodiscard]] bool loaded() const { return loaded_; }

        // ширина одного символа моно-шрифта при данном кегле
        // (jetbrains mono моноширинный, ширина одинакова для всех глифов)
        [[nodiscard]] float advance(float fontSize) const;

        // рисует строку верхним левым углом в (x, y)
        void draw(const std::string& text, float x, float y, float fontSize, Color color) const;

        [[nodiscard]] Vector2 measure(const std::string& text, float fontSize) const;

        [[nodiscard]] const Font& font() const { return font_; }

    private:
        EditorFont() = default;

        Font font_{};
        float baseAdvance_ = 0.0f; // ширина символа при baseSize атласа
        bool loaded_ = false;
        bool attempted_ = false;
    };
}
