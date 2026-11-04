#include "Base.hpp"

int main(int argc, char** argv) {
    auto& instance = Base::Logger::instance();
    instance.log(Base::LogLevel::INFO, "Hello from flatcode");
    return 0;
}