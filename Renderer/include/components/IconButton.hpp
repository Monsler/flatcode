#pragma once
#include "Component.hpp"
#include "IconsLucide.h"
#include <functional>

namespace Renderer {

    // квадратная кнопка с lucide иконкой (кнопка активити бара vscode)
    class FIconButton : public Component {
    public:
        FIconButton(const char* icon, const float size = 48.0f);

        void setIcon(const char* icon) { icon_ = icon; }
        void setActive(const bool active) { active_ = active; }
        [[nodiscard]] bool active() const { return active_; }
        [[nodiscard]] bool hovered() const { return hovered_; }

        void setOnClick(std::function<void()> callback) { onClick_ = std::move(callback); }

        bool hitTest(float mx, float my) const override;

    protected:
        void onDraw() override;
        void onUpdate(float dt) override;
        void onMousePress() override;
        void onMouseRelease() override;

    private:
        const char* icon_;
        float size_;
        float iconSize_ = 24.0f;
        bool active_ = false;
        bool hovered_ = false;
        bool pressed_ = false;
        std::function<void()> onClick_;
    };
}
