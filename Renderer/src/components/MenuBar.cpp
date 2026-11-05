#include "components/MenuBar.hpp"
#include "components/FileTree.hpp"
#include "IconFont.hpp"
#include "IconsLucide.h"
#include "MainFont.hpp"
#include "Theme.hpp"
#include "raylib.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

namespace {

    // высота строки-разделителя в выпадающем меню
    constexpr float SEPARATOR_H = 9.0f;
    // внутренний паддинг выпадающего списка
    constexpr float DROP_PAD = 6.0f;

    // скругление для прямоугольника с данным радиусом
    float roundnessFor(const float radius, const Rectangle& rec) {
        const float minSide = std::min(rec.width, rec.height);
        return minSide > 0.0f ? std::clamp(2.0f * radius / minSide, 0.0f, 1.0f) : 0.0f;
    }

    // дописывает codepoint в utf-8 строку
    void appendUtf8(std::string& out, const int cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    // убирает с конца строки одну utf-8 последовательность целиком
    void popUtf8(std::string& s) {
        while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) {
            s.pop_back();
        }
        if (!s.empty()) {
            s.pop_back();
        }
    }

    // подстрочный поиск без учёта регистра; для не-ascii байтов tolower в
    // локали C тождественен, так что utf-8 не ломается (без fold для кириллицы)
    bool containsCI(const std::string& hay, const std::string& needle) {
        if (needle.empty()) {
            return true;
        }
        const auto lowered = [](std::string s) {
            for (char& c : s) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return s;
        };
        return lowered(hay).find(lowered(needle)) != std::string::npos;
    }
}

namespace Renderer {

    FMenuBar::FMenuBar(std::string workspaceName) : workspace_(std::move(workspaceName)) {
        this->buildMenus();
        this->buildTools();
    }

    void FMenuBar::buildMenus() {
        this->menus_ = {
            {"File", {
                {"New Text File", "Ctrl+N"},
                {"Open File...", "Ctrl+O"},
                {"Open Folder...", "Ctrl+K Ctrl+O"},
                {},
                {"Save", "Ctrl+S", false, [this] {
                    if (this->onSaveFile_) {
                        this->onSaveFile_();
                    }
                }},
                {"Save As...", "Ctrl+Shift+S"},
                {},
                {"Auto Save", "", true},
                {},
                {"Exit", "", false, [] { CloseWindow(); }},
            }},
            {"Edit", {
                {"Undo", "Ctrl+Z"},
                {"Redo", "Ctrl+Shift+Z"},
                {},
                {"Cut", "Ctrl+X"},
                {"Copy", "Ctrl+C"},
                {"Paste", "Ctrl+V"},
                {},
                {"Find", "Ctrl+F"},
                {"Replace", "Ctrl+H"},
            }},
            {"Selection", {
                {"Select All", "Ctrl+A"},
                {},
                {"Expand Selection", "Shift+Alt+Right"},
                {"Shrink Selection", "Shift+Alt+Left"},
                {},
                {"Copy Line Up", "Shift+Alt+Up"},
                {"Copy Line Down", "Shift+Alt+Down"},
                {"Move Line Up", "Alt+Up"},
                {"Move Line Down", "Alt+Down"},
                {},
                {"Add Cursor Above", "Ctrl+Alt+Up"},
                {"Add Cursor Below", "Ctrl+Alt+Down"},
            }},
            {"View", {
                {"Command Palette...", "Ctrl+Shift+P"},
                {"Open View..."},
                {},
                {"Explorer", "Ctrl+Shift+E", false, [this] {
                    if (this->onToggleSidebar_) {
                        this->onToggleSidebar_();
                    }
                }, [this] {
                    return this->sidebarVisible_ ? this->sidebarVisible_() : true;
                }},
                {"Search", "Ctrl+Shift+F"},
                {"Source Control", "Ctrl+Shift+G"},
                {"Run", "Ctrl+Shift+D"},
                {"Extensions", "Ctrl+Shift+X"},
                {},
                {"Problems", "Ctrl+Shift+M"},
                {"Output", "Ctrl+Shift+U"},
                {"Word Wrap", "Alt+Z"},
            }},
            {"Go", {
                {"Back", "Ctrl+Alt+Left"},
                {"Forward", "Ctrl+Alt+Right"},
                {},
                {"Go to File...", "Ctrl+P"},
                {"Go to Symbol in Editor...", "Ctrl+Shift+O"},
                {"Go to Line/Column...", "Ctrl+G"},
            }},
            {"Run", {
                {"Start Debugging", "F5"},
                {"Run Without Debugging", "Ctrl+F5"},
                {"Stop Debugging", "Shift+F5"},
                {},
                {"Open Configurations"},
                {"Add Configuration..."},
            }},
            {"Terminal", {
                {"New Terminal", "Ctrl+`"},
                {"Split Terminal", "Ctrl+Shift+5"},
                {},
                {"Run Task..."},
                {"Configure Tasks..."},
                {},
                {"Clear Terminal"},
            }},
            {"Help", {
                {"Welcome"},
                {"Show All Commands", "Ctrl+Shift+P"},
                {},
                {"About"},
            }},
        };
    }

