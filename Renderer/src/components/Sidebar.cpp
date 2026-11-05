#include "components/Sidebar.hpp"
#include "IconFont.hpp"
#include "MainFont.hpp"
#include "Theme.hpp"
#include "raylib.h"

#include <cctype>

namespace Renderer {

    FSidebar::FSidebar() = default;

    void FSidebar::setItems(std::vector<Item> items) {
        this->items_ = std::move(items);
        this->content_ = nullptr;
        this->contentIndex_ = -1;
        this->rebuild();
    }

    void FSidebar::rebuild() {
        // вынимаем контент панели, чтобы clearChildren его не удалил
        std::unique_ptr<Component> keep;
        if (this->content_) {
            keep = this->releaseChild(this->content_);
            this->content_ = nullptr;
            this->contentIndex_ = -1;
        }

        this->clearChildren();
        this->buttons_.clear();

        const Theme& theme = Theme::instance();

        this->activityBar_ = &this->addChild<FRectangle>(0, 0, ACTIVITY_WIDTH, this->height_);
        this->activityBar_->setColor(theme.activityBg);

        this->panel_ = &this->addChild<FRectangle>(ACTIVITY_WIDTH, 0, this->panelVisible_ ? PANEL_WIDTH : 0.0f, this->height_);
        this->panel_->setColor(theme.panelBg);

        this->indicator_ = &this->addChild<FRectangle>(0, 0, 2, ACTIVITY_WIDTH);
        this->indicator_->setColor(theme.accent);

        for (std::size_t i = 0; i < this->items_.size(); ++i) {
            auto& btn = this->addChild<FIconButton>(this->items_[i].icon, ACTIVITY_WIDTH);
            btn.setPosition(0, static_cast<float>(i) * ACTIVITY_WIDTH);
            btn.setOnClick([this, i] {
                if (this->active_ == static_cast<int>(i)) {
                    this->panelVisible_ = !this->panelVisible_;
                } else {
                    this->setActive(static_cast<int>(i));
                    this->panelVisible_ = true;
                }
                if (this->onSelect_) {
                    this->onSelect_(this->active_);
                }
            });
            this->buttons_.push_back(&btn);
        }

        // кнопка настроек, прижатая к низу как в vscode
        auto& settings = this->addChild<FIconButton>(ICON_LC_SETTINGS, ACTIVITY_WIDTH);
        settings.setOnClick([this] {
            if (this->onSelect_) {
                this->onSelect_(-1);
            }
        });
        this->settingsButton_ = &settings;

        if (keep) {
            // возвращаем контент последним ребёнком — он поверх панели и получает клики первым
            this->content_ = this->addChild(std::move(keep));
            this->contentIndex_ = this->active_;
        }

        this->updateIndicator();
    }

    // отдаёт владение контентом обратно item-у, чтобы панель можно было переключить
    void FSidebar::detachActiveContent() {
        if (!this->content_) {
            return;
        }
        if (this->contentIndex_ >= 0 && this->contentIndex_ < static_cast<int>(this->items_.size())) {
            this->items_[static_cast<std::size_t>(this->contentIndex_)].content = this->releaseChild(this->content_);
        }
        this->content_ = nullptr;
        this->contentIndex_ = -1;
    }

    void FSidebar::attachActiveContent() {
        if (this->active_ < 0 || this->active_ >= static_cast<int>(this->items_.size())) {
            return;
        }
        auto& content = this->items_[static_cast<std::size_t>(this->active_)].content;
        if (!content) {
            return;
        }
        this->content_ = this->addChild(std::move(content));
        this->contentIndex_ = this->active_;
    }

    void FSidebar::setActive(const int index) {
        this->detachActiveContent();
        this->active_ = index;
        for (std::size_t i = 0; i < this->buttons_.size(); ++i) {
            this->buttons_[i]->setActive(static_cast<int>(i) == index);
        }
        this->attachActiveContent();
        this->updateIndicator();
    }

    void FSidebar::updateIndicator() {
        if (!this->indicator_) {
            return;
        }
        if (this->active_ >= 0 && static_cast<std::size_t>(this->active_) < this->buttons_.size()) {
            this->indicator_->setPosition(0, static_cast<float>(this->active_) * ACTIVITY_WIDTH);
            this->indicator_->setSize(2, ACTIVITY_WIDTH);
        } else {
            this->indicator_->setSize(0, 0);
        }
    }

    void FSidebar::layout() {
        // высота до низа окна с учётом собственного смещения
        // (сайдбар стоит под верхней меню-панелью)
        this->height_ = static_cast<float>(GetScreenHeight()) - this->y_;
        if (this->activityBar_) {
            this->activityBar_->setSize(ACTIVITY_WIDTH, this->height_);
        }
        if (this->panel_) {
            this->panel_->setSize(this->panelVisible_ ? PANEL_WIDTH : 0.0f, this->height_);
        }
        if (this->settingsButton_) {
            this->settingsButton_->setPosition(0, this->height_ - ACTIVITY_WIDTH);
        }
        if (this->content_) {
            this->content_->setPosition(ACTIVITY_WIDTH, PANEL_TOP);
            this->content_->resize(this->panelVisible_ ? PANEL_WIDTH : 0.0f, this->height_ - PANEL_TOP);
        }
    }

    void FSidebar::onUpdate(const float) {
        this->layout();
    }

    bool FSidebar::hitTest(const float mx, const float my) const {
        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);
        const float width = ACTIVITY_WIDTH + (this->panelVisible_ ? PANEL_WIDTH : 0.0f);
        return mx >= ax && mx <= ax + width && my >= ay && my <= ay + this->height_;
    }

    void FSidebar::onDrawAfterChildren() {
        if (!this->panelVisible_ || this->active_ < 0 || this->active_ >= static_cast<int>(this->items_.size())) {
            return;
        }

        float ax = 0.0f;
        float ay = 0.0f;
        this->absolutePos(ax, ay);

        // заголовок панели капсом, как в vscode и jetbrains
        const Item& item = this->items_[static_cast<std::size_t>(this->active_)];
        std::string title = item.title;
        for (char& c : title) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        const Theme& theme = Theme::instance();
        MainFont::instance().draw(title, ax + ACTIVITY_WIDTH + 12.0f, ay + 13.0f, 13.0f,
                                  theme.panelTitle, MainFont::Weight::Bold);
    }
}
