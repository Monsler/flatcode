#pragma once
#include "Component.hpp"
#include "components/IconButton.hpp"
#include "components/Rectangle.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Renderer {

    // сайдбар как в vscode:
    //  - слева вертикальный activity bar с lucide иконками
    //  - справа панель с заголовком и опциональным контентом
    //    (например, деревом файлов для Explorer)
    // повторный клик по активной иконке сворачивает панель
    class FSidebar : public Component {
    public:
        struct Item {
            const char* icon;   // имя глифа lucide (ICON_LC_*)
            const char* title;  // заголовок панели
            std::unique_ptr<Component> content; // чем заполнить панель (может быть пусто)
        };

        FSidebar();

        void setItems(std::vector<Item> items);
        void setActive(int index);
        [[nodiscard]] int active() const { return active_; }
        // полная ширина сайдбара вместе с панелью (или только activity bar)
        [[nodiscard]] float totalWidth() const { return ACTIVITY_WIDTH + (this->panelVisible_ ? PANEL_WIDTH : 0.0f); }
        void setOnSelect(std::function<void(int)> callback) { onSelect_ = std::move(callback); }

        bool hitTest(float mx, float my) const override;

    protected:
        void onDrawAfterChildren() override;
        void onUpdate(float dt) override;

    private:
        static constexpr float ACTIVITY_WIDTH = 48.0f;
        static constexpr float PANEL_WIDTH = 240.0f;
        static constexpr float PANEL_TOP = 40.0f; // высота шапки панели

        void rebuild();
        void layout();
        void updateIndicator();
        void attachActiveContent();
        void detachActiveContent();

        std::vector<Item> items_;
        int active_ = -1;
        bool panelVisible_ = true;
        float height_ = 0.0f;

        FRectangle* activityBar_ = nullptr;
        FRectangle* panel_ = nullptr;
        FRectangle* indicator_ = nullptr;
        FIconButton* settingsButton_ = nullptr;
        std::vector<FIconButton*> buttons_;

        Component* content_ = nullptr; // контент активной панели (владение в items_)
        int contentIndex_ = -1;

        std::function<void(int)> onSelect_;
    };
}
