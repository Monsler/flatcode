#pragma once

#include <string>
#include <mutex>

namespace Base {
    enum class LogLevel {
        INFO,
        WARN,
        ERR
    };

    inline const char* to_str(LogLevel level) {
        switch(level) {
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERR: return "ERROR";
        }

        return "UNKNOWN";
    }

    class Logger {
    public:
        static Logger& instance();
        void log(LogLevel level, const std::string_view& message);

    private:
        Logger() = default;
        ~Logger() = default;

        std::mutex mutex_;
    };
}