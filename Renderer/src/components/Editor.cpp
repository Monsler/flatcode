#include "components/Editor.hpp"
#include "EditorFont.hpp"
#include "IconFont.hpp"
#include "IconsLucide.h"
#include "Logger.hpp"
#include "MainFont.hpp"
#include "Theme.hpp"
#include "components/FileTree.hpp"
#include "components/MenuBar.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_set>

namespace {

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

    // длина utf-8 последовательности по первому байту
    int cpLen(const std::string& s, const std::size_t at) {
        if (at >= s.size()) {
            return 1;
        }
        const unsigned char b = static_cast<unsigned char>(s[at]);
        if ((b & 0x80) == 0) {
            return 1;
        }
        if ((b & 0xE0) == 0xC0) {
            return 2;
        }
        if ((b & 0xF0) == 0xE0) {
            return 3;
        }
        return 4;
    }

    // колонки каретки считаются в символах (codepoints), а не в байтах —
    // иначе кириллица в комментариях сбивает геометрию каретки
    int colCount(const std::string& s) {
        int count = 0;
        std::size_t i = 0;
        while (i < s.size()) {
            i += static_cast<std::size_t>(cpLen(s, i));
            ++count;
        }
        return count;
    }

    // байтовое смещение для колонки в символах
    std::size_t byteOffset(const std::string& s, const int col) {
        std::size_t i = 0;
        int c = 0;
        while (i < s.size() && c < col) {
            i += static_cast<std::size_t>(cpLen(s, i));
            ++c;
        }
        return std::min(i, s.size());
    }

    // --- словари подсветки c/c++ (в духе vscode dark+) ---------------------

    const std::unordered_set<std::string>& cppKeywords() {
        static const std::unordered_set<std::string> set = {
            "alignas", "alignof", "and", "and_eq", "asm", "bitand", "bitor", "class",
            "compl", "concept", "consteval", "constexpr", "constinit", "const_cast",
            "co_await", "co_return", "co_yield", "decltype", "delete", "dynamic_cast",
            "explicit", "export", "friend", "inline", "mutable", "namespace", "new",
            "noexcept", "not", "not_eq", "operator", "or", "or_eq", "private",
            "protected", "public", "register", "reinterpret_cast", "requires",
            "sizeof", "static", "static_assert", "static_cast", "struct", "template",
            "this", "thread_local", "typedef", "typeid", "typename", "union", "using",
            "virtual", "volatile", "xor", "xor_eq",
        };
        return set;
    }

    const std::unordered_set<std::string>& cppControlKeywords() {
        static const std::unordered_set<std::string> set = {
            "break", "case", "catch", "continue", "default", "do", "else", "for",
            "goto", "if", "return", "switch", "throw", "try", "while",
        };
        return set;
    }

    const std::unordered_set<std::string>& cppTypes() {
        static const std::unordered_set<std::string> set = {
            "auto", "bool", "char", "char8_t", "char16_t", "char32_t", "double",
            "float", "int", "long", "short", "signed", "unsigned", "void", "wchar_t",
            "std", "size_t", "ssize_t", "ptrdiff_t", "int8_t", "int16_t", "int32_t",
            "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        };
        return set;
    }

    const std::unordered_set<std::string>& cppLiterals() {
        static const std::unordered_set<std::string> set = {
            "true", "false", "nullptr", "NULL",
        };
        return set;
    }
}

namespace Renderer {

    FEditor::FEditor() = default;

    // --- документ -----------------------------------------------------------

    bool FEditor::loadFromDisk(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            Base::Logger::instance().log(Base::LogLevel::ERR, "Editor: cannot open " + path);
            return false;
        }
        const std::string data{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        if (data.size() > 5 * 1024 * 1024) {
            Base::Logger::instance().log(Base::LogLevel::WARN, "Editor: file too large, refused: " + path);
            return false;
        }
        if (data.find('\0') != std::string::npos) {
            Base::Logger::instance().log(Base::LogLevel::WARN, "Editor: binary file, refused: " + path);
            return false;
        }

        // разбор по строкам, \r убираем, табы раскрываем в пробелы:
        // колонка каретки = символы * advance, геометрия не должна плавать
        std::vector<std::string> parsed;
        std::size_t start = 0;
        while (true) {
            const std::size_t nl = data.find('\n', start);
            std::string line = data.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.find('\t') != std::string::npos) {
                std::string expanded;
                expanded.reserve(line.size() + 16);
                for (const char c : line) {
                    if (c == '\t') {
                        expanded += "    ";
                    } else {
                        expanded.push_back(c);
                    }
                }
                line = std::move(expanded);
            }
            parsed.push_back(std::move(line));
            if (nl == std::string::npos) {
                break;
            }
            start = nl + 1;
        }

        this->hadTrailingNewline_ = !parsed.empty() && parsed.back().empty();
        if (this->hadTrailingNewline_) {
            parsed.pop_back();
        }
        if (parsed.empty()) {
            parsed.emplace_back("");
        }

        // язык по расширению
        std::string ext = std::filesystem::path(path).extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        const bool isCpp = ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" ||
                           ext == ".h" || ext == ".hh" || ext == ".hpp" || ext == ".hxx" ||
                           ext == ".inl";
        this->lang_ = isCpp ? Lang::Cpp : (ext == ".json" ? Lang::Json : Lang::Plain);

        this->setLines(std::move(parsed));
        this->path_ = path;
        this->name_ = std::filesystem::path(path).filename().string();
        this->dirty_ = false;

        Base::Logger::instance().log(Base::LogLevel::INFO,
                                     "Editor: opened " + path + " (" + std::to_string(this->lines_.size()) + " lines)");
        return true;
    }

    void FEditor::setLines(std::vector<std::string> lines) {
        this->lines_ = std::move(lines);
        this->cursor_ = this->anchor_ = Pos{};
        this->desiredCol_ = -1;
        this->undo_.clear();
        this->redo_.clear();
        this->lastEditKind_ = -1;
        this->scrollY_ = this->scrollX_ = 0.0f;
        this->widthDirty_ = true;
        this->statesDirty_ = true;
    }

    void FEditor::openFile(const std::string& path) {
        if (path == this->path_) {
            this->grabKeyboard(); // уже открыт — просто фокус
            return;
        }
        if (this->loadFromDisk(path)) {
            this->grabKeyboard();
        }
    }

    void FEditor::closeDocument() {
        this->path_.clear();
        this->name_.clear();
        this->hadTrailingNewline_ = false;
        this->lang_ = Lang::Plain;
        this->dirty_ = false;
        this->setLines({""});
        if (this->ownsKeyboard()) {
            this->releaseKeyboard();
        }
    }

    void FEditor::save() {
        if (this->path_.empty()) {
            return;
        }
        std::ofstream file(this->path_, std::ios::binary | std::ios::trunc);
        if (!file) {
            Base::Logger::instance().log(Base::LogLevel::ERR, "Editor: cannot write " + this->path_);
            return;
        }
        for (std::size_t i = 0; i < this->lines_.size(); ++i) {
            file << this->lines_[i];
            const bool last = i + 1 == this->lines_.size();
            if (!last || this->hadTrailingNewline_) {
                file << '\n';
            }
        }
        this->dirty_ = false;
        Base::Logger::instance().log(Base::LogLevel::INFO, "Editor: saved " + this->path_);
    }

