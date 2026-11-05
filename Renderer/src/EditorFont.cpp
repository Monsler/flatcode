#include "EditorFont.hpp"
#include "Logger.hpp"

#include <vector>

namespace Renderer {
    EditorFont& EditorFont::instance() {
        static EditorFont font;
        return font;
    }

    EditorFont::~EditorFont() {
        if (this->loaded_) {
            UnloadFont(this->font_);
        }
    }

    void EditorFont::load() {
        if (this->attempted_) {
            return;
        }
        this->attempted_ = true;

        // ascii + latin-1 + кириллица + типографские тире/кавычки/многоточие +
        // знак номера: всё, что реально встречается в коде и комментариях
        std::vector<int> codepoints;
        auto addRange = [&codepoints](const int from, const int to) {
            for (int cp = from; cp <= to; ++cp) {
                codepoints.push_back(cp);
            }
        };
        addRange(32, 255);
        addRange(0x0400, 0x04FF);
        addRange(0x2010, 0x2026);
        codepoints.push_back(0x2116);

        // ttf лежит рядом с исполняемым файлом (копируется cmake-ом)
        this->font_ = LoadFontEx("Assets/fonts/JetBrainsMono/JetBrainsMono-Regular.ttf", 48,
                                 codepoints.data(), static_cast<int>(codepoints.size()));
        if (this->font_.texture.id == 0) {
            Base::Logger::instance().log(Base::LogLevel::ERR, "EditorFont: failed to load JetBrainsMono-Regular.ttf");
            return;
        }

        // мипмапы + трилинейка: код рисуется мельче атласа 48px
        GenTextureMipmaps(&this->font_.texture);
        SetTextureFilter(this->font_.texture, TEXTURE_FILTER_TRILINEAR);

        const int m = GetGlyphIndex(this->font_, 'M');
        this->baseAdvance_ = static_cast<float>(this->font_.glyphs[m].advanceX);
        if (this->baseAdvance_ <= 0.0f) {
            // страховой путь, если advance не проставлен
            this->baseAdvance_ = MeasureTextEx(this->font_, "M", static_cast<float>(this->font_.baseSize), 0.0f).x;
        }

        this->loaded_ = true;
        Base::Logger::instance().log(Base::LogLevel::INFO, "EditorFont: JetBrains Mono loaded");
    }

    float EditorFont::advance(const float fontSize) const {
        if (!this->loaded_) {
            return fontSize * 0.6f; // приемлемая оценка до загрузки шрифта
        }
        return this->baseAdvance_ * (fontSize / static_cast<float>(this->font_.baseSize));
    }

    void EditorFont::draw(const std::string& text, const float x, const float y,
                          const float fontSize, const Color color) const {
        if (!this->loaded_) {
            return;
        }
        DrawTextEx(this->font_, text.c_str(), {x, y}, fontSize, 0.0f, color);
    }

    Vector2 EditorFont::measure(const std::string& text, const float fontSize) const {
        if (!this->loaded_) {
            return {static_cast<float>(text.size()) * this->advance(fontSize), fontSize};
        }
        return MeasureTextEx(this->font_, text.c_str(), fontSize, 0.0f);
    }
}