    void FMenuBar::buildTools() {
        this->tools_ = {
            {ICON_LC_LAYOUT_GRID},                                    // customize layout
            {ICON_LC_PANEL_LEFT, true, false, true, [this] {          // primary side bar
                if (this->onToggleSidebar_) {
                    this->onToggleSidebar_();
                }
            }},
            {ICON_LC_PANEL_BOTTOM, true},                             // bottom panel (пока локальный тоггл)
            {ICON_LC_PANEL_RIGHT, true},                              // secondary side bar (пока локальный тоггл)
        };
        this->nav_ = {
            {ICON_LC_ARROW_LEFT},  // back
            {ICON_LC_ARROW_RIGHT}, // forward
        };
    }

    // --- геометрия -------------------------------------------------------

    float FMenuBar::menuX(const std::size_t index) const {
        float x = 8.0f;
        for (std::size_t i = 0; i < index; ++i) {
            x += this->menuWidth(i);
        }
        return x;
    }

    float FMenuBar::menuWidth(const std::size_t index) const {
        const Vector2 m = MainFont::instance().measure(this->menus_[index].title, MENU_FONT);
        return m.x + 2.0f * MENU_PAD_X;
    }

    float FMenuBar::dropdownWidth(const std::size_t index) const {
        float w = 200.0f;
        for (const MenuItem& item : this->menus_[index].items) {
            if (!item.label) {
                continue;
            }
            const float label = MainFont::instance().measure(item.label, MENU_FONT).x;
            const float shortcut = MainFont::instance().measure(item.shortcut, MENU_FONT).x;
            w = std::max(w, 28.0f + label + 48.0f + shortcut + 16.0f);
        }
        return w;
    }

    Rectangle FMenuBar::dropdownRect(const std::size_t index) const {
        float h = 2.0f * DROP_PAD;
        for (const MenuItem& item : this->menus_[index].items) {
            h += item.label ? ITEM_H : SEPARATOR_H;
        }
        return {this->menuX(index), HEIGHT, this->dropdownWidth(index), h};
    }

    Rectangle FMenuBar::toolRect(const std::size_t index) const {
        const float right = static_cast<float>(GetScreenWidth()) - 8.0f;
        const float x = right - static_cast<float>(this->tools_.size() - index) * TOOL_W;
        return {x, 0.0f, TOOL_W, HEIGHT};
    }

    Rectangle FMenuBar::navRect(const std::size_t index) const {
        const float x = this->menuX(this->menus_.size()) + 14.0f + static_cast<float>(index) * TOOL_W;
        return {x, 0.0f, TOOL_W, HEIGHT};
    }

    Rectangle FMenuBar::searchRect() const {
        const float screenW = static_cast<float>(GetScreenWidth());
        const float left = this->navRect(this->nav_.size()).x + 14.0f;
        const float right = this->toolRect(0).x - 10.0f;
        const float free = std::max(0.0f, right - left);
        // максимум — треть ширины окна; если места меньше, занимает всё свободное
        const float w = std::min(free, std::max(screenW / 3.0f, 160.0f));
        // поле стоит по центру свободного пространства, как в vscode
        const float x = left + (free - w) * 0.5f;
        return {x, (HEIGHT - 24.0f) * 0.5f, w, 24.0f};
    }

