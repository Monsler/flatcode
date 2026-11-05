#include "State.hpp"
#include <mutex>
#include <utility>

namespace Base {
    const std::string& State::getCurrentWorkspace() {
        std::lock_guard<std::mutex> lock(this->mutex);
        return this->currentWorkspace;
    }

    void State::setCurrentWorkspace(std::string workspace) {
        std::lock_guard<std::mutex> lock(this->mutex);
        this->currentWorkspace = std::move(workspace);
    }

    State& State::instance() {
        static State state;
        return state;
    }
}