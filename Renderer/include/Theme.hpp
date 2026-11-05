#pragma once

#include "raylib.h"

namespace Renderer {

    // единая точка палитры оформления: все компоненты берут цвета отсюда,
    // а не держат локальные константы — так тему можно поменять целиком
    class Theme {
    public:
        static Theme& instance();

        // готовые наборы; текущая палитра хранится в полях ниже
        void loadDarkModern();

        // фоны
        Color windowBg;     // фон всего окна
        Color activityBg;   // activity bar слева
        Color panelBg;      // боковые панели
        Color titleBarBg;   // верхняя меню-панель
        Color searchBg;     // поле поиска в меню-панели
        Color dropdownBg;   // фон выпадающего меню
        Color dropdownBorder;// рамка выпадающего меню

        // акценты и состояния
        Color accent;       // активный элемент / индикатор
        Color hoverOverlay; // подложка при наведении
        Color pressOverlay; // подложка при нажатии
        Color rowHoverBg;   // подсветка строки дерева при наведении
        Color rowSelectedBg;// выделение выбранной строки дерева
        Color menuHoverBg;  // подсветка пункта в выпадающем меню

        // иконки
        Color iconIdle;
        Color iconActive;

        // текст
        Color text;      // основной текст
        Color textDim;   // второстепенный текст и значки
        Color panelTitle;// заголовок панели

        // прочее
        Color scrollbarThumb; // ползунок скроллбара
        Color chevron;        // стрелки раскрытия дерева

        // редактор кода (палитра vscode dark modern)
        Color editorBg;           // фон области кода
        Color editorTabBarBg;     // полоса вкладок над кодом
        Color editorBorder;       // тонкие разделители редактора
        Color editorLineNum;      // номера строк
        Color editorLineNumActive;// номер строки с кареткой
        Color editorCurrentLine;  // подсветка строки с кареткой
        Color editorCursor;       // каретка
        Color editorSelection;    // выделение текста
        Color editorKeyword;      // ключевые слова (class, namespace...)
        Color editorControl;      // управляющие конструкции (if, for, return...)
        Color editorType;         // типы (int, auto, std...)
        Color editorFunction;     // имена функций
        Color editorVariable;     // прочие идентификаторы
        Color editorString;       // строковые литералы
        Color editorNumber;       // числа
        Color editorComment;      // комментарии
        Color editorPreproc;      // препроцессор (#include...)

    private:
        Theme() { this->loadDarkModern(); }
    };
}
