#include "components/Rectangle.hpp"
#include "raylib.h"
#include <algorithm>

namespace Renderer {
    FRectangle::FRectangle(const float x, const float y, const float width, const float height) {
        this->setPosition(x, y);
        this->setSize(width, height);
    }

    void FRectangle::setSize(const float width, const float height) {
        this->width_ = width;
        this->height_ = height;
        this->updateRoundness();
    }

    void FRectangle::setColor(const Color color) {
        this->color_ = color;
    }

    void FRectangle::setCornerRadius(const float rad) {
        this->cornerRadius = rad;
        this->updateRoundness();
    }

    void FRectangle::updateRoundness() {
        const float minSide = std::min(this->width_, this->height_);
        this->roundness = (minSide > 0.0f) ? std::clamp(2.0f * this->cornerRadius / minSide, 0.0f, 1.0f) : 0.0f;
    }

    bool FRectangle::hitTest(const float mx, const float my) const {
        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);
        return mx >= ax && mx <= ax + this->width_ && my >= ay && my <= ay + this->height_;
    }

    void FRectangle::onDraw() {
        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);
        DrawRectangleRounded({ax, ay, this->width_, this->height_}, this->roundness, 32, this->color_);
    }
}
