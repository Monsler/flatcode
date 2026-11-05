#include "components/IconButton.hpp"
#include "IconFont.hpp"
#include "Theme.hpp"
#include "raylib.h"

namespace Renderer {

    FIconButton::FIconButton(const char* icon, const float size) : icon_(icon), size_(size) {
        this->iconSize_ = size * 0.5f;
    }

    bool FIconButton::hitTest(const float mx, const float my) const {
        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);
        return mx >= ax && mx <= ax + this->size_ && my >= ay && my <= ay + this->size_;
    }

    void FIconButton::onUpdate(const float) {
        const Vector2 mouse = GetMousePosition();
        this->hovered_ = this->hitTest(mouse.x, mouse.y);
        // страховка: если релиз произошёл вне кнопки, сбрасываем зажатие
        if (this->pressed_ && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            this->pressed_ = false;
        }
    }

    void FIconButton::onMousePress() {
        this->pressed_ = this->hovered_;
    }

    void FIconButton::onMouseRelease() {
        if (this->pressed_ && this->hovered_ && this->onClick_) {
            this->onClick_();
        }
        this->pressed_ = false;
    }

    void FIconButton::onDraw() {
        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);

        const Theme& theme = Theme::instance();
        if (this->pressed_ && this->hovered_) {
            DrawRectangleRec({ax, ay, this->size_, this->size_}, theme.pressOverlay);
        } else if (this->hovered_ || this->active_) {
            DrawRectangleRec({ax, ay, this->size_, this->size_}, theme.hoverOverlay);
        }

        const Color iconColor = this->active_ ? theme.iconActive : theme.iconIdle;
        IconFont::instance().drawCentered(this->icon_, ax, ay, this->size_, this->iconSize_, iconColor);
    }
}
