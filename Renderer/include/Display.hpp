#pragma once

#include "components/Component.hpp"
#include <memory>
#include <vector>

namespace Renderer {
    // display класс для манипуляции окном а также вгрузкой в него сцен
    // а также их функций и врапперов
    class Display {
    public:
        Display();
        ~Display() = default;

        void setWindowTitle(const char* title);
        void setWindowDim(const int width, const int height);
        void mainLoop();
        int addComponent(std::unique_ptr<Component> c);

    private:
        std::vector<std::unique_ptr<Component>> components;

        // ищет самый верхний (глубокий) компонент под точкой
        Component* hitAt(float mx, float my) const;

        void buildUi();
    };
}
