#include "Base.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <ostream>
#include <sstream>
#include "termcolor.hpp"
#include <iostream>

namespace Base {
    Logger& Logger::instance() {
        static Logger logger;
        return logger;
    }

    void Logger::log(LogLevel level, const std::string_view &message) {
        std::lock_guard<std::mutex> lock(this->mutex_);

        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << "[" << to_str(level) << " " << 
            std::put_time(std::localtime(&in_time_t), 
            "%Y-%m-%d %H:%M:%S") << "] " << message;

        switch(level) {
            case Base::LogLevel::INFO: std::cout << termcolor::green << ss.str() << termcolor::reset << std::endl; break;
            case Base::LogLevel::WARN: std::cout << termcolor::yellow << ss.str() << termcolor::reset << std::endl; break;
            case Base::LogLevel::ERR:  std::cout << termcolor::red << ss.str() << termcolor::reset << std::endl; break;
            default: std::cout << termcolor::white << ss.str() << termcolor::reset << std::endl; break;
        }
    }
}