#pragma once
#include "Component.hpp"
#include "raylib.h"

namespace Renderer {
    class FRectangle : public Component {
    public:
        FRectangle(const float x, const float y, const float width, const float height);

        void setSize(const float width, const float height);
        void setColor(const Color color);
        void setCornerRadius(const float rad);

        [[nodiscard]] float width() const { return width_; }
        [[nodiscard]] float height() const { return height_; }
        [[nodiscard]] Color color() const { return color_; }

        bool hitTest(float mx, float my) const override;

    protected:
        void onDraw() override;

    private:
        float width_ = 0.0f;
        float height_ = 0.0f;
        float cornerRadius = 0.0f;
        float roundness = 0.0f;
        Color color_ = WHITE;

        void updateRoundness();
    };
}