    // --- история правок -------------------------------------------------------

    void FEditor::pushUndo(const int kind) {
        const double now = GetTime();
        // однотипные правки подряд (печать по символу, удержание backspace)
        // склеиваются в один шаг истории
        const bool coalesce = kind == this->lastEditKind_ &&
                              now - this->lastEditTime_ < 0.8 &&
                              this->redo_.empty();
        if (!coalesce) {
            this->undo_.push_back({this->lines_, this->cursor_, this->anchor_});
            if (this->undo_.size() > UNDO_LIMIT) {
                this->undo_.pop_front();
            }
            this->redo_.clear();
        }
        this->lastEditKind_ = kind;
        this->lastEditTime_ = now;
    }

    void FEditor::doUndo() {
        if (this->undo_.empty()) {
            return;
        }
        this->redo_.push_back({this->lines_, this->cursor_, this->anchor_});
        const Snapshot& snap = this->undo_.back();
        this->lines_ = snap.lines;
        this->cursor_ = snap.cursor;
        this->anchor_ = snap.anchor;
        this->undo_.pop_back();
        this->lastEditKind_ = -1;
        this->desiredCol_ = -1;
        this->dirty_ = true;
        this->widthDirty_ = true;
        this->statesDirty_ = true;
        this->ensureCursorVisible();
    }

    void FEditor::doRedo() {
        if (this->redo_.empty()) {
            return;
        }
        this->undo_.push_back({this->lines_, this->cursor_, this->anchor_});
        const Snapshot& snap = this->redo_.back();
        this->lines_ = snap.lines;
        this->cursor_ = snap.cursor;
        this->anchor_ = snap.anchor;
        this->redo_.pop_back();
        this->lastEditKind_ = -1;
        this->desiredCol_ = -1;
        this->dirty_ = true;
        this->widthDirty_ = true;
        this->statesDirty_ = true;
        this->ensureCursorVisible();
    }

    // --- правка ----------------------------------------------------------------

    void FEditor::insertText(const std::string& text) {
        if (text.empty()) {
            return;
        }
        this->pushUndo(0);
        this->eraseSelectionInternal();

        // \r выкидываем, таб раскрываем в 4 пробела
        std::string clean;
        clean.reserve(text.size() + 16);
        for (const char c : text) {
            if (c == '\r') {
                continue;
            }
            if (c == '\t') {
                clean += "    ";
            } else {
                clean.push_back(c);
            }
        }
        this->insertAtCursor(clean);
        this->markEdited();
    }

    // вставка в каретку без снятия выделения и без undo — внутренний инструмент
    void FEditor::insertAtCursor(const std::string& text) {
        const Pos p = this->cursor_;

        const std::size_t nl = text.find('\n');
        if (nl == std::string::npos) {
            const std::size_t b = byteOffset(this->lines_[p.line], p.col);
            this->lines_[p.line].insert(b, text);
            this->cursor_ = {p.line, p.col + colCount(text)};
            this->anchor_ = this->cursor_;
            return;
        }

        // многострочная вставка: хвост текущей строки уезжает за последнюю вставленную
        const std::size_t b = byteOffset(this->lines_[p.line], p.col);
        std::string tail = this->lines_[p.line].substr(b);
        this->lines_[p.line].erase(b);
        this->lines_[p.line] += text.substr(0, nl);

        std::vector<std::string> rest;
        std::size_t start = nl + 1;
        while (true) {
            const std::size_t next = text.find('\n', start);
            rest.emplace_back(text.substr(start, next == std::string::npos ? std::string::npos : next - start));
            if (next == std::string::npos) {
                break;
            }
            start = next + 1;
        }
        this->lines_.insert(this->lines_.begin() + p.line + 1, rest.begin(), rest.end());
        this->lines_[p.line + static_cast<int>(rest.size())] += tail;
        this->cursor_ = {p.line + static_cast<int>(rest.size()), colCount(rest.back())};
        this->anchor_ = this->cursor_;
    }

    void FEditor::eraseSelection() {
        if (!this->hasSelection()) {
            return;
        }
        this->pushUndo(1);
        this->eraseSelectionInternal();
        this->markEdited();
    }

    void FEditor::eraseSelectionInternal() {
        const Pos s = this->selectionStart();
        const Pos e = this->selectionEnd();
        if (s == e) {
            return;
        }
        const std::size_t sb = byteOffset(this->lines_[s.line], s.col);
        if (s.line == e.line) {
            const std::size_t eb = byteOffset(this->lines_[e.line], e.col);
            this->lines_[s.line].erase(sb, eb - sb);
        } else {
            const std::size_t eb = byteOffset(this->lines_[e.line], e.col);
            std::string tail = this->lines_[e.line].substr(eb);
            this->lines_[s.line].erase(sb);
            this->lines_[s.line] += tail;
            this->lines_.erase(this->lines_.begin() + s.line + 1, this->lines_.begin() + e.line + 1);
        }
        this->cursor_ = s;
        this->anchor_ = s;
        if (this->lines_.empty()) {
            this->lines_.emplace_back("");
        }
    }

    void FEditor::deleteChar(const bool forward) {
        if (this->hasSelection()) {
            this->pushUndo(1);
            this->eraseSelectionInternal();
            this->markEdited();
            return;
        }
        this->pushUndo(1);
        const int line = this->cursor_.line;
        const int col = this->cursor_.col;
        const int len = colCount(this->lines_[line]);

        if (!forward) {
            if (col > 0) {
                const std::size_t b = byteOffset(this->lines_[line], col - 1);
                const std::size_t e = byteOffset(this->lines_[line], col);
                this->lines_[line].erase(b, e - b);
                this->cursor_.col = col - 1;
            } else if (line > 0) {
                // начало строки — слияние с предыдущей
                const int prevLen = colCount(this->lines_[line - 1]);
                this->lines_[line - 1] += this->lines_[line];
                this->lines_.erase(this->lines_.begin() + line);
                this->cursor_ = {line - 1, prevLen};
            }
        } else {
            if (col < len) {
                const std::size_t b = byteOffset(this->lines_[line], col);
                const std::size_t e = byteOffset(this->lines_[line], col + 1);
                this->lines_[line].erase(b, e - b);
            } else if (line + 1 < static_cast<int>(this->lines_.size())) {
                // конец строки — подтягиваем следующую
                this->lines_[line] += this->lines_[line + 1];
                this->lines_.erase(this->lines_.begin() + line + 1);
            }
        }
        this->anchor_ = this->cursor_;
        this->markEdited();
    }

    void FEditor::markEdited() {
        this->dirty_ = true;
        this->widthDirty_ = true;
        // число строк могло измениться — состояния подсветки пересобираем всегда
        this->statesDirty_ = true;
        this->ensureCursorVisible();
    }

    // --- каретка и выделение -----------------------------------------------------

    FEditor::Pos FEditor::selectionStart() const {
        if (this->cursor_.line < this->anchor_.line ||
            (this->cursor_.line == this->anchor_.line && this->cursor_.col < this->anchor_.col)) {
            return this->cursor_;
        }
        return this->anchor_;
    }

