#pragma once
#include <functional>
#include <string>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace UI {

    struct UIRect {
        float x{ 0 }, y{ 0 }, w{ 0 }, h{ 0 }; // screen-space, origin bottom-left
        bool contains(const glm::vec2& p) const {
            return (p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h);
        }
    };

    enum class ButtonState { Normal, Hover, Pressed, Disabled };

    struct UIButton {
        int id{ -1 };
        UIRect rect{};
        std::string label;
        ButtonState state{ ButtonState::Normal };
        bool visible{ true };
        bool enabled{ true };
        int z{ 0 };

        // Callbacks
        std::function<void()> onClicked = []() {};
        std::function<void()> onPressed = []() {};
        std::function<void()> onReleased = []() {};
        std::function<void()> onHover = []() {};
    };

    struct UIDrawContext {
        // Draw a filled rect:  min(x,y), max(x,y), color
        std::function<void(const glm::vec2&, const glm::vec2&, const glm::vec4&)> rectFill;

        // Draw a stroked rect: min, max, thickness(px), color
        std::function<void(const glm::vec2&, const glm::vec2&, float, const glm::vec4&)> rectStroke;

        // Draw text at baseline origin (screen space): text, pos, pxSize, color
        std::function<void(const std::string&, const glm::vec2&, float, const glm::vec4&)> drawText;
    };

    class UIContext {
    public:
        // Add a button and get its id back
        int addButton(const UIRect& r, std::string label, std::function<void()> onClicked);

        // Update rect (e.g., on resize)
        void setButtonRect(int id, const UIRect& r);

        // Per-frame input update
        // mouse: screen-space, origin bottom-left
        // pressedEdge: went down this frame
        // releasedEdge: went up this frame
        // isDown: currently held
        // sets consumesPointer true if a button captured the press this frame
        void update(const glm::vec2& mouse, bool pressedEdge, bool releasedEdge, bool isDown, bool& consumesPointer);

        // Per-frame draw
        // uiScale is a simple scalar for text/spacing if you want it (e.g., screenH / reference)
        void draw(const UIDrawContext& ctx, float uiScale = 1.0f) const;

    private:
        int m_hot{ -1 };    // hovered
        int m_active{ -1 }; // pressed
        std::vector<UIButton> m_buttons;

        static glm::vec4 colorNormal();
        static glm::vec4 colorHover();
        static glm::vec4 colorPressed();
        static glm::vec4 colorDisabled();
    };

} // namespace UI