    Rectangle FMenuBar::quickOpenRect() const {
        const Rectangle search = this->searchRect();
        const float rows = std::max<float>(static_cast<float>(this->results_.size()), 1.0f);
        return {search.x, HEIGHT + 4.0f, search.width, rows * RESULT_H + 2.0f * DROP_PAD};
    }

    // --- быстрый поиск ---------------------------------------------------

    void FMenuBar::openQuickOpen() {
        this->quickOpen_ = true;
        this->openMenu_ = -1;
        this->query_.clear();
        this->selectedResult_ = 0;
        this->hoverResult_ = -1;
        this->index_.clear(); // пересканируем проект при следующем вводе
        this->updateResults(); // пустой запрос — показываем недавние
        this->grabKeyboard(); // печатаемый ввод забирает quick open, а не редактор
    }

    void FMenuBar::closeQuickOpen() {
        this->quickOpen_ = false;
        this->query_.clear();
        this->results_.clear();
        this->hoverResult_ = -1;
        if (this->ownsKeyboard()) {
            // не трогаем фокус, если acceptResult уже отдал его редактору
            this->releaseKeyboard();
        }
    }

    void FMenuBar::updateResults() {
        // влезаем в экран, но не больше 20 строк
        const float avail = static_cast<float>(GetScreenHeight()) - HEIGHT - 4.0f - 2.0f * DROP_PAD - 10.0f;
        const int maxRows = std::clamp(static_cast<int>(avail / RESULT_H), 1, 20);

        this->results_.clear();
        if (this->query_.empty()) {
            // пустой запрос — недавние файлы из дерева
            if (this->tree_) {
                for (const std::string& p : this->tree_->recents()) {
                    if (static_cast<int>(this->results_.size()) >= maxRows) {
                        break;
                    }
                    this->results_.push_back(p);
                }
            }
        } else {
            if (this->index_.empty() && this->tree_) {
                this->index_ = FFileTree::scanFiles(this->tree_->rootPath());
            }
            // сначала совпадения по имени файла, затем по пути
            for (const std::string& p : this->index_) {
                if (static_cast<int>(this->results_.size()) >= maxRows) {
                    break;
                }
                if (containsCI(std::filesystem::path(p).filename().string(), this->query_)) {
                    this->results_.push_back(p);
                }
            }
            for (const std::string& p : this->index_) {
                if (static_cast<int>(this->results_.size()) >= maxRows) {
                    break;
                }
                if (containsCI(p, this->query_) &&
                    std::find(this->results_.begin(), this->results_.end(), p) == this->results_.end()) {
                    this->results_.push_back(p);
                }
            }
        }
        this->selectedResult_ = 0;
    }

    void FMenuBar::acceptResult() {
        if (this->selectedResult_ < 0 || this->selectedResult_ >= static_cast<int>(this->results_.size())) {
            return;
        }
        if (this->tree_) {
            this->tree_->reveal(this->results_[static_cast<std::size_t>(this->selectedResult_)]);
        }
        this->closeQuickOpen();
    }

    // --- взаимодействие --------------------------------------------------

