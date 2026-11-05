#pragma once

#include "Component.hpp"
#include "raylib.h"
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace Renderer {

    // редактор кода — нативная замена monaco для flatcode:
    //  - моноширинный рендер с подсветкой синтаксиса (c/c++ и json)
    //  - номера строк, подсветка текущей строки, выделение мышью и клавиатурой
    //  - виртуализация: на кадр рисуются только видимые строки, поэтому
    //    большие файлы не просаживают fps
    //  - undo/redo (снимки текста), буфер обмена, сохранение по ctrl+s
    //  - без автопереноса: длинная строка уезжает за горизонтальный скролл,
    //    каретка автопрокручивается к ней
    //
    // занимает всю площадь окна справа от сайдбара: сам следит за шириной
    // сайдбара (leftInset-пров) и рисует таб-бар с открытым файлом
    class FEditor : public Component {
    public:
        FEditor();

        // сколько пикселей занимает всё слева (сайдбар); редактор ставится правее
        void setLeftInset(std::function<float()> probe) { leftInset_ = std::move(probe); }

        // открыть файл с диска; повторное открытие того же пути просто фокусирует
        void openFile(const std::string& path);
        void save();
        [[nodiscard]] const std::string& currentPath() const { return path_; }
        [[nodiscard]] bool hasDocument() const { return !path_.empty(); }

        bool hitTest(float mx, float my) const override;

    protected:
        void onUpdate(float dt) override;
        void onDraw() override;
        void onMousePress() override;
        void onMouseRelease() override;

    private:
        // позиция в документе: строка + колонка (колонка в символах, не байтах —
        // табы при загрузке раскрываются в пробелы, так что смещение = advance * col)
        struct Pos {
            int line = 0;
            int col = 0;
            bool operator==(const Pos& o) const { return line == o.line && col == o.col; }
            bool operator!=(const Pos& o) const { return !(*this == o); }
        };

        // --- модель ---------------------------------------------------------
        enum class Lang {
            Cpp,   // c, c++ и заголовки
            Json,
            Plain, // всё остальное — без подсветки
        };

        bool loadFromDisk(const std::string& path);
        void closeDocument();
        void setLines(std::vector<std::string> lines);

        // снимки для undo/redo: перед мутацией pushUndo, подряд идущие однотипные
        // правки (печать символа за символом) склеиваются в один шаг
        struct Snapshot {
            std::vector<std::string> lines;
            Pos cursor;
            Pos anchor;
        };
        void pushUndo(int kind);
        void doUndo();
        void doRedo();

        void insertText(const std::string& text); // в каретку, заменяя выделение
        void insertAtCursor(const std::string& text); // без снятия выделения и undo
        void eraseSelection();          // с проверкой, что выделение есть
        void eraseSelectionInternal();  // сам стиратель (undo берёт на себя вызывающий)
        void deleteChar(bool forward);
        void markEdited(); // dirty + пересчёт ширины + прокрутка к каретке

        void moveCursor(Pos p, bool select);
        void moveCursorHoriz(int delta, bool select);   // с прыжком между строками
        void moveCursorVert(int delta, bool select);
        void moveWord(bool forward, bool select);
        void selectWordAt(const Pos& p);
        void selectLineAt(int line);
        [[nodiscard]] Pos selectionStart() const;
        [[nodiscard]] Pos selectionEnd() const;
        [[nodiscard]] bool hasSelection() const { return anchor_ != cursor_; }
        [[nodiscard]] std::string selectedTextOrLine() const;
        [[nodiscard]] std::string lineIndent(int line) const;
        void ensureCursorVisible();
        [[nodiscard]] float maxScrollY() const;
        [[nodiscard]] float maxScrollX() const;
        [[nodiscard]] float textViewWidth() const; // зона кода без гаттера и миникарты

        // --- ввод -------------------------------------------------------------
        void pollKeys(float dt);
        [[nodiscard]] Pos posFromMouse(float mx, float my) const;
        [[nodiscard]] bool repeatKey(int key, float dt); // нажатие с автоповтором при удержании

        // --- отрисовка ---------------------------------------------------------
        [[nodiscard]] float lineHeight() const { return FONT_SIZE * 1.35f; }
        [[nodiscard]] float advance() const;
        [[nodiscard]] float gutterWidth() const;
        [[nodiscard]] Rectangle contentRect() const;  // код под таб-баром, в локальных координатах
        [[nodiscard]] Rectangle tabRect() const;      // вкладка открытого файла
        [[nodiscard]] Rectangle tabCloseRect() const; // крестик на вкладке

        void drawTabBar() const;
        void drawEmptyState() const;
        void drawCode() const;

        struct Token {
            int col; // смещение в символах
            int len;
            Color color;
        };
        void tokenizeLine(const std::string& line, int& state, std::vector<Token>& out) const;
        void tokenizeCpp(const std::string& line, int& state, std::vector<Token>& out) const;
        void tokenizeJson(const std::string& line, int& state, std::vector<Token>& out) const;

        // --- миникарта (как в monaco/vscode) --------------------------------
        // рисуется напрямую каждый кадр (только строки, попавшие в её окно —
        // их около сотни), без кеш-текстуры: всегда синхронна с документом
        [[nodiscard]] Rectangle minimapRect() const;
        [[nodiscard]] float minimapScroll() const; // смещение окна миникарты по документу
        void drawMinimap() const;
        void handleMinimapPress(float relY); // клик/захват слайдера
        bool minimapDrag_ = false;
        float minimapGrabOffset_ = 0.0f;

        std::vector<std::string> lines_{""}; // документ; минимум одна строка
        std::string path_;  // "" — документ не открыт
        std::string name_;  // имя файла для вкладки
        Lang lang_ = Lang::Plain;
        bool hadTrailingNewline_ = false;
        bool dirty_ = false;

        Pos cursor_;
        Pos anchor_; // якорь выделения (== cursor_, когда выделения нет)
        int desiredCol_ = -1; // запомненная колонка при вертикальных прыжках каретки
        bool dragging_ = false;
        int clickCount_ = 0; // 1 = каретка, 2 = слово, 3 = строка
        double lastClickTime_ = -10.0;
        Pos lastClickPos_;

        float scrollY_ = 0.0f; // в пикселях
        float scrollX_ = 0.0f;
        mutable bool widthDirty_ = true;   // пересчитать ширину самой длинной строки
        mutable float maxWidthPx_ = 0.0f;  // кеш той самой ширины
        mutable bool statesDirty_ = true;  // пересобрать состояние строк (блочные комментарии)
        mutable std::vector<int> lineStates_; // state подсветки ПЕРЕД строкой i

        std::deque<Snapshot> undo_;
        std::deque<Snapshot> redo_;
        double lastEditTime_ = -10.0;
        int lastEditKind_ = -1;
        float repeatTimer_ = 0.0f;
        int repeatKey_ = -1;

        float width_ = 600.0f;
        float height_ = 400.0f;
        bool mouseInside_ = false; // для возврата курсора-стрелки, когда мышь ушла
        std::function<float()> leftInset_;

        static constexpr float FONT_SIZE = 14.0f;
        static constexpr float TAB_BAR_H = 35.0f;
        static constexpr float TEXT_PAD_X = 10.0f;  // отступ кода после гаттера
        static constexpr float EDITOR_PAD_Y = 4.0f;
        static constexpr std::size_t UNDO_LIMIT = 100; // размер истории

        // миникарта
        static constexpr float MINIMAP_W = 110.0f;
        static constexpr float MINIMAP_GAP = 12.0f; // до скроллбара у правого края
        static constexpr float MINI_LINE_H = 4.0f;  // высота строки миникарты
        static constexpr float MINI_FONT = 4.0f;
    };
}
