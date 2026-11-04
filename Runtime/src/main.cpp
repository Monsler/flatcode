#include "Base.hpp"
#include "Display.hpp"

int main(int argc, char** argv) {
    auto& instance = Base::Logger::instance();
    instance.log(Base::LogLevel::INFO, "Hello from flatcode");

    Renderer::Display display;
    display.setWindowTitle("FlatCode");

    display.mainLoop();
    return 0;
}