    bool FMenuBar::hitTest(const float mx, const float my) const {
        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);
        const float lx = mx - ax;
        const float ly = my - ay;
        if (this->quickOpen_) {
            return true; // открытый поиск съедает клики по всему окну
        }
        if (ly >= 0.0f && ly <= HEIGHT) {
            return true; // вся полоса
        }
        if (this->openMenu_ >= 0) {
            return CheckCollisionPointRec({lx, ly}, this->dropdownRect(static_cast<std::size_t>(this->openMenu_)));
        }
        return false;
    }

    void FMenuBar::onUpdate(const float dt) {
        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);
        const Vector2 mouse = GetMousePosition();
        const float mx = mouse.x - ax;
        const float my = mouse.y - ay;
        const bool inBar = my >= 0.0f && my <= HEIGHT;

        // --- быстрый поиск -------------------------------------------------
        if (this->quickOpen_) {
            // ввод текста
            int ch = GetCharPressed();
            bool changed = false;
            while (ch > 0) {
                if (ch >= 32) {
                    appendUtf8(this->query_, ch);
                    changed = true;
                }
                ch = GetCharPressed();
            }
            // backspace: один сразу, при удержании — автоповтор
            if (IsKeyPressed(KEY_BACKSPACE)) {
                popUtf8(this->query_);
                changed = true;
                this->backspaceTimer_ = 0.35f;
            } else if (IsKeyDown(KEY_BACKSPACE)) {
                this->backspaceTimer_ -= dt;
                if (this->backspaceTimer_ <= 0.0f) {
                    popUtf8(this->query_);
                    changed = true;
                    this->backspaceTimer_ = 0.03f;
                }
            } else {
                this->backspaceTimer_ = 0.0f;
            }
            if (changed) {
                this->updateResults();
            }

            if (IsKeyPressed(KEY_DOWN)) {
                this->selectedResult_ = std::min(this->selectedResult_ + 1, static_cast<int>(this->results_.size()) - 1);
            }
            if (IsKeyPressed(KEY_UP)) {
                this->selectedResult_ = std::max(this->selectedResult_ - 1, 0);
            }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                this->acceptResult();
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                this->closeQuickOpen();
            }

            // строка результата под курсором
            this->hoverResult_ = -1;
            if (!this->results_.empty()) {
                const Rectangle panel = this->quickOpenRect();
                if (CheckCollisionPointRec({mx, my}, panel)) {
                    const int row = static_cast<int>((my - panel.y - DROP_PAD) / RESULT_H);
                    if (row >= 0 && row < static_cast<int>(this->results_.size())) {
                        this->hoverResult_ = row;
                    }
                }
            }
            this->hoverSearch_ = CheckCollisionPointRec({mx, my}, this->searchRect());
            // меню закрыто, чужие подсветки гасим
            this->openMenu_ = -1;
            this->hoverMenu_ = this->hoverItem_ = this->hoverTool_ = this->hoverNav_ = -1;
            return;
        }

        // Ctrl+P открывает быстрый поиск, как в vscode
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_P)) {
            this->openQuickOpen();
            return;
        }

        // пункт меню под курсором
        this->hoverMenu_ = -1;
        if (inBar) {
            for (std::size_t i = 0; i < this->menus_.size(); ++i) {
                const float x = this->menuX(i);
                if (mx >= x && mx <= x + this->menuWidth(i)) {
                    this->hoverMenu_ = static_cast<int>(i);
                    break;
                }
            }
        }

        // строка выпадающего меню под курсором
        this->hoverItem_ = -1;
        if (this->openMenu_ >= 0) {
            const Rectangle dr = this->dropdownRect(static_cast<std::size_t>(this->openMenu_));
            if (CheckCollisionPointRec({mx, my}, dr)) {
                float y = dr.y + DROP_PAD;
                for (std::size_t i = 0; i < this->menus_[static_cast<std::size_t>(this->openMenu_)].items.size(); ++i) {
                    const MenuItem& item = this->menus_[static_cast<std::size_t>(this->openMenu_)].items[i];
                    const float h = item.label ? ITEM_H : SEPARATOR_H;
                    if (my >= y && my <= y + h && item.label) {
                        this->hoverItem_ = static_cast<int>(i);
                    }
                    y += h;
                }
            }

            // при открытом меню наведение на соседний пункт переключает его
            if (this->hoverMenu_ >= 0 && this->hoverMenu_ != this->openMenu_) {
                this->openMenu_ = this->hoverMenu_;
                this->hoverItem_ = -1;
            }

            // клик мимо полосы и мимо списка закрывает меню
            if (!inBar && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                !CheckCollisionPointRec({mx, my}, this->dropdownRect(static_cast<std::size_t>(this->openMenu_)))) {
                this->openMenu_ = -1;
            }
        }

        if (IsKeyPressed(KEY_ESCAPE)) {
            this->openMenu_ = -1;
        }

        // иконки-кнопки под курсором
        this->hoverTool_ = -1;
        for (std::size_t i = 0; i < this->tools_.size(); ++i) {
            if (CheckCollisionPointRec({mx, my}, this->toolRect(i))) {
                this->hoverTool_ = static_cast<int>(i);
            }
        }
        this->hoverNav_ = -1;
        for (std::size_t i = 0; i < this->nav_.size(); ++i) {
            if (CheckCollisionPointRec({mx, my}, this->navRect(i))) {
                this->hoverNav_ = static_cast<int>(i);
            }
        }
        this->hoverSearch_ = CheckCollisionPointRec({mx, my}, this->searchRect());
    }

    void FMenuBar::onMousePress() {
        if (this->quickOpen_) {
            if (this->hoverResult_ >= 0) {
                this->selectedResult_ = this->hoverResult_;
                this->acceptResult();
            } else if (!this->hoverSearch_) {
                // клик мимо поля и результатов просто закрывает поиск
                this->closeQuickOpen();
            }
            return;
        }
        if (this->hoverSearch_) {
            this->openQuickOpen(); // клик по полю поиска открывает quick open
            return;
        }
        if (this->hoverMenu_ >= 0) {
            // клик по пункту меню: открыть или закрыть, если уже открыт
            this->openMenu_ = (this->openMenu_ == this->hoverMenu_) ? -1 : this->hoverMenu_;
            return;
        }
        if (this->hoverNav_ >= 0) {
            if (this->nav_[static_cast<std::size_t>(this->hoverNav_)].onClick) {
                this->nav_[static_cast<std::size_t>(this->hoverNav_)].onClick();
            }
            return;
        }
        if (this->hoverTool_ >= 0) {
            ToolButton& tool = this->tools_[static_cast<std::size_t>(this->hoverTool_)];
            if (tool.isToggle) {
                tool.active = !tool.active;
            }
            if (tool.onClick) {
                tool.onClick();
            }
            return;
        }
        if (this->openMenu_ >= 0 && this->hoverItem_ >= 0) {
            MenuItem& item = this->menus_[static_cast<std::size_t>(this->openMenu_)].items[static_cast<std::size_t>(this->hoverItem_)];
            if (item.action) {
                item.action();
            }
            this->openMenu_ = -1;
        }
    }

    // --- отрисовка -------------------------------------------------------

    void FMenuBar::onDraw() {
        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);

        const Theme& theme = Theme::instance();
        MainFont& font = MainFont::instance();
        IconFont& icons = IconFont::instance();

        // фон полосы на всю ширину окна
        DrawRectangleRec({ax, ay, static_cast<float>(GetScreenWidth()), HEIGHT}, theme.titleBarBg);

        // пункты меню
        for (std::size_t i = 0; i < this->menus_.size(); ++i) {
            const float x = ax + this->menuX(i);
            const float w = this->menuWidth(i);
            if (static_cast<int>(i) == this->openMenu_) {
                DrawRectangleRec({x, ay, w, HEIGHT}, theme.pressOverlay);
            } else if (static_cast<int>(i) == this->hoverMenu_) {
                DrawRectangleRec({x, ay, w, HEIGHT}, theme.hoverOverlay);
            }
            font.draw(this->menus_[i].title, x + MENU_PAD_X, ay + (HEIGHT - MENU_FONT) * 0.5f, MENU_FONT, theme.text);
        }

        // стрелки навигации
        for (std::size_t i = 0; i < this->nav_.size(); ++i) {
            const Rectangle r = this->navRect(i);
            if (static_cast<int>(i) == this->hoverNav_) {
                DrawRectangleRec({ax + r.x, ay, r.width, HEIGHT}, theme.hoverOverlay);
            }
            icons.drawCentered(this->nav_[i].icon, ax + r.x, ay, r.width, HEIGHT, ICON, theme.iconIdle);
        }

        // поле поиска: имя проекта или ввод быстрого поиска
        const Rectangle search = this->searchRect();
        if (search.width > 60.0f) {
            const Rectangle field{ax + search.x, ay + search.y, search.width, search.height};
            DrawRectangleRounded(field, roundnessFor(6.0f, field), 16, theme.searchBg);
            if (this->quickOpen_) {
                DrawRectangleRoundedLinesEx(field, roundnessFor(6.0f, field), 16, 1.0f, theme.accent);
            }

            const float textY = ay + search.y + (search.height - MENU_FONT) * 0.5f;
            if (!this->quickOpen_) {
                // закрытый поиск показывает имя воркспейса
                font.drawEllipsized(this->workspace_, ax + search.x + 10.0f, textY,
                                    search.width - 44.0f, MENU_FONT, theme.text);
            } else if (this->query_.empty()) {
                font.drawEllipsized("Search files by name...", ax + search.x + 10.0f, textY,
                                    search.width - 44.0f, MENU_FONT, theme.textDim);
            } else {
                font.drawEllipsized(this->query_, ax + search.x + 10.0f, textY,
                                    search.width - 44.0f, MENU_FONT, theme.text);
                // мигающий курсор в конце ввода
                if (std::fmod(GetTime(), 1.2) < 0.7) {
                    const float caretX = ax + search.x + 10.0f + font.measure(this->query_, MENU_FONT).x + 1.0f;
                    DrawLineEx({caretX, ay + search.y + 4.0f},
                               {caretX, ay + search.y + search.height - 4.0f}, 1.5f, theme.text);
                }
            }
            icons.drawCentered(ICON_LC_FOLDER, ax + search.x + search.width - 24.0f, ay, 24.0f, HEIGHT, 14.0f, theme.iconIdle);
        }

        // правые иконки: customize + layout-тогглы
        for (std::size_t i = 0; i < this->tools_.size(); ++i) {
            const ToolButton& tool = this->tools_[i];
            const Rectangle r = this->toolRect(i);

            // primary-тоггл подсвечен состоянием реального сайдбара
            bool active = tool.primary && this->sidebarVisible_ ? this->sidebarVisible_() : tool.active;

            if (active) {
                const Color glow{theme.accent.r, theme.accent.g, theme.accent.b, 130};
                DrawRectangleRec({ax + r.x, ay, r.width, HEIGHT}, glow);
            } else if (static_cast<int>(i) == this->hoverTool_) {
                DrawRectangleRec({ax + r.x, ay, r.width, HEIGHT}, theme.hoverOverlay);
            }
            icons.drawCentered(tool.icon, ax + r.x, ay, r.width, HEIGHT, ICON, active ? theme.iconActive : theme.iconIdle);
        }

        // выпадающее меню поверх всего
        if (this->openMenu_ >= 0) {
            const std::size_t mi = static_cast<std::size_t>(this->openMenu_);
            const Rectangle dr = this->dropdownRect(mi);
            const Rectangle absDr{ax + dr.x, ay + dr.y, dr.width, dr.height};

            DrawRectangleRounded(absDr, roundnessFor(6.0f, absDr), 16, theme.dropdownBg);
            DrawRectangleRoundedLinesEx(absDr, roundnessFor(6.0f, absDr), 16, 1.0f, theme.dropdownBorder);

            float y = absDr.y + DROP_PAD;
            for (std::size_t i = 0; i < this->menus_[mi].items.size(); ++i) {
                const MenuItem& item = this->menus_[mi].items[i];
                if (!item.label) {
                    // разделитель
                    DrawLineEx({absDr.x + 10.0f, y + SEPARATOR_H * 0.5f},
                               {absDr.x + absDr.width - 10.0f, y + SEPARATOR_H * 0.5f}, 1.0f, theme.dropdownBorder);
                    y += SEPARATOR_H;
                    continue;
                }

                if (static_cast<int>(i) == this->hoverItem_) {
                    const Rectangle row{absDr.x + 2.0f, y, absDr.width - 4.0f, ITEM_H};
                    DrawRectangleRounded(row, roundnessFor(4.0f, row), 8, theme.menuHoverBg);
                }

                // галочка: динамическая из probe или статическая из item.checked
                const bool checked = item.checkedProbe ? item.checkedProbe() : item.checked;
                if (checked) {
                    icons.draw(ICON_LC_CHECK, absDr.x + 9.0f, y + (ITEM_H - 13.0f) * 0.5f, 13.0f, theme.text);
                }

                font.draw(item.label, absDr.x + 28.0f, y + (ITEM_H - MENU_FONT) * 0.5f, MENU_FONT, theme.text);
                if (item.shortcut && item.shortcut[0] != '\0') {
                    const float sw = font.measure(item.shortcut, MENU_FONT).x;
                    font.draw(item.shortcut, absDr.x + absDr.width - 12.0f - sw,
                              y + (ITEM_H - MENU_FONT) * 0.5f, MENU_FONT, theme.textDim);
                }
                y += ITEM_H;
            }
        }

        // панель результатов быстрого поиска поверх всего
        if (this->quickOpen_) {
            const Rectangle q = this->quickOpenRect();
            const Rectangle panel{ax + q.x, ay + q.y, q.width, q.height};

            DrawRectangleRounded(panel, roundnessFor(8.0f, panel), 16, theme.dropdownBg);
            DrawRectangleRoundedLinesEx(panel, roundnessFor(8.0f, panel), 16, 1.0f, theme.dropdownBorder);

            float y = panel.y + DROP_PAD;
            if (this->results_.empty()) {
                // подсказка или отсутствие результатов
                const char* hint = this->query_.empty() ? "Type a file name to search" : "No matching results";
                font.draw(hint, panel.x + 12.0f, y + (RESULT_H - MENU_FONT) * 0.5f, MENU_FONT, theme.textDim);
            } else {
                for (std::size_t i = 0; i < this->results_.size(); ++i) {
                    const bool sel = static_cast<int>(i) == this->selectedResult_;
                    const Rectangle row{panel.x + 4.0f, y, panel.width - 8.0f, RESULT_H};
                    if (sel) {
                        DrawRectangleRounded(row, roundnessFor(4.0f, row), 8, theme.rowSelectedBg);
                    } else if (static_cast<int>(i) == this->hoverResult_) {
                        DrawRectangleRounded(row, roundnessFor(4.0f, row), 8, theme.rowHoverBg);
                    }

                    const std::filesystem::path p(this->results_[i]);
                    const std::string name = p.filename().string();
                    const bool firstRecent = this->query_.empty() && i == 0;

                    icons.draw(FFileTree::fileIcon(name), panel.x + 12.0f,
                               y + (RESULT_H - ICON) * 0.5f, ICON, theme.textDim);

                    const float nameX = panel.x + 12.0f + ICON + 8.0f;
                    const float nameW = font.measure(name, MENU_FONT).x;
                    font.draw(name, nameX, y + (RESULT_H - MENU_FONT) * 0.5f, MENU_FONT,
                              sel ? theme.iconActive : theme.text);

                    // каталог относительно корня проекта, приглушённо;
                    // на первой строке недавних вместо него метка секции
                    if (firstRecent) {
                        const char* tag = "recently opened";
                        const float tagW = font.measure(tag, 12.0f).x;
                        font.draw(tag, panel.x + panel.width - 12.0f - tagW,
                                  y + (RESULT_H - 12.0f) * 0.5f, 12.0f, theme.textDim);
                    } else {
                        std::error_code dirc;
                        const std::filesystem::path rel =
                            std::filesystem::relative(p, this->projectRoot_, dirc);
                        std::string dir = dirc ? "" : rel.parent_path().string();
                        if (!dir.empty()) {
                            const float dirX = nameX + nameW + 10.0f;
                            const float maxW = panel.x + panel.width - dirX - 12.0f;
                            font.drawEllipsized(dir, dirX, y + (RESULT_H - 12.0f) * 0.5f, maxW, 12.0f, theme.textDim);
                        }
                    }
                    y += RESULT_H;
                }
            }
        }
    }
}
