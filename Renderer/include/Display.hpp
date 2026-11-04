#pragma once
#include <string>

namespace Renderer {
    // display класс для манипуляции окном а также вгрузкой в него сцен
    // а также их функций и врапперов
    // TODO: класс сцен, имплементация логики
    class Display {
    public:
        Display();
        ~Display() = default;

        void setWindowTitle(std::string& title);
        void setWindowDim(const int width, const int height);
        void mainLoop();
    };
}