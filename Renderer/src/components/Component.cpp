#include "components/Component.hpp"

namespace Renderer {
    // единственный читатель печатаемого ввода (см. grabKeyboard)
    Component* Component::keyboardOwner_ = nullptr;

    Component::~Component() {
        if (keyboardOwner_ == this) {
            keyboardOwner_ = nullptr;
        }
    }

    void Component::grabKeyboard() {
        keyboardOwner_ = this;
    }

    void Component::releaseKeyboard() {
        if (keyboardOwner_ == this) {
            keyboardOwner_ = nullptr;
        }
    }

    void Component::attach(std::unique_ptr<Component> child) {
        child->parent_ = this;
        children_.push_back(std::move(child));
    }

    Component* Component::addChild(std::unique_ptr<Component> child) {
        Component* raw = child.get();
        this->attach(std::move(child));
        return raw;
    }

    std::unique_ptr<Component> Component::releaseChild(Component* child) {
        for (auto it = this->children_.begin(); it != this->children_.end(); ++it) {
            if (it->get() == child) {
                std::unique_ptr<Component> released = std::move(*it);
                this->children_.erase(it);
                released->parent_ = nullptr;
                return released;
            }
        }
        return nullptr;
    }

    void Component::absolutePos(float& outX, float& outY) const {
        outX = x_;
        outY = y_;
        for (const Component* p = parent_; p != nullptr; p = p->parent_) {
            outX += p->x_;
            outY += p->y_;
        }
    }

    bool Component::hitTest(const float, const float) const {
        // без собственных границ компонент не ловит клики — ловят дети
        return false;
    }

    Component* Component::hitAt(const float mx, const float my) {
        if (!this->visible_) {
            return nullptr;
        }
        for (auto it = this->children_.rbegin(); it != this->children_.rend(); ++it) {
            if (Component* hit = (*it)->hitAt(mx, my)) {
                return hit;
            }
        }
        return this->hitTest(mx, my) ? this : nullptr;
    }

    void Component::draw() {
        if (!this->visible_) {
            return;
        }
        this->onDraw();
        for (auto& c : children_) {
            c->draw();
        }
        this->onDrawAfterChildren();
    }

    void Component::update(const float dt) {
        if (!this->visible_) {
            return;
        }
        this->onUpdate(dt);
        for (auto& c : children_) {
            c->update(dt);
        }
    }

    void Component::mousePress() {
        this->onMousePress();
        for (auto& c : children_) {
            c->mousePress();
        }
    }

    void Component::mouseRelease() {
        this->onMouseRelease();
        for (auto& c : children_) {
            c->mouseRelease();
        }
    }
}