    FEditor::Pos FEditor::selectionEnd() const {
        if (this->cursor_.line < this->anchor_.line ||
            (this->cursor_.line == this->anchor_.line && this->cursor_.col < this->anchor_.col)) {
            return this->anchor_;
        }
        return this->cursor_;
    }

    void FEditor::moveCursor(const Pos p, const bool select) {
        this->desiredCol_ = -1;
        Pos q = p;
        q.line = std::clamp(q.line, 0, static_cast<int>(this->lines_.size()) - 1);
        q.col = std::clamp(q.col, 0, colCount(this->lines_[q.line]));
        this->cursor_ = q;
        if (!select) {
            this->anchor_ = q;
        }
    }

    void FEditor::moveCursorHoriz(const int delta, const bool select) {
        this->desiredCol_ = -1;
        // стрелка без shift сворачивает выделение, как в vscode
        if (!select && this->hasSelection()) {
            this->cursor_ = delta < 0 ? this->selectionStart() : this->selectionEnd();
            this->anchor_ = this->cursor_;
            return;
        }
        if (delta < 0) {
            if (this->cursor_.col > 0) {
                this->cursor_.col -= 1;
            } else if (this->cursor_.line > 0) {
                this->cursor_.line -= 1;
                this->cursor_.col = colCount(this->lines_[this->cursor_.line]);
            }
        } else {
            if (this->cursor_.col < colCount(this->lines_[this->cursor_.line])) {
                this->cursor_.col += 1;
            } else if (this->cursor_.line + 1 < static_cast<int>(this->lines_.size())) {
                this->cursor_.line += 1;
                this->cursor_.col = 0;
            }
        }
        if (!select) {
            this->anchor_ = this->cursor_;
        }
    }

    void FEditor::moveCursorVert(const int delta, const bool select) {
        const int lines = static_cast<int>(this->lines_.size());
        const int target = std::clamp(this->cursor_.line + delta, 0, lines - 1);
        if (this->desiredCol_ < 0) {
            this->desiredCol_ = this->cursor_.col;
        }
        if (target == this->cursor_.line) {
            // край документа: vscode заводит каретку в начало/конец строки
            this->cursor_.col = delta < 0 ? 0 : colCount(this->lines_[target]);
        } else {
            this->cursor_.line = target;
            this->cursor_.col = std::min(this->desiredCol_, colCount(this->lines_[target]));
        }
        if (!select) {
            this->anchor_ = this->cursor_;
        }
    }

    void FEditor::moveWord(const bool forward, const bool select) {
        this->desiredCol_ = -1;
        const std::string& line = this->lines_[this->cursor_.line];
        const int len = colCount(line);
        const auto isWord = [&line](const int c) {
            const std::size_t b = byteOffset(line, c);
            if (b >= line.size()) {
                return false;
            }
            const unsigned char ch = static_cast<unsigned char>(line[b]);
            return std::isalnum(ch) != 0 || ch == '_';
        };
        int col = this->cursor_.col;
        if (!forward) {
            while (col > 0 && !isWord(col - 1)) {
                col--;
            }
            while (col > 0 && isWord(col - 1)) {
                col--;
            }
        } else {
            while (col < len && !isWord(col)) {
                col++;
            }
            while (col < len && isWord(col)) {
                col++;
            }
        }
        this->cursor_.col = col;
        if (!select) {
            this->anchor_ = this->cursor_;
        }
    }

    void FEditor::selectWordAt(const Pos& p) {
        this->desiredCol_ = -1;
        const int line = std::clamp(p.line, 0, static_cast<int>(this->lines_.size()) - 1);
        const std::string& s = this->lines_[line];
        const int len = colCount(s);
        const int col = std::clamp(p.col, 0, len);
        const auto isWord = [&s](const int c) {
            const std::size_t b = byteOffset(s, c);
            if (b >= s.size()) {
                return false;
            }
            const unsigned char ch = static_cast<unsigned char>(s[b]);
            return std::isalnum(ch) != 0 || ch == '_';
        };
        if (len == 0 || !isWord(col)) {
            // несловесный символ выделяем целиком, как в vscode
            this->anchor_ = {line, col};
            this->cursor_ = {line, std::min(col + 1, len)};
            return;
        }
        int from = col;
        int to = col;
        while (from > 0 && isWord(from - 1)) {
            from--;
        }
        while (to < len && isWord(to + 1)) {
            to++;
        }
        this->anchor_ = {line, from};
        this->cursor_ = {line, std::min(to + 1, len)};
    }

    void FEditor::selectLineAt(const int line) {
        this->desiredCol_ = -1;
        const int l = std::clamp(line, 0, static_cast<int>(this->lines_.size()) - 1);
        this->anchor_ = {l, 0};
        if (l + 1 < static_cast<int>(this->lines_.size())) {
            this->cursor_ = {l + 1, 0};
        } else {
            this->cursor_ = {l, colCount(this->lines_[l])};
        }
    }

    std::string FEditor::selectedTextOrLine() const {
        if (!this->hasSelection()) {
            return this->lines_[this->cursor_.line];
        }
        const Pos s = this->selectionStart();
        const Pos e = this->selectionEnd();
        if (s.line == e.line) {
            const std::size_t from = byteOffset(this->lines_[s.line], s.col);
            const std::size_t to = byteOffset(this->lines_[s.line], e.col);
            return this->lines_[s.line].substr(from, to - from);
        }
        std::string out = this->lines_[s.line].substr(byteOffset(this->lines_[s.line], s.col));
        for (int row = s.line + 1; row < e.line; ++row) {
            out += '\n';
            out += this->lines_[row];
        }
        out += '\n';
        out += this->lines_[e.line].substr(0, byteOffset(this->lines_[e.line], e.col));
        return out;
    }

    std::string FEditor::lineIndent(const int line) const {
        const std::string& s = this->lines_[line];
        std::size_t i = 0;
        while (i < s.size() && s[i] == ' ') {
            i++;
        }
        return s.substr(0, i);
    }

    void FEditor::ensureCursorVisible() {
        const Rectangle area = this->contentRect();
        const float lh = this->lineHeight();
        const float adv = this->advance();

        const float top = EDITOR_PAD_Y + this->cursor_.line * lh;
        if (top - this->scrollY_ < 0.0f) {
            this->scrollY_ = top;
        }
        if (top + lh - this->scrollY_ > area.height) {
            this->scrollY_ = top + lh - area.height;
        }
        this->scrollY_ = std::clamp(this->scrollY_, 0.0f, this->maxScrollY());

        const float textX = this->gutterWidth() + TEXT_PAD_X;
        const float cx = this->cursor_.col * adv;
        const float view = this->textViewWidth();
        if (cx - this->scrollX_ < 0.0f) {
            this->scrollX_ = std::max(0.0f, cx - adv * 2.0f);
        } else if (cx + adv - this->scrollX_ > view) {
            this->scrollX_ = cx + adv - view;
        }
        this->scrollX_ = std::clamp(this->scrollX_, 0.0f, this->maxScrollX());
    }

    // --- геометрия ---------------------------------------------------------------

    float FEditor::advance() const {
        return EditorFont::instance().advance(FONT_SIZE);
    }

