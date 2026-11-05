#pragma once

#include "Component.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Renderer {

    // рабочее дерево файлов в духе jetbrains:
    //  - дети папки читаются с диска только при первом раскрытии (lazy load)
    //  - из дерева строится плоский список видимых строк, и пересобирается
    //    он только когда структура меняется, а не каждый кадр
    //  - рисуются и тестируются только строки, попадающие в вьюпорт
    //    (виртуализация), поэтому большие проекты не просаживают fps
    //  - мусор вроде .git или build по умолчанию не показывается
    class FFileTree : public Component {
    public:
        explicit FFileTree(std::string rootPath);

        void setRoot(std::string rootPath);
        void setExcluded(std::vector<std::string> names);

        [[nodiscard]] const std::string& rootPath() const { return root_->path; }

        // раскрывает путь к файлу (догружая папки по дороге), выделяет его
        // и прокручивает дерево к нему; файл попадает в recently opened
        void reveal(const std::string& path);

        // файлы, кликнутые в дереве или открытые через быстрый поиск
        [[nodiscard]] const std::vector<std::string>& recents() const { return recents_; }

        // кому сообщать об открытии файла (клик по файлу и reveal из quick open);
        // по этому колбэку редактор открывает документ
        void setOnOpenFile(std::function<void(const std::string&)> callback) {
            onOpenFile_ = std::move(callback);
        }

        // список всех файлов проекта (для индекса быстрого поиска)
        static std::vector<std::string> scanFiles(const std::string& root);

        // иконка по имени файла: расширения, знакомые редактору, подсвечиваем
        static const char* fileIcon(const std::string& name);

        bool hitTest(float mx, float my) const override;

    protected:
        void onUpdate(float dt) override;
        void onDraw() override;
        void onMousePress() override;
        void onResize(float width, float height) override;

    private:
        struct Node {
            std::string name;   // имя для отображения
            std::string path;   // полный путь на диске
            bool dir = false;
            bool expanded = false;
            bool loaded = false; // дети уже прочитаны с диска?
            int depth = 0;
            Node* parent = nullptr;
            std::vector<std::unique_ptr<Node>> children;
        };

        void loadChildren(Node& node);
        void flatten(); // пересобирает visible_ из раскрытых узлов
        void toggle(Node& node);
        bool excluded(const std::string& name) const;
        void pushRecent(const std::string& path);
        void notifyOpen(const std::string& path);

        std::unique_ptr<Node> root_;
        std::vector<Node*> visible_;    // плоский список строк на экране
        std::vector<std::string> excludes_;
        std::vector<std::string> recents_;
        std::function<void(const std::string&)> onOpenFile_;
        Node* selected_ = nullptr;
        int hoverRow_ = -1;
        float scroll_ = 0.0f;
        float maxScroll_ = 0.0f;
        float width_ = 240.0f;
        float height_ = 400.0f;
        bool dirty_ = true;

        static constexpr float ROW = 28.0f;    // высота строки дерева
        static constexpr float INDENT = 16.0f; // отступ на уровень вложенности
        static constexpr float ICON = 17.0f;   // размер значка файла/папки
        static constexpr float TEXT = 15.0f;   // кегль имени файла
        static constexpr float CHEVRON = 13.0f;// стрелка раскрытия
    };
}
