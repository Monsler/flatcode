#include "Theme.hpp"

namespace Renderer {

    Theme& Theme::instance() {
        static Theme theme;
        return theme;
    }

    // базовая палитра vscode dark modern; выделение строки — в духе jetbrains dark
    void Theme::loadDarkModern() {
        this->windowBg       = {31, 31, 31, 255};    // #1f1f1f
        this->activityBg     = {51, 51, 51, 255};    // #333333
        this->panelBg        = {37, 37, 38, 255};    // #252526
        this->titleBarBg     = {60, 60, 60, 255};    // #3c3c3c
        this->searchBg       = {44, 44, 45, 255};    // #2c2c2d
        this->dropdownBg     = {31, 31, 31, 255};    // #1f1f1f
        this->dropdownBorder = {69, 69, 69, 255};    // #454545
        this->accent         = {0, 120, 212, 255};   // #0078d4
        this->hoverOverlay   = {255, 255, 255, 18};
        this->pressOverlay   = {255, 255, 255, 34};
        this->rowHoverBg     = {255, 255, 255, 14};
        this->rowSelectedBg  = {66, 66, 70, 255};    // выделение строки как в jetbrains
        this->menuHoverBg    = {0, 120, 212, 190};   // подсветка пункта меню как в vscode
        this->iconIdle       = {134, 134, 133, 255};
        this->iconActive     = {255, 255, 255, 255};
        this->text           = {204, 204, 204, 255}; // #cccccc
        this->textDim        = {169, 169, 169, 255}; // #a9a9a9
        this->panelTitle     = {179, 179, 179, 255}; // #b3b3b3
        this->scrollbarThumb = {121, 121, 121, 130};
        this->chevron        = {169, 169, 169, 255};

        // редактор кода: цвета токенов из vscode dark+
        this->editorBg            = {31, 31, 31, 255};    // #1f1f1f
        this->editorTabBarBg      = {24, 24, 24, 255};    // #181818
        this->editorBorder        = {43, 43, 43, 255};    // #2b2b2b
        this->editorLineNum       = {110, 118, 129, 255}; // #6e7681
        this->editorLineNumActive = {204, 204, 204, 255}; // #cccccc
        this->editorCurrentLine   = {255, 255, 255, 10};
        this->editorCursor        = {174, 175, 173, 255}; // #aeafad
        this->editorSelection     = {38, 79, 120, 255};   // #264f78
        this->editorKeyword       = {86, 156, 214, 255};  // #569cd6
        this->editorControl       = {197, 134, 198, 255}; // #c586c6
        this->editorType          = {78, 201, 176, 255};  // #4ec9b0
        this->editorFunction      = {220, 220, 170, 255}; // #dcdcaa
        this->editorVariable      = {156, 220, 254, 255}; // #9cdcfe
        this->editorString        = {206, 145, 120, 255}; // #ce9178
        this->editorNumber        = {181, 206, 168, 255}; // #b5cea8
        this->editorComment       = {106, 153, 85, 255};  // #6a9955
        this->editorPreproc       = {197, 134, 198, 255}; // #c586c6
    }
}