    float FEditor::gutterWidth() const {
        const int digits = std::max(2, static_cast<int>(std::to_string(this->lines_.size()).size()));
        return static_cast<float>(digits) * this->advance() + 12.0f;
    }

    Rectangle FEditor::contentRect() const {
        return {0.0f, TAB_BAR_H, this->width_, std::max(0.0f, this->height_ - TAB_BAR_H)};
    }

    Rectangle FEditor::tabRect() const {
        const float nameW = MainFont::instance().measure(this->name_, 13.0f).x;
        const float w = std::clamp(10.0f + 15.0f + 8.0f + nameW + 8.0f + 22.0f + 8.0f, 120.0f, 240.0f);
        return {0.0f, 0.0f, w, TAB_BAR_H};
    }

    Rectangle FEditor::tabCloseRect() const {
        const Rectangle tr = this->tabRect();
        return {tr.x + tr.width - 24.0f, (TAB_BAR_H - 20.0f) * 0.5f, 20.0f, 20.0f};
    }

    // --- миникарта ---------------------------------------------------------------

    Rectangle FEditor::minimapRect() const {
        const Rectangle area = this->contentRect();
        return {area.x + area.width - MINIMAP_W - MINIMAP_GAP, area.y, MINIMAP_W, area.height};
    }

    // окно миникарты скользит по документу пропорционально прокрутке редактора
    float FEditor::minimapScroll() const {
        const float content = static_cast<float>(this->lines_.size()) * MINI_LINE_H;
        const Rectangle mini = this->minimapRect();
        if (content <= mini.height) {
            return 0.0f; // весь документ влезает — миникарта статична
        }
        const float maxEditor = this->maxScrollY();
        const float ratio = maxEditor > 0.0f ? this->scrollY_ / maxEditor : 0.0f;
        return ratio * (content - mini.height);
    }

    void FEditor::handleMinimapPress(const float relY) {
        const Rectangle area = this->contentRect();
        const float maxEditor = this->maxScrollY();
        if (maxEditor <= 0.0f) {
            return; // документ и так виден целиком
        }
        const float lh = this->lineHeight();
        const float sliderTop = (this->scrollY_ / lh) * MINI_LINE_H;
        const float sliderH = (area.height / lh) * MINI_LINE_H;
        const float yInContent = relY + this->minimapScroll();

        if (yInContent >= sliderTop && yInContent <= sliderTop + sliderH) {
            // захват слайдера: тянем с сохранением точки хвата
            this->minimapDrag_ = true;
            this->minimapGrabOffset_ = yInContent - sliderTop;
            return;
        }
        // клик мимо слайдера: строка под курсором — в центр вьюпорта
        const int line = static_cast<int>(yInContent / MINI_LINE_H);
        this->scrollY_ = std::clamp(line * lh - area.height * 0.5f, 0.0f, maxEditor);
    }

    void FEditor::drawMinimap() const {
        float ax, ay;
        this->absolutePos(ax, ay);
        const Theme& theme = Theme::instance();
        const EditorFont& mono = EditorFont::instance();
        const Rectangle area = this->contentRect();
        const Rectangle mini = this->minimapRect();
        const float miniAdv = mono.advance(MINI_FONT);
        const float scroll = this->minimapScroll();

        // непрозрачный фон: уехавший за миникарту код не должен просвечивать
        DrawRectangleRec({ax + mini.x, ay + mini.y, mini.width, mini.height}, theme.editorBg);
        DrawLineEx({ax + mini.x, ay + mini.y}, {ax + mini.x, ay + mini.y + mini.height}, 1.0f, theme.editorBorder);

        BeginScissorMode(static_cast<int>(ax + mini.x + 1.0f), static_cast<int>(ay + mini.y),
                         static_cast<int>(mini.width - 1.0f), static_cast<int>(mini.height));

        const int firstMini = std::max(0, static_cast<int>(scroll / MINI_LINE_H));
        const int lastMini = std::min<int>(static_cast<int>(this->lines_.size()),
                                           firstMini + static_cast<int>(mini.height / MINI_LINE_H) + 2);
        std::vector<Token> tokens;
        for (int i = firstMini; i < lastMini; ++i) {
            const std::string& line = this->lines_[i];
            if (line.empty()) {
                continue;
            }
            int st = this->lineStates_[i];
            tokens.clear();
            this->tokenizeLine(line, st, tokens);
            const float y = ay + mini.y + i * MINI_LINE_H - scroll;
            for (const Token& t : tokens) {
                const float x = ax + mini.x + 2.0f + t.col * miniAdv;
                if (x - (ax + mini.x) >= mini.width) {
                    break; // токены идут по возрастанию колонки — дальше не рисуем
                }
                float w = t.len * miniAdv;
                if (x + w - (ax + mini.x) > mini.width) {
                    w = mini.width - (x - (ax + mini.x));
                }
                if (w <= 0.0f) {
                    continue;
                }
                // текст в 4px читается как цветные чёрточки — то, что нужно миникарте
                mono.draw(line.substr(byteOffset(line, t.col), byteOffset(line, t.col + t.len) - byteOffset(line, t.col)),
                          x, y, MINI_FONT, t.color);
            }
        }

        // слайдер: полоса видимой области редактора
        const float lh = this->lineHeight();
        const float sliderTop = (this->scrollY_ / lh) * MINI_LINE_H - scroll;
        const float sliderH = (area.height / lh) * MINI_LINE_H;
        const Color slider{theme.accent.r, theme.accent.g, theme.accent.b, 80};
        DrawRectangleRec({ax + mini.x + 1.0f, ay + mini.y + sliderTop, mini.width - 1.0f, sliderH}, slider);
        EndScissorMode();
    }

    float FEditor::maxScrollY() const {
        const Rectangle area = this->contentRect();
        const float content = static_cast<float>(this->lines_.size()) * this->lineHeight() + 2.0f * EDITOR_PAD_Y;
        return std::max(0.0f, content - area.height);
    }

    float FEditor::textViewWidth() const {
        return std::max(1.0f, this->contentRect().width - this->gutterWidth() - TEXT_PAD_X
                                  - MINIMAP_W - MINIMAP_GAP);
    }

    float FEditor::maxScrollX() const {
        if (this->widthDirty_) {
            int maxCols = 0;
            for (const std::string& line : this->lines_) {
                maxCols = std::max(maxCols, colCount(line));
            }
            this->maxWidthPx_ = (maxCols + 2) * this->advance();
            this->widthDirty_ = false;
        }
        return std::max(0.0f, this->maxWidthPx_ - this->textViewWidth());
    }

    // --- ввод ----------------------------------------------------------------------

    bool FEditor::repeatKey(const int key, const float dt) {
        if (IsKeyPressed(key)) {
            this->repeatKey_ = key;
            this->repeatTimer_ = 0.35f;
            return true;
        }
        if (IsKeyDown(key) && this->repeatKey_ == key) {
            this->repeatTimer_ -= dt;
            if (this->repeatTimer_ <= 0.0f) {
                this->repeatTimer_ = 0.02f;
                return true;
            }
            return false;
        }
        if (!IsKeyDown(key) && this->repeatKey_ == key) {
            this->repeatKey_ = -1;
        }
        return false;
    }

