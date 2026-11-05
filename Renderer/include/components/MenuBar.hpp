#pragma once

#include "Component.hpp"
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

namespace Renderer {

    class FFileTree;

    // верхняя панель как в vscode: слева меню (File, Edit, ...),
    // рядом стрелки навигации, по центру поле поиска с именем проекта,
    // справа кнопки раскладки. кнопки окна и sign in не входят
    //
    // клик по пункту меню открывает выпадающий список; при открытом меню
    // наведение на соседний пункт переключает его, как в настольных приложениях
    //
    // поле поиска — это quick open как в vscode: клик по нему или Ctrl+P
    // открывает список файлов с фильтром по вводу; Enter раскрывает файл в
    // дереве explorer-а
    class FMenuBar : public Component {
    public:
        struct MenuItem {
            const char* label = nullptr; // nullptr = разделитель
            const char* shortcut = "";
            bool checked = false;
            std::function<void()> action;
            // если задан, галочка берётся из него в момент отрисовки
            // (например, Explorer отражает реальную видимость сайдбара)
            std::function<bool()> checkedProbe;
        };

        struct Menu {
            const char* title;
            std::vector<MenuItem> items;
        };

        explicit FMenuBar(std::string workspaceName);

        void setWorkspaceName(std::string name) { workspace_ = std::move(name); }

        // чем ответить на вопрос "виден ли сайдбар" (для layout-кнопки)
        void setSidebarProbe(std::function<bool()> probe) { sidebarVisible_ = std::move(probe); }
        // что сделать по запросу "переключи сайдбар" (кнопка и View -> Explorer)
        void setOnToggleSidebar(std::function<void()> action) { onToggleSidebar_ = std::move(action); }
        // дерево explorer-а: из него берутся недавние файлы и куда раскрывается результат
        void setFileTree(FFileTree* tree) { tree_ = tree; }
        // корень проекта (для индекса быстрого поиска)
        void setProjectRoot(std::string root) { projectRoot_ = std::move(root); }
        // File -> Save (Ctrl+S из меню; из самого редактора — свой обработчик)
        void setOnSaveFile(std::function<void()> action) { onSaveFile_ = std::move(action); }

        static constexpr float HEIGHT = 35.0f;

        bool hitTest(float mx, float my) const override;

    protected:
        void onUpdate(float dt) override;
        void onDraw() override;
        void onMousePress() override;

    private:
        // маленькая иконка-кнопка на панели (стрелки, layout-тогглы)
        struct ToolButton {
            const char* icon;
            bool isToggle = false;  // тоггл держит активное состояние
            bool active = false;    // состояние тоггла (для не-тогглов не важно)
            bool primary = false;   // состояние берётся из sidebarVisible_, а не из active
            std::function<void()> onClick;
        };

        void buildMenus();
        void buildTools();

        // --- quick open ---------------------------------------------------
        void openQuickOpen();
        void closeQuickOpen();
        void updateResults();  // пересобирает results_ по query_
        void acceptResult();   // раскрыть выбранный файл в дереве
        Rectangle quickOpenRect() const;

        float menuX(std::size_t index) const;          // левый край пункта меню
        float menuWidth(std::size_t index) const;      // ширина пункта меню
        float dropdownWidth(std::size_t index) const;  // ширина выпадающего списка
        Rectangle dropdownRect(std::size_t index) const;
        Rectangle toolRect(std::size_t index) const;   // кнопки считаются справа налево
        Rectangle navRect(std::size_t index) const;    // стрелки навигации
        Rectangle searchRect() const;

        std::vector<Menu> menus_;
        std::vector<ToolButton> tools_; // правая группа: customize + layout-тогглы
        std::vector<ToolButton> nav_;   // стрелки навигации слева
        std::string workspace_;
        std::string projectRoot_;
        FFileTree* tree_ = nullptr;

        int openMenu_ = -1;   // индекс раскрытого меню
        int hoverMenu_ = -1;  // пункт меню под курсором
        int hoverItem_ = -1;  // строка выпадающего меню под курсором
        int hoverTool_ = -1;  // иконка-кнопка под курсором
        int hoverNav_ = -1;   // стрелка навигации под курсором
        bool hoverSearch_ = false;

        // --- quick open ---------------------------------------------------
        bool quickOpen_ = false;
        std::string query_;
        std::vector<std::string> index_;    // все файлы проекта (сканируется при первом вводе)
        std::vector<std::string> results_;  // пути под текущий запрос
        int selectedResult_ = 0;
        int hoverResult_ = -1;
        float backspaceTimer_ = 0.0f;       // автоповтор при зажатом backspace

        std::function<bool()> sidebarVisible_;
        std::function<void()> onToggleSidebar_;
        std::function<void()> onSaveFile_;

        static constexpr float MENU_PAD_X = 8.0f;   // горизонтальный паддинг пункта меню
        static constexpr float MENU_FONT = 13.0f;
        static constexpr float ITEM_H = 28.0f;      // высота строки выпадающего меню
        static constexpr float TOOL_W = 30.0f;      // ширина иконки-кнопки
        static constexpr float ICON = 15.0f;
        static constexpr float RESULT_H = 26.0f;    // высота строки результатов поиска
    };
}
