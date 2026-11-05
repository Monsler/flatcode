#include "Display.hpp"
#include "EditorFont.hpp"
#include "IconFont.hpp"
#include "Logger.hpp"
#include "MainFont.hpp"
#include "Theme.hpp"
#include "components/Editor.hpp"
#include "components/FileTree.hpp"
#include "components/IconButton.hpp"
#include "components/MenuBar.hpp"
#include "components/Rectangle.hpp"
#include "components/Sidebar.hpp"
#include <raylib.h>

#include <filesystem>
#include <string>

namespace Renderer {
    namespace {
        // ищем корень проекта: exe обычно лежит внутри build/, поэтому
        // поднимаемся от него до каталога с CMakeLists.txt;
        // запасной вариант — текущий каталог процесса
        std::string detectProjectRoot() {
            namespace fs = std::filesystem;
            fs::path dir = fs::path(GetApplicationDirectory()).parent_path();
            for (int i = 0; i < 4 && dir.has_parent_path(); ++i) {
                if (fs::exists(dir / "CMakeLists.txt")) {
                    return dir.string();
                }
                dir = dir.parent_path();
            }
            return fs::current_path().string();
        }
    }

    Display::Display() {
        SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
        InitWindow(800, 500, "FlatCode");
        Base::Logger::instance().log(Base::LogLevel::INFO, "Window initialization done");

        IconFont::instance().load();
        MainFont::instance().load();
        EditorFont::instance().load();
        this->buildUi();
    }

    void Display::buildUi() {
        const std::string projectRoot = detectProjectRoot();
        auto sidebar = std::make_unique<FSidebar>();

        auto tree = std::make_unique<FFileTree>(projectRoot);
        FFileTree* treePtr = tree.get();

        FSidebar::Item explorer;
        explorer.icon = ICON_LC_FILES;
        explorer.title = "Explorer";
        explorer.content = std::move(tree);

        std::vector<FSidebar::Item> items;
        items.push_back(std::move(explorer));
        items.push_back({ICON_LC_SEARCH, "Search", nullptr});
        items.push_back({ICON_LC_GIT_BRANCH, "Source Control", nullptr});
        items.push_back({ICON_LC_BUG, "Run and Debug", nullptr});
        items.push_back({ICON_LC_BLOCKS, "Extensions", nullptr});

        sidebar->setItems(std::move(items));
        sidebar->setActive(0);

        // сайдбар стоит под меню-панелью; меню-панель знает, как его спрятать
        sidebar->setPosition(0, FMenuBar::HEIGHT);
        FSidebar* sidebarPtr = sidebar.get();

        // редактор занимает всё справа от сайдбара и открывает файлы из дерева
        auto editor = std::make_unique<FEditor>();
        FEditor* editorPtr = editor.get();
        editor->setLeftInset([sidebarPtr] {
            return sidebarPtr->visible() ? sidebarPtr->totalWidth() : 0.0f;
        });
        treePtr->setOnOpenFile([editorPtr](const std::string& path) {
            editorPtr->openFile(path);
        });

        auto menuBar = std::make_unique<FMenuBar>(
            std::filesystem::path(projectRoot).filename().string());
        menuBar->setSidebarProbe([sidebarPtr] { return sidebarPtr->visible(); });
        menuBar->setOnToggleSidebar([sidebarPtr] {
            sidebarPtr->setVisible(!sidebarPtr->visible());
        });
        // быстрый поиск берёт файлы и недавние из дерева, результат раскрывает в нём
        menuBar->setFileTree(treePtr);
        menuBar->setProjectRoot(projectRoot);
        menuBar->setOnSaveFile([editorPtr] { editorPtr->save(); });

        this->addComponent(std::move(sidebar));
        // редактор — средний слой, поверх него меню-панель
        this->addComponent(std::move(editor));
        // меню-панель последней: рисуется поверх и первой ловит клики
        this->addComponent(std::move(menuBar));
    }

    void Display::setWindowDim(const int width, const int height) {
        SetWindowSize(width, height);
    }

    void Display::setWindowTitle(const char* title) {
        SetWindowTitle(title);
    }

    int Display::addComponent(std::unique_ptr<Component> c) {
        this->components.push_back(std::move(c));
        return components.size() - 1;
    }

    Component* Display::hitAt(const float mx, const float my) const {
        for (auto it = this->components.rbegin(); it != this->components.rend(); ++it) {
            if (Component* hit = (*it)->hitAt(mx, my)) {
                return hit;
            }
        }
        return nullptr;
    }

    // основной цикл дисплея
    void Display::mainLoop() {
        while (!WindowShouldClose()) {
            const float dt = GetFrameTime();
            const Vector2 mouse = GetMousePosition();

            for (auto& c : this->components) {
                c->update(dt);
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (Component* c = this->hitAt(mouse.x, mouse.y)) {
                    c->mousePress();
                }
            }
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                if (Component* c = this->hitAt(mouse.x, mouse.y)) {
                    c->mouseRelease();
                }
            }

            BeginDrawing();
            ClearBackground(Theme::instance().windowBg);
            for (auto& c : this->components) {
                c->draw();
            }
            EndDrawing();
        }
    }
}