    FEditor::Pos FEditor::posFromMouse(const float mx, const float my) const {
        float ax, ay;
        this->absolutePos(ax, ay);
        const Rectangle area = this->contentRect();
        const float lh = this->lineHeight();
        const float adv = this->advance();

        int line = static_cast<int>((my - (ay + area.y + EDITOR_PAD_Y) + this->scrollY_) / lh);
        line = std::clamp(line, 0, static_cast<int>(this->lines_.size()) - 1);

        const float textX = this->gutterWidth() + TEXT_PAD_X;
        int col = static_cast<int>(std::lround((mx - (ax + area.x + textX) + this->scrollX_) / adv));
        col = std::clamp(col, 0, colCount(this->lines_[line]));
        return {line, col};
    }

    void FEditor::pollKeys(const float dt) {
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        // --- печатаемые символы (владелец клавиатуры — только мы) -----------
        {
            bool typed = false;
            int ch = GetCharPressed();
            while (ch > 0) {
                if (ch >= 32 && !ctrl) {
                    std::string utf8;
                    appendUtf8(utf8, ch);
                    this->insertText(utf8);
                    typed = true;
                }
                ch = GetCharPressed();
            }
            if (typed) {
                return; // печать и навигация в один кадр не смешиваются
            }
        }

        // --- сохранение, история, буфер обмена -------------------------------
        if (ctrl && IsKeyPressed(KEY_S)) {
            this->save();
            return;
        }
        if (ctrl && !shift && IsKeyPressed(KEY_Z)) {
            this->doUndo();
            return;
        }
        if (ctrl && (IsKeyPressed(KEY_Y) || (shift && IsKeyPressed(KEY_Z)))) {
            this->doRedo();
            return;
        }
        if (ctrl && !shift && IsKeyPressed(KEY_C)) {
            SetClipboardText(this->selectedTextOrLine().c_str());
            return;
        }
        if (ctrl && IsKeyPressed(KEY_X)) {
            SetClipboardText(this->selectedTextOrLine().c_str());
            this->pushUndo(1);
            if (this->hasSelection()) {
                this->eraseSelectionInternal();
            } else {
                // без выделения вырезаем строку целиком, как в vscode
                const int line = this->cursor_.line;
                this->lines_.erase(this->lines_.begin() + line);
                if (this->lines_.empty()) {
                    this->lines_.emplace_back("");
                }
                this->cursor_ = {std::min(line, static_cast<int>(this->lines_.size()) - 1), 0};
                this->anchor_ = this->cursor_;
            }
            this->markEdited();
            return;
        }
        if (ctrl && !shift && IsKeyPressed(KEY_V)) {
            const char* clip = GetClipboardText();
            if (clip && clip[0] != '\0') {
                this->insertText(clip);
            }
            return;
        }
        if (ctrl && IsKeyPressed(KEY_A)) {
            this->anchor_ = {0, 0};
            this->cursor_ = {static_cast<int>(this->lines_.size()) - 1, colCount(this->lines_.back())};
            this->desiredCol_ = -1;
            this->ensureCursorVisible();
            return;
        }

        // --- перемещение каретки ---------------------------------------------
        if (this->repeatKey(KEY_LEFT, dt)) {
            if (ctrl) {
                this->moveWord(false, shift);
            } else {
                this->moveCursorHoriz(-1, shift);
            }
            this->ensureCursorVisible();
            return;
        }
        if (this->repeatKey(KEY_RIGHT, dt)) {
            if (ctrl) {
                this->moveWord(true, shift);
            } else {
                this->moveCursorHoriz(1, shift);
            }
            this->ensureCursorVisible();
            return;
        }
        if (this->repeatKey(KEY_UP, dt)) {
            this->moveCursorVert(-1, shift);
            this->ensureCursorVisible();
            return;
        }
        if (this->repeatKey(KEY_DOWN, dt)) {
            this->moveCursorVert(1, shift);
            this->ensureCursorVisible();
            return;
        }
        if (IsKeyPressed(KEY_HOME) || IsKeyPressed(KEY_END)) {
            const bool home = IsKeyPressed(KEY_HOME);
            this->desiredCol_ = -1;
            if (ctrl) {
                this->cursor_ = home ? Pos{0, 0}
                                     : Pos{static_cast<int>(this->lines_.size()) - 1,
                                           colCount(this->lines_.back())};
            } else if (home) {
                // smart home: сначала к первому не-пробелу, повторно — в начало
                const int indent = colCount(this->lineIndent(this->cursor_.line));
                this->cursor_.col = this->cursor_.col == indent ? 0 : indent;
            } else {
                this->cursor_.col = colCount(this->lines_[this->cursor_.line]);
            }
            if (!shift) {
                this->anchor_ = this->cursor_;
            }
            this->ensureCursorVisible();
            return;
        }
        if (IsKeyPressed(KEY_PAGE_UP) || IsKeyPressed(KEY_PAGE_DOWN)) {
            const Rectangle area = this->contentRect();
            const int rows = std::max(1, static_cast<int>(area.height / this->lineHeight()) - 2);
            this->moveCursorVert(IsKeyPressed(KEY_PAGE_UP) ? -rows : rows, shift);
            this->ensureCursorVisible();
            return;
        }

        // --- правка -----------------------------------------------------------
        if (this->repeatKey(KEY_BACKSPACE, dt)) {
            this->deleteChar(false);
            return;
        }
        if (this->repeatKey(KEY_DELETE, dt)) {
            this->deleteChar(true);
            return;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            // авто-отступ: ведущие пробелы копируются, после { — ещё одна ступень
            std::string indent = this->lineIndent(this->cursor_.line);
            std::string trimmed = this->lines_[this->cursor_.line];
            while (!trimmed.empty() && trimmed.back() == ' ') {
                trimmed.pop_back();
            }
            if (!trimmed.empty() && trimmed.back() == '{') {
                indent += "    ";
            }
            this->insertText("\n" + indent);
            return;
        }
        if (IsKeyPressed(KEY_TAB)) {
            this->pushUndo(0);
            if (shift) {
                // убрать до 4 ведущих пробелов у строки с кареткой
                std::string& line = this->lines_[this->cursor_.line];
                int removed = 0;
                while (!line.empty() && line.front() == ' ' && removed < 4) {
                    line.erase(0, 1);
                    ++removed;
                }
                this->cursor_.col = std::max(0, this->cursor_.col - removed);
                this->anchor_ = this->cursor_;
            } else {
                this->insertAtCursor("    ");
            }
            this->markEdited();
            return;
        }
    }

    // --- взаимодействие мышью -------------------------------------------------------

