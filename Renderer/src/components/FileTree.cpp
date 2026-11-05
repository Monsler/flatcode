#include "components/FileTree.hpp"
#include "IconFont.hpp"
#include "IconsLucide.h"
#include "MainFont.hpp"
#include "Theme.hpp"
#include "raylib.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Renderer {
    namespace {
        // мусорные каталоги, которые не показываем нигде: ни в дереве, ни в поиске
        const std::vector<std::string>& defaultExcludes() {
            static const std::vector<std::string> names = {
                ".git", ".cache", "__pycache__", "node_modules",
                "build", "cmake-build-debug", "cmake-build-release",
            };
            return names;
        }
    }

    FFileTree::FFileTree(std::string rootPath) {
        this->setExcluded(defaultExcludes());
        this->setRoot(std::move(rootPath));
    }

    void FFileTree::setRoot(std::string rootPath) {
        auto root = std::make_unique<Node>();
        root->name = std::filesystem::path(rootPath).filename().string();
        if (root->name.empty()) {
            root->name = rootPath; // случай корня файловой системы
        }
        root->path = std::move(rootPath);
        root->dir = true;
        root->expanded = true;
        this->root_ = std::move(root);
        this->selected_ = nullptr;
        this->scroll_ = 0.0f;
        this->dirty_ = true;
    }

    void FFileTree::setExcluded(std::vector<std::string> names) {
        this->excludes_ = std::move(names);
    }

    bool FFileTree::excluded(const std::string& name) const {
        return std::find(this->excludes_.begin(), this->excludes_.end(), name) != this->excludes_.end();
    }

    // недавние файлы: без дублей, самый свежий — в начале, максимум 8
    void FFileTree::pushRecent(const std::string& path) {
        std::erase(this->recents_, path);
        this->recents_.insert(this->recents_.begin(), path);
        if (this->recents_.size() > 8) {
            this->recents_.resize(8);
        }
    }

    // файл открыли (клик или quick open): дереву — в недавние, редактору — в документ
    void FFileTree::notifyOpen(const std::string& path) {
        this->pushRecent(path);
        if (this->onOpenFile_) {
            this->onOpenFile_(path);
        }
    }

    // раскрывает путь к файлу: догружает папки по цепочке, выделяет узел
    // и прокручивает дерево так, чтобы строка была примерно в середине вьюпорта
    void FFileTree::reveal(const std::string& path) {
        if (!this->root_) {
            return;
        }

        // путь к цели относительно корня дерева
        std::error_code ec;
        const std::filesystem::path rel =
            std::filesystem::relative(path, this->root_->path, ec);
        if (ec || rel.empty()) {
            return;
        }

        Node* cur = this->root_.get();
        cur->expanded = true;
        this->loadChildren(*cur);
        for (const auto& part : rel) {
            const std::string name = part.string();
            Node* next = nullptr;
            for (auto& child : cur->children) {
                if (child->name == name) {
                    next = child.get();
                    break;
                }
            }
            if (!next) {
                return; // файла нет на диске (или он исключён)
            }
            if (next->dir) {
                next->expanded = true;
                this->loadChildren(*next);
            }
            cur = next;
        }

        this->selected_ = cur;
        if (!cur->dir) {
            this->notifyOpen(cur->path);
        }

        // пересобираем видимый список и подводим прокрутку к строке
        this->dirty_ = false;
        this->flatten();
        this->maxScroll_ = std::max(0.0f, static_cast<float>(this->visible_.size()) * ROW - this->height_);
        for (std::size_t i = 0; i < this->visible_.size(); ++i) {
            if (this->visible_[i] == cur) {
                const float rowY = static_cast<float>(i) * ROW;
                this->scroll_ = std::clamp(rowY - (this->height_ - ROW) * 0.5f, 0.0f, this->maxScroll_);
                break;
            }
        }
    }

    // индекс всех файлов проекта для быстрого поиска
    std::vector<std::string> FFileTree::scanFiles(const std::string& root) {
        namespace fs = std::filesystem;
        std::vector<std::string> out;
        std::error_code ec;
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            const std::string name = it->path().filename().string();
            if (std::find(defaultExcludes().begin(), defaultExcludes().end(), name) != defaultExcludes().end()) {
                if (it->is_directory(ec)) {
                    it.disable_recursion_pending();
                }
                ec.clear();
                continue;
            }
            if (!it->is_regular_file(ec) || ec) {
                ec.clear();
                continue;
            }
            out.push_back(it->path().string());
            if (out.size() >= 4000) {
                break; // предохранитель для очень больших репозиториев
            }
        }
        return out;
    }

    // читает детей папки с диска один раз: папки сверху, файлы ниже,
    // обе группы по алфавиту без учёта регистра
    void FFileTree::loadChildren(Node& node) {
        if (node.loaded || !node.dir) {
            return;
        }
        node.loaded = true;

        std::vector<std::unique_ptr<Node>> dirs;
        std::vector<std::unique_ptr<Node>> files;

        std::error_code ec;
        for (auto it = std::filesystem::directory_iterator(node.path, ec);
             it != std::filesystem::directory_iterator(); it.increment(ec)) {
            const std::string name = it->path().filename().string();
            if (this->excluded(name)) {
                continue;
            }
            auto child = std::make_unique<Node>();
            child->name = name;
            child->path = it->path().string();
            child->dir = it->is_directory(ec);
            child->depth = node.depth + 1;
            child->parent = &node;
            (child->dir ? dirs : files).push_back(std::move(child));
            if (ec) {
                ec.clear();
            }
        }

        const auto byName = [](const std::unique_ptr<Node>& a, const std::unique_ptr<Node>& b) {
            return std::lexicographical_compare(
                a->name.begin(), a->name.end(), b->name.begin(), b->name.end(),
                [](const unsigned char ca, const unsigned char cb) {
                    return std::tolower(ca) < std::tolower(cb);
                });
        };
        std::sort(dirs.begin(), dirs.end(), byName);
        std::sort(files.begin(), files.end(), byName);

        node.children.reserve(dirs.size() + files.size());
        for (auto& d : dirs) {
            node.children.push_back(std::move(d));
        }
        for (auto& f : files) {
            node.children.push_back(std::move(f));
        }
    }

    void FFileTree::flatten() {
        this->visible_.clear();
        if (!this->root_) {
            return;
        }
        // лениво догружаем детей раскрытых узлов прямо здесь
        if (this->root_->expanded) {
            this->loadChildren(*this->root_);
        }
        // итеративный обход в глубину, чтобы не упереться в стек на глубоких путях
        std::vector<Node*> stack;
        stack.push_back(this->root_.get());
        while (!stack.empty()) {
            Node* n = stack.back();
            stack.pop_back();
            this->visible_.push_back(n);
            if (n->expanded) {
                this->loadChildren(*n);
                for (auto it = n->children.rbegin(); it != n->children.rend(); ++it) {
                    stack.push_back(it->get());
                }
            }
        }
    }

    void FFileTree::toggle(Node& node) {
        node.expanded = !node.expanded;
        this->dirty_ = true;
    }

    bool FFileTree::hitTest(const float mx, const float my) const {
        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);
        return mx >= ax && mx <= ax + this->width_ && my >= ay && my <= ay + this->height_;
    }

    void FFileTree::onResize(const float width, const float height) {
        this->width_ = width;
        this->height_ = height;
    }

    void FFileTree::onUpdate(const float) {
        if (this->dirty_) {
            this->flatten();
            this->dirty_ = false;
            this->maxScroll_ = std::max(0.0f, static_cast<float>(this->visible_.size()) * ROW - this->height_);
        }

        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);
        const Vector2 mouse = GetMousePosition();
        const bool inside = mouse.x >= ax && mouse.x <= ax + this->width_ &&
                            mouse.y >= ay && mouse.y <= ay + this->height_;

        // скролл колесом только когда курсор над деревом, по три строки за щелчок;
        // колесо в raylib не consume-ится — без проверки крутились бы все панели сразу
        if (inside) {
            this->scroll_ -= GetMouseWheelMove() * ROW * 3.0f;
            this->scroll_ = std::clamp(this->scroll_, 0.0f, this->maxScroll_);
            this->hoverRow_ = static_cast<int>((mouse.y - ay + this->scroll_) / ROW);
            if (this->hoverRow_ >= static_cast<int>(this->visible_.size())) {
                this->hoverRow_ = -1;
            }
        } else {
            this->hoverRow_ = -1;
        }
    }

    void FFileTree::onMousePress() {
        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);
        const Vector2 mouse = GetMousePosition();
        const int row = static_cast<int>((mouse.y - ay + this->scroll_) / ROW);
        if (row < 0 || row >= static_cast<int>(this->visible_.size())) {
            return;
        }
        Node* node = this->visible_[static_cast<std::size_t>(row)];
        this->selected_ = node;
        if (node->dir) {
            this->toggle(*node); // клик по папке раскрывает/сворачивает её
        } else {
            this->notifyOpen(node->path); // клик по файлу: недавние + редактор
        }
    }

    // значок по имени файла: расширения, знакомые редактору, подсвечиваем
    const char* FFileTree::fileIcon(const std::string& name) {
        if (name == "CMakeLists.txt" || name == "Makefile") {
            return ICON_LC_FILE_BRACES;
        }
        const auto dot = name.rfind('.');
        if (dot == std::string::npos) {
            return ICON_LC_FILE;
        }
        std::string ext = name.substr(dot + 1);
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "c" ||
            ext == "hpp" || ext == "hh" || ext == "h" || ext == "inl") {
            return ICON_LC_FILE_CODE;
        }
        if (ext == "json") {
            return ICON_LC_FILE_JSON;
        }
        if (ext == "md" || ext == "txt" || ext == "log") {
            return ICON_LC_FILE_TEXT;
        }
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "svg" ||
            ext == "bmp" || ext == "ico" || ext == "gif") {
            return ICON_LC_FILE_IMAGE;
        }
        if (ext == "zip" || ext == "tar" || ext == "gz" || ext == "7z" || ext == "rar") {
            return ICON_LC_FILE_ARCHIVE;
        }
        if (ext == "ttf" || ext == "otf") {
            return ICON_LC_FILE_TYPE_2;
        }
        if (ext == "sh" || ext == "py" || ext == "rs") {
            return ICON_LC_FILE_TERMINAL;
        }
        return ICON_LC_FILE;
    }

    void FFileTree::onDraw() {
        if (this->visible_.empty()) {
            return;
        }
        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);

        const Theme& theme = Theme::instance();

        // виртуализация: рисуем только строки, пересекающие вьюпорт
        const int firstRow = std::max(0, static_cast<int>(this->scroll_ / ROW) - 1);
        const int lastRow = std::min(static_cast<int>(this->visible_.size()),
                                     static_cast<int>((this->scroll_ + this->height_) / ROW) + 1);

        for (int i = firstRow; i < lastRow; ++i) {
            const Node* node = this->visible_[static_cast<std::size_t>(i)];
            const float rowY = ay + static_cast<float>(i) * ROW - this->scroll_;

            if (node == this->selected_) {
                DrawRectangleRec({ax, rowY, this->width_, ROW}, theme.rowSelectedBg);
            } else if (i == this->hoverRow_) {
                DrawRectangleRec({ax, rowY, this->width_, ROW}, theme.rowHoverBg);
            }

            float cx = ax + 6.0f + static_cast<float>(node->depth) * INDENT;

            // стрелка раскрытия: у ещё не читанных папок всегда есть,
            // а у прочитанных пустых — уже нет
            if (node->dir && (!node->loaded || !node->children.empty())) {
                const char* chev = node->expanded ? ICON_LC_CHEVRON_DOWN : ICON_LC_CHEVRON_RIGHT;
                IconFont::instance().draw(chev, cx, rowY + (ROW - CHEVRON) * 0.5f, CHEVRON, theme.chevron);
            }
            cx += 18.0f;

            if (node->dir) {
                const char* folder = node->expanded ? ICON_LC_FOLDER_OPEN : ICON_LC_FOLDER;
                IconFont::instance().draw(folder, cx, rowY + (ROW - ICON) * 0.5f, ICON, theme.textDim);
            } else {
                IconFont::instance().draw(this->fileIcon(node->name), cx, rowY + (ROW - ICON) * 0.5f, ICON, theme.textDim);
            }
            cx += ICON + 6.0f;

            const float textY = rowY + (ROW - TEXT) * 0.5f;
            const float maxWidth = this->width_ - (cx - ax) - 8.0f;
            MainFont::instance().drawEllipsized(node->name, cx, textY, maxWidth, TEXT, theme.text);
        }

        // тонкий скроллбар как в jetbrains
        if (this->maxScroll_ > 0.0f) {
            const float totalPx = static_cast<float>(this->visible_.size()) * ROW;
            const float thumbH = std::max(24.0f, this->height_ * this->height_ / totalPx);
            const float thumbY = ay + this->scroll_ / this->maxScroll_ * (this->height_ - thumbH);
            DrawRectangleRec({ax + this->width_ - 4.0f, thumbY, 3.0f, thumbH}, theme.scrollbarThumb);
        }
    }
}
