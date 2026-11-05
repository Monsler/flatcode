#pragma once

#include <mutex>
#include <string>
namespace Base {
    // стейт хранит все текущие данные живущие в редакторе бтв кстати
    class State {
    public:
    State& instance();
    const std::string& getCurrentWorkspace();
    void setCurrentWorkspace(std::string workspace);
    
    private:
    State() = default;
    ~State() = default;

    std::string currentWorkspace;

    mutable std::mutex mutex;
    };
}