    void FEditor::onMousePress() {
        float ax, ay;
        this->absolutePos(ax, ay);
        const Vector2 mouse = GetMousePosition();

        // крестик на вкладке закрывает документ
        if (this->hasDocument()) {
            const Rectangle close = this->tabCloseRect();
            if (CheckCollisionPointRec(mouse, {ax + close.x, ay + close.y, close.width, close.height})) {
                this->closeDocument();
                return;
            }
        }

        this->grabKeyboard();

        if (!this->hasDocument()) {
            return;
        }

        // клик по миникарте: прыжок или захват слайдера
        const Rectangle mini = this->minimapRect();
        if (CheckCollisionPointRec(mouse, {ax + mini.x, ay + mini.y, mini.width, mini.height})) {
            this->handleMinimapPress(mouse.y - (ay + mini.y));
            return;
        }

        const Rectangle area = this->contentRect();
        const bool inCode = mouse.y >= ay + area.y && mouse.y <= ay + area.y + area.height;
        if (!inCode) {
            return; // клик по вкладке — просто фокус
        }

        const Pos p = this->posFromMouse(mouse.x, mouse.y);
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const double now = GetTime();

        // серия кликов: одиночный — каретка, двойной — слово, тройной — строка
        const bool nearLast = now - this->lastClickTime_ < 0.4 &&
                              p.line == this->lastClickPos_.line &&
                              std::abs(p.col - this->lastClickPos_.col) <= 2;
        this->clickCount_ = nearLast ? std::min(this->clickCount_ + 1, 3) : 1;
        this->lastClickTime_ = now;
        this->lastClickPos_ = p;

        if (shift && this->clickCount_ == 1) {
            this->moveCursor(p, true);
        } else {
            switch (this->clickCount_) {
                case 2:
                    this->selectWordAt(p);
                    break;
                case 3:
                    this->selectLineAt(p.line);
                    break;
                default:
                    this->moveCursor(p, false);
                    break;
            }
        }
        this->dragging_ = true;
        this->ensureCursorVisible();
    }

    void FEditor::onMouseRelease() {
        this->dragging_ = false;
        this->minimapDrag_ = false;
    }

    void FEditor::onUpdate(const float dt) {
        // следим за окном и сайдбаром слева
        const float inset = this->leftInset_ ? this->leftInset_() : 0.0f;
        this->setPosition(inset, FMenuBar::HEIGHT);
        this->width_ = std::max(100.0f, static_cast<float>(GetScreenWidth()) - inset);
        this->height_ = std::max(100.0f, static_cast<float>(GetScreenHeight()) - FMenuBar::HEIGHT);

        if (!this->hasDocument()) {
            return;
        }

        float ax, ay;
        this->absolutePos(ax, ay);
        const Vector2 mouse = GetMousePosition();
        const bool inside = mouse.x >= ax && mouse.x <= ax + this->width_ &&
                            mouse.y >= ay && mouse.y <= ay + this->height_;

        // колесо работает только когда курсор над редактором: в raylib оно
        // не consume-ится, и без проверки крутились бы все панели сразу
        if (inside) {
            const Vector2 wheel = GetMouseWheelMoveV();
            if (wheel.y != 0.0f) {
                this->scrollY_ = std::clamp(this->scrollY_ - wheel.y * this->lineHeight() * 3.0f,
                                            0.0f, this->maxScrollY());
            }
            if (wheel.x != 0.0f) {
                this->scrollX_ = std::clamp(this->scrollX_ + wheel.x * 40.0f, 0.0f, this->maxScrollX());
            }
        }

        // дотягивание выделения при зажатой кнопке
        if (this->dragging_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            this->moveCursor(this->posFromMouse(mouse.x, mouse.y), true);
            this->ensureCursorVisible();
        }

        // слайдер миникарты тянет прокрутку редактора
        if (this->minimapDrag_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            const Rectangle mini = this->minimapRect();
            const float maxEditor = this->maxScrollY();
            if (maxEditor > 0.0f) {
                const float yInContent = mouse.y - (ay + mini.y) + this->minimapScroll();
                const float targetTop = (yInContent - this->minimapGrabOffset_) / MINI_LINE_H * this->lineHeight();
                this->scrollY_ = std::clamp(targetTop, 0.0f, maxEditor);
            }
        }

        // курсор мыши: айбим над кодом, рука над крестиком вкладки
        if (inside) {
            if (mouse.y >= ay + TAB_BAR_H) {
                const Rectangle mini = this->minimapRect();
                const bool overMinimap = mouse.x >= ax + mini.x && mouse.x <= ax + mini.x + mini.width;
                SetMouseCursor(overMinimap ? MOUSE_CURSOR_ARROW : MOUSE_CURSOR_IBEAM);
            } else {
                const Rectangle close = this->tabCloseRect();
                SetMouseCursor(CheckCollisionPointRec(mouse, {ax + close.x, ay + close.y, close.width, close.height})
                                   ? MOUSE_CURSOR_POINTING_HAND
                                   : MOUSE_CURSOR_ARROW);
            }
        } else if (this->mouseInside_) {
            SetMouseCursor(MOUSE_CURSOR_ARROW);
        }
        this->mouseInside_ = inside;

        if (this->ownsKeyboard()) {
            this->pollKeys(dt);
        }
    }

    // --- подсветка синтаксиса ---------------------------------------------------------

    void FEditor::tokenizeLine(const std::string& line, int& state, std::vector<Token>& out) const {
        switch (this->lang_) {
            case Lang::Cpp:
                this->tokenizeCpp(line, state, out);
                break;
            case Lang::Json:
                this->tokenizeJson(line, state, out);
                break;
            default:
                out.push_back({0, colCount(line), Theme::instance().text});
                break;
        }
    }

