#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace Renderer {

    // базовый примитив для гуи фреймворка
    // поддерживает дерево компонентов: позиция хранится относительно родителя,
    // а draw/update рекурсивно проходят по всем детям
    class Component {
    public:
        Component(const Component&) = delete;
        Component& operator=(const Component&) = delete;

        // публичное api — вызывает виртуальные on* методы и рекурсивно детей
        void draw();
        void update(const float dt);
        void mousePress();
        void mouseRelease();

        // скрытый компонент не рисуется, не обновляется и не ловит клики
        void setVisible(const bool visible) { visible_ = visible; }
        [[nodiscard]] bool visible() const { return visible_; }

        template<typename T, typename... Args>
        T& addChild(Args&&... args) {
            auto child = std::make_unique<T>(std::forward<Args>(args)...);
            T& ref = *child;
            this->attach(std::move(child));
            return ref;
        }

        Component* addChild(std::unique_ptr<Component> child);
        void clearChildren() { children_.clear(); }

        // вынимает ребёнка из дерева, передавая владение вызывающему
        // (нужно, чтобы контент панели переживал rebuild сайдбара)
        std::unique_ptr<Component> releaseChild(Component* child);

        std::size_t childCount() const { return children_.size(); }
        Component* parent() const { return parent_; }

        void setPosition(const float x, const float y) { x_ = x; y_ = y; }
        float x() const { return x_; }
        float y() const { return y_; }

        // сообщает компоненту его размеры (владелец панели зовёт при layout)
        void resize(const float width, const float height) { this->onResize(width, height); }

        // абсолютные координаты (с учётом всей цепочки родителей)
        void absolutePos(float& outX, float& outY) const;

        // проверка попадания точки в компонент (в экранных координатах)
        virtual bool hitTest(float mx, float my) const;

        // рекурсивный поиск самого верхнего компонента под точкой (дети приоритетнее)
        Component* hitAt(float mx, float my);

        // --- клавиатура -----------------------------------------------------
        // печатаемый ввод (GetCharPressed) consume-once, поэтому в каждый момент
        // его читает ровно один компонент — владелец клавиатуры. IsKeyPressed
        // не consume-ится, опрашивать его может кто угодно
        void grabKeyboard();   // стать владельцем
        void releaseKeyboard();// отказаться от владения (если оно есть)
        [[nodiscard]] bool ownsKeyboard() const { return keyboardOwner_ == this; }

        virtual ~Component();

    protected:
        Component() = default;

        virtual void onDraw() {}
        virtual void onDrawAfterChildren() {}
        virtual void onUpdate(float dt) {}
        virtual void onMousePress() {}
        virtual void onMouseRelease() {}
        virtual void onResize(float width, float height) {}

        Component* parent_ = nullptr;
        float x_ = 0.0f; // позиция относительно родителя
        float y_ = 0.0f;
        bool visible_ = true;
        std::vector<std::unique_ptr<Component>> children_;

    private:
        static Component* keyboardOwner_;

        void attach(std::unique_ptr<Component> child);
    };
}
