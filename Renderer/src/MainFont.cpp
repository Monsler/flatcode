#include "MainFont.hpp"
#include "Logger.hpp"

#include <vector>

namespace Renderer {

    MainFont& MainFont::instance() {
        static MainFont font;
        return font;
    }

    MainFont::~MainFont() {
        if (this->loaded_) {
            UnloadFont(this->regular_);
            UnloadFont(this->medium_);
            UnloadFont(this->bold_);
        }
    }

    Font MainFont::loadFace(const char* path) const {
        // sora не содержит кириллицу, поэтому берём ascii + latin-1: этого хватает на весь ui
        std::vector<int> codepoints;
        codepoints.reserve(256 - 32);
        for (int cp = 32; cp < 256; ++cp) {
            codepoints.push_back(cp);
        }

        // 48 с запасом: у sora крупные выносные элементы, при 32 raylib режет глифы
        Font font = LoadFontEx(path, 48, codepoints.data(), static_cast<int>(codepoints.size()));
        if (font.texture.id != 0) {
            // мипмапы + трилинейка: текст рендерится сильно меньше атласа,
            // без этого по краям глифов остаётся лесенка и мерцание при скролле
            GenTextureMipmaps(&font.texture);
            SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
        }
        return font;
    }

    void MainFont::load() {
        if (this->attempted_) {
            return;
        }
        this->attempted_ = true;

        // ttf лежат рядом с исполняемым файлом (копируются cmake-ом вместе с Assets)
        this->regular_ = this->loadFace("Assets/fonts/Sora/Sora-Regular.ttf");
        this->medium_ = this->loadFace("Assets/fonts/Sora/Sora-Medium.ttf");
        this->bold_ = this->loadFace("Assets/fonts/Sora/Sora-Bold.ttf");

        if (this->regular_.texture.id == 0) {
            Base::Logger::instance().log(Base::LogLevel::ERR, "MainFont: failed to load Sora-Regular.ttf");
            return;
        }
        if (this->medium_.texture.id == 0) {
            // отдельный атлас, а не копия структуры: иначе texture выгрузится дважды
            this->medium_ = this->loadFace("Assets/fonts/Sora/Sora-Regular.ttf");
        }
        if (this->bold_.texture.id == 0) {
            this->bold_ = this->loadFace("Assets/fonts/Sora/Sora-Regular.ttf");
        }

        this->loaded_ = true;
        Base::Logger::instance().log(Base::LogLevel::INFO, "MainFont: Sora loaded");
    }

    const Font& MainFont::font(const Weight weight) const {
        switch (weight) {
            case Weight::Medium: return this->medium_;
            case Weight::Bold: return this->bold_;
            case Weight::Regular: break;
        }
        return this->regular_;
    }

    Vector2 MainFont::measure(const std::string& text, const float fontSize, const Weight weight) const {
        if (!this->loaded_) {
            return {0, 0};
        }
        return MeasureTextEx(this->font(weight), text.c_str(), fontSize, 0.0f);
    }

    void MainFont::draw(const std::string& text, const float x, const float y, const float fontSize,
                        const Color color, const Weight weight) const {
        if (!this->loaded_) {
            return;
        }
        DrawTextEx(this->font(weight), text.c_str(), {x, y}, fontSize, 0.0f, color);
    }

    void MainFont::drawCenteredX(const std::string& text, const float x, const float width, const float y,
                                 const float fontSize, const Color color, const Weight weight) const {
        const Vector2 m = this->measure(text, fontSize, weight);
        this->draw(text, x + (width - m.x) * 0.5f, y, fontSize, color, weight);
    }

    void MainFont::drawEllipsized(const std::string& text, const float x, const float y, const float maxWidth,
                                  const float fontSize, const Color color, const Weight weight) const {
        if (this->measure(text, fontSize, weight).x <= maxWidth) {
            this->draw(text, x, y, fontSize, color, weight);
            return;
        }
        std::string clipped = text;
        const Vector2 dots = this->measure("...", fontSize, weight);
        while (!clipped.empty() && this->measure(clipped, fontSize, weight).x + dots.x > maxWidth) {
            clipped.pop_back();
        }
        this->draw(clipped + "...", x, y, fontSize, color, weight);
    }
}