    void FEditor::tokenizeCpp(const std::string& line, int& state, std::vector<Token>& out) const {
        const Theme& theme = Theme::instance();
        const std::size_t n = line.size();

        // препроцессор: строка, начинающаяся с # — целиком одним цветом
        const std::size_t firstChar = line.find_first_not_of(' ');
        if (firstChar != std::string::npos && line[firstChar] == '#') {
            out.push_back({0, colCount(line), theme.editorPreproc});
            return;
        }

        auto push = [&out](const int col, const int len, const Color color) {
            out.push_back({col, len, color});
        };

        int cp = 0;        // текущий символ в codepoints
        std::size_t i = 0; // текущий байт

        // продолжение блочного комментария с прошлой строки
        if (state == 1) {
            const std::size_t end = line.find("*/");
            if (end == std::string::npos) {
                push(0, colCount(line), theme.editorComment);
                return;
            }
            const std::size_t through = end + 2;
            push(0, colCount(line.substr(0, through)), theme.editorComment);
            i = through;
            cp = colCount(line.substr(0, through));
            state = 0;
        }

        while (i < n) {
            const char c = line[i];
            const unsigned char b = static_cast<unsigned char>(c);
            const int step = cpLen(line, i);

            // не-ascii (кириллица в комментариях-строках) — обычный текст
            if (step > 1) {
                push(cp, 1, theme.text);
                i += static_cast<std::size_t>(step);
                cp += 1;
                continue;
            }

            if (c == '/' && i + 1 < n && line[i + 1] == '/') {
                push(cp, colCount(line.substr(i)), theme.editorComment);
                break;
            }
            if (c == '/' && i + 1 < n && line[i + 1] == '*') {
                const std::size_t end = line.find("*/", i + 2);
                if (end == std::string::npos) {
                    push(cp, colCount(line.substr(i)), theme.editorComment);
                    state = 1;
                    break;
                }
                const std::size_t through = end + 2;
                push(cp, colCount(line.substr(i, through - i)), theme.editorComment);
                i = through;
                cp = colCount(line.substr(0, through));
                continue;
            }
            if (c == '"' || c == '\'') {
                std::size_t j = i + 1;
                while (j < n) {
                    if (line[j] == '\\') {
                        j += 2;
                        continue;
                    }
                    if (line[j] == c) {
                        j += 1;
                        break;
                    }
                    j += 1;
                }
                j = std::min(j, n);
                push(cp, colCount(line.substr(i, j - i)), theme.editorString);
                i = j;
                cp = colCount(line.substr(0, i));
                continue;
            }
            if (std::isdigit(b)) {
                std::size_t j = i;
                if (c == '0' && j + 1 < n && (line[j + 1] == 'x' || line[j + 1] == 'X')) {
                    j += 2;
                    while (j < n && std::isxdigit(static_cast<unsigned char>(line[j]))) {
                        j++;
                    }
                } else {
                    while (j < n && std::isdigit(static_cast<unsigned char>(line[j]))) {
                        j++;
                    }
                    if (j < n && line[j] == '.') {
                        j++;
                        while (j < n && std::isdigit(static_cast<unsigned char>(line[j]))) {
                            j++;
                        }
                    }
                    if (j < n && (line[j] == 'e' || line[j] == 'E')) {
                        std::size_t k = j + 1;
                        if (k < n && (line[k] == '+' || line[k] == '-')) {
                            k++;
                        }
                        if (k < n && std::isdigit(static_cast<unsigned char>(line[k]))) {
                            j = k;
                            while (j < n && std::isdigit(static_cast<unsigned char>(line[j]))) {
                                j++;
                            }
                        }
                    }
                }
                while (j < n && (std::isdigit(static_cast<unsigned char>(line[j])) ||
                                 line[j] == 'u' || line[j] == 'U' ||
                                 line[j] == 'l' || line[j] == 'L' || line[j] == 'f' || line[j] == 'F')) {
                    j++;
                }
                push(cp, colCount(line.substr(i, j - i)), theme.editorNumber);
                i = j;
                cp = colCount(line.substr(0, i));
                continue;
            }
            if (std::isalpha(b) || c == '_') {
                std::size_t j = i;
                while (j < n && (std::isalnum(static_cast<unsigned char>(line[j])) || line[j] == '_')) {
                    j++;
                }
                const std::string word = line.substr(i, j - i);
                // следующий не-пробельный символ — «(» значит вызов функции
                std::size_t k = j;
                while (k < n && line[k] == ' ') {
                    k++;
                }
                Color color = theme.editorVariable;
                if (cppControlKeywords().count(word) != 0) {
                    color = theme.editorControl;
                } else if (cppTypes().count(word) != 0) {
                    color = theme.editorType;
                } else if (cppLiterals().count(word) != 0 || cppKeywords().count(word) != 0) {
                    color = theme.editorKeyword;
                } else if (k < n && line[k] == '(') {
                    color = theme.editorFunction;
                }
                push(cp, colCount(line.substr(i, j - i)), color);
                i = j;
                cp = colCount(line.substr(0, i));
                continue;
            }

            // операторы и пунктуация
            push(cp, 1, theme.text);
            i += 1;
            cp += 1;
        }
    }

