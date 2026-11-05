#include "IconFont.hpp"
#include "Logger.hpp"

#include <vector>

namespace Renderer {
    IconFont& IconFont::instance() {
        static IconFont font;
        return font;
    }

    IconFont::~IconFont() {
        if (this->loaded_) {
            UnloadFont(this->font_);
        }
    }

    void IconFont::load() {
        if (this->attempted_) {
            return;
        }
        this->attempted_ = true;

        // глифы иконочного шрифта лежат в private use area,
        // поэтому явно передаём диапазон codepoints из IconsLucide.h
        std::vector<int> codepoints;
        codepoints.reserve(ICON_MAX_LC - ICON_MIN_LC + 1);
        for (int cp = ICON_MIN_LC; cp <= ICON_MAX_LC; ++cp) {
            codepoints.push_back(cp);
        }

        // ttf лежит рядом с исполняемым файлом (копируется cmake-ом)
        this->font_ = LoadFontEx("Assets/lucide.ttf", 64, codepoints.data(), static_cast<int>(codepoints.size()));

        if (this->font_.texture.id == 0) {
            Base::Logger::instance().log(Base::LogLevel::ERR, "IconFont: failed to load Assets/lucide.ttf");
            return;
        }

        // мипмапы + трилинейка: иконки рисуются меньше атласа 64px,
        // так что без мипмапов по краям лесенка
        GenTextureMipmaps(&this->font_.texture);
        SetTextureFilter(this->font_.texture, TEXTURE_FILTER_TRILINEAR);
        this->loaded_ = true;
        Base::Logger::instance().log(Base::LogLevel::INFO, "IconFont: lucide.ttf loaded");
    }

    Vector2 IconFont::measure(const char* icon, const float size) const {
        if (!this->loaded_) {
            return {0, 0};
        }
        return MeasureTextEx(this->font_, icon, size, 0.0f);
    }

    void IconFont::draw(const char* icon, const float x, const float y, const float size, const Color color) const {
        if (!this->loaded_) {
            return;
        }
        DrawTextEx(this->font_, icon, {x, y}, size, 0.0f, color);
    }

    void IconFont::drawCentered(const char* icon, const float x, const float y, const float box, const float size, const Color color) const {
        this->drawCentered(icon, x, y, box, box, size, color);
    }

    void IconFont::drawCentered(const char* icon, const float x, const float y, const float boxW, const float boxH,
                                const float size, const Color color) const {
        if (!this->loaded_) {
            return;
        }
        // оптическое центрирование по фактическим границам глифа:
        // measure() даёт высоту строки (=fontSize), а ink-бокс иконки определяют
        // offsetY и высота глифа в атласе — у иконок lucide они различаются,
        // из-за чего простое центрирование съезжает по вертикали
        int bytes = 0;
        const int cp = GetCodepointNext(icon, &bytes);
        const int index = GetGlyphIndex(this->font_, cp);
        const float scale = size / static_cast<float>(this->font_.baseSize);
        const float inkW = static_cast<float>(this->font_.recs[index].width) * scale;
        const float inkH = static_cast<float>(this->font_.recs[index].height) * scale;
        const float offX = static_cast<float>(this->font_.glyphs[index].offsetX) * scale;
        const float offY = static_cast<float>(this->font_.glyphs[index].offsetY) * scale;
        const float px = x + (boxW - inkW) * 0.5f - offX;
        const float py = y + (boxH - inkH) * 0.5f - offY;
        this->draw(icon, px, py, size, color);
    }
}