    void FEditor::tokenizeJson(const std::string& line, int& state, std::vector<Token>& out) const {
        // state: 0 — ждём ключ или структуру, 1 — после двоеточия (значение)
        const Theme& theme = Theme::instance();
        const std::size_t n = line.size();
        int cp = 0;
        std::size_t i = 0;

        while (i < n) {
            const char c = line[i];
            if (c == ' ' || c == '\t') {
                i += 1;
                cp += 1;
                continue;
            }
            if (c == '"') {
                std::size_t j = i + 1;
                while (j < n) {
                    if (line[j] == '\\') {
                        j += 2;
                        continue;
                    }
                    if (line[j] == '"') {
                        j += 1;
                        break;
                    }
                    j += 1;
                }
                j = std::min(j, n);
                const int len = colCount(line.substr(i, j - i));
                out.push_back({cp, len, state == 1 ? theme.editorString : theme.editorVariable});
                i = j;
                cp += len;
                continue;
            }
            if (c == ':') {
                state = 1;
            } else if (c == ',' || c == '{' || c == '[' || c == '}' || c == ']') {
                state = 0;
            }
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') {
                std::size_t j = i + 1;
                while (j < n && (std::isdigit(static_cast<unsigned char>(line[j])) ||
                                 line[j] == '.' || line[j] == 'e' || line[j] == 'E' ||
                                 line[j] == '+' || line[j] == '-')) {
                    j++;
                }
                out.push_back({cp, colCount(line.substr(i, j - i)), theme.editorNumber});
                cp += colCount(line.substr(i, j - i));
                i = j;
                continue;
            }
            if (std::isalpha(static_cast<unsigned char>(c))) {
                std::size_t j = i;
                while (j < n && std::isalpha(static_cast<unsigned char>(line[j]))) {
                    j++;
                }
                out.push_back({cp, colCount(line.substr(i, j - i)), theme.editorKeyword});
                cp += colCount(line.substr(i, j - i));
                i = j;
                continue;
            }
            out.push_back({cp, 1, theme.text});
            i += 1;
            cp += 1;
        }
    }

    // --- отрисовка -----------------------------------------------------------------

    bool FEditor::hitTest(const float mx, const float my) const {
        float ax, ay;
        this->absolutePos(ax, ay);
        return mx >= ax && mx <= ax + this->width_ && my >= ay && my <= ay + this->height_;
    }

    void FEditor::drawEmptyState() const {
        float ax, ay;
        this->absolutePos(ax, ay);
        const Rectangle area = this->contentRect();
        const Theme& theme = Theme::instance();

        DrawRectangleRec({ax + area.x, ay + area.y, area.width, area.height}, theme.editorBg);

        const float cx = ax + area.x + area.width * 0.5f;
        const float cy = ay + area.y + area.height * 0.5f;
        const Color dim{theme.iconIdle.r, theme.iconIdle.g, theme.iconIdle.b, 70};
        IconFont::instance().drawCentered(ICON_LC_FILE_CODE_2, cx - 40.0f, cy - 96.0f, 80.0f, 80.0f, 72.0f, dim);
        MainFont::instance().drawCenteredX("Open a file from the Explorer", cx - 220.0f, 440.0f, cy - 6.0f, 15.0f, theme.textDim);
        MainFont::instance().drawCenteredX("or press Ctrl+P to search files", cx - 220.0f, 440.0f, cy + 18.0f, 13.0f,
                                           Color{theme.textDim.r, theme.textDim.g, theme.textDim.b, 140});
    }

    void FEditor::drawTabBar() const {
        float ax, ay;
        this->absolutePos(ax, ay);
        const Theme& theme = Theme::instance();
        const IconFont& icons = IconFont::instance();
        const MainFont& font = MainFont::instance();
        const Rectangle tr = this->tabRect();
        const Vector2 mouse = GetMousePosition();

        // полоса вкладок с активной вкладкой первого файла
        DrawRectangleRec({ax, ay, this->width_, TAB_BAR_H}, theme.editorTabBarBg);
        DrawRectangleRec({ax, ay + TAB_BAR_H - 1.0f, this->width_, 1.0f}, theme.editorBorder);
        DrawRectangleRec({ax + tr.x, ay, tr.width, TAB_BAR_H}, theme.editorBg);
        DrawRectangleRec({ax + tr.x, ay, tr.width, 2.0f}, theme.accent);

        icons.draw(FFileTree::fileIcon(this->name_), ax + tr.x + 10.0f, ay + (TAB_BAR_H - 15.0f) * 0.5f, 15.0f, theme.iconIdle);

        const float nameX = ax + tr.x + 10.0f + 15.0f + 8.0f;
        font.drawEllipsized(this->name_, nameX, ay + (TAB_BAR_H - 13.0f) * 0.5f,
                            tr.width - (nameX - (ax + tr.x)) - 30.0f, 13.0f, theme.text);

        // крестик; у несохранённого файла — точка, при наведении всё равно крестик
        const Rectangle close = this->tabCloseRect();
        const bool closeHover = CheckCollisionPointRec(mouse, {ax + close.x, ay + close.y, close.width, close.height});
        const float ccx = ax + close.x + close.width * 0.5f;
        const float ccy = ay + TAB_BAR_H * 0.5f;
        if (closeHover) {
            DrawRectangleRec({ax + close.x, ay + (TAB_BAR_H - 22.0f) * 0.5f, close.width, 22.0f}, theme.hoverOverlay);
        }
        if (this->dirty_ && !closeHover) {
            DrawCircle(ccx, ccy, 2.5f, theme.text);
        } else {
            icons.draw(ICON_LC_X, ccx - 6.0f, ccy - 6.0f, 12.0f, closeHover ? theme.text : theme.iconIdle);
        }
    }

    void FEditor::drawCode() const {
        float ax, ay;
        this->absolutePos(ax, ay);
        const Theme& theme = Theme::instance();
        const EditorFont& mono = EditorFont::instance();
        const float lh = this->lineHeight();
        const float adv = this->advance();
        const Rectangle area = this->contentRect();
        const float textX = this->gutterWidth() + TEXT_PAD_X;

        // состояния строк (внутри блочного комментария или нет) пересобираем лениво;
        // проверка размера — страховка от рассинхрона после любой мутации
        if (this->statesDirty_ || this->lineStates_.size() != this->lines_.size() + 1) {
            this->lineStates_.assign(this->lines_.size() + 1, 0);
            if (this->lang_ != Lang::Plain) {
                int st = 0;
                std::vector<Token> scratch;
                for (std::size_t i = 0; i < this->lines_.size(); ++i) {
                    this->lineStates_[i] = st;
                    scratch.clear();
                    this->tokenizeLine(this->lines_[i], st, scratch);
                }
                this->lineStates_.back() = st;
            }
            this->statesDirty_ = false;
        }

        const int lineTotal = static_cast<int>(this->lines_.size());
        const int first = std::clamp(static_cast<int>((this->scrollY_ - EDITOR_PAD_Y) / lh), 0, std::max(0, lineTotal - 1));
        const int visible = static_cast<int>(area.height / lh) + 2;
        const int last = std::min(first + visible, lineTotal);

        const auto rowTop = [&](const int row) {
            return ay + area.y + EDITOR_PAD_Y + row * lh - this->scrollY_;
        };

        // фон, строка каретки, выделение
        BeginScissorMode(static_cast<int>(ax + area.x), static_cast<int>(ay + area.y),
                         static_cast<int>(area.width), static_cast<int>(area.height));
        DrawRectangleRec({ax + area.x, ay + area.y, area.width, area.height}, theme.editorBg);
        if (!this->hasSelection()) {
            DrawRectangleRec({ax + area.x + textX, rowTop(this->cursor_.line), area.width - textX, lh},
                             theme.editorCurrentLine);
        }
        if (this->hasSelection()) {
            const Pos s = this->selectionStart();
            const Pos e = this->selectionEnd();
            for (int row = s.line; row <= e.line; ++row) {
                const float x0 = row == s.line ? s.col * adv : 0.0f;
                const float x1 = row == e.line ? e.col * adv : colCount(this->lines_[row]) * adv;
                DrawRectangleRec({ax + area.x + textX + x0 - this->scrollX_, rowTop(row),
                                  std::max(x1 - x0, 2.0f), lh}, theme.editorSelection);
            }
        }
        EndScissorMode();

        // сам код: клип по зоне правее гаттера, строки уезжают за горизонтальный скролл
        BeginScissorMode(static_cast<int>(ax + area.x + textX), static_cast<int>(ay + area.y),
                         static_cast<int>(area.width - textX), static_cast<int>(area.height));
        std::vector<Token> tokens;
        for (int row = first; row < last; ++row) {
            const std::string& line = this->lines_[row];
            if (line.empty()) {
                continue;
            }
            const float y = rowTop(row) + (lh - FONT_SIZE) * 0.5f;
            int st = this->lineStates_[row];
            tokens.clear();
            this->tokenizeLine(line, st, tokens);
            for (const Token& t : tokens) {
                const std::size_t from = byteOffset(line, t.col);
                const std::size_t to = byteOffset(line, t.col + t.len);
                if (to <= from) {
                    continue;
                }
                mono.draw(line.substr(from, to - from),
                          ax + area.x + textX + t.col * adv - this->scrollX_, y, FONT_SIZE, t.color);
            }
        }
        EndScissorMode();

        // гаттер с номерами строк — фон непрозрачный, чтобы код не просвечивал
        BeginScissorMode(static_cast<int>(ax + area.x), static_cast<int>(ay + area.y),
                         static_cast<int>(textX - 4.0f), static_cast<int>(area.height));
        DrawRectangleRec({ax + area.x, ay + area.y, textX - 4.0f, area.height}, theme.editorBg);
        for (int row = first; row < last; ++row) {
            const std::string num = std::to_string(row + 1);
            const float w = mono.measure(num, FONT_SIZE).x;
            const Color c = row == this->cursor_.line && !this->hasSelection()
                                ? theme.editorLineNumActive
                                : theme.editorLineNum;
            mono.draw(num, ax + area.x + (textX - 8.0f) - w, rowTop(row) + (lh - FONT_SIZE) * 0.5f, FONT_SIZE, c);
        }
        EndScissorMode();

        // каретка и вертикальный скроллбар поверх всего
        BeginScissorMode(static_cast<int>(ax + area.x), static_cast<int>(ay + area.y),
                         static_cast<int>(area.width), static_cast<int>(area.height));
        if (this->ownsKeyboard() && std::fmod(GetTime(), 1.1) < 0.6) {
            const float cx = ax + area.x + textX + this->cursor_.col * adv - this->scrollX_;
            DrawRectangleRec({cx, rowTop(this->cursor_.line) + 1.0f, 2.0f, lh - 2.0f}, theme.editorCursor);
        }
        const float contentH = lineTotal * lh + 2.0f * EDITOR_PAD_Y;
        if (contentH > area.height) {
            const float thumbH = std::max(24.0f, area.height * area.height / contentH);
            const float ty = ay + area.y + (this->scrollY_ / (contentH - area.height)) * (area.height - thumbH);
            DrawRectangleRec({ax + area.x + area.width - 8.0f, ty, 4.0f, thumbH}, theme.scrollbarThumb);
        }
        EndScissorMode();

        this->drawMinimap();
    }

    void FEditor::onDraw() {
        if (this->hasDocument()) {
            this->drawTabBar();
            this->drawCode();
        } else {
            this->drawEmptyState();
        }
    }
}
