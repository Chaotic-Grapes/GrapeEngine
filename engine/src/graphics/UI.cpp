#include "graphics/UI.hpp"
#include <algorithm>

namespace UI {

    int UIContext::addButton(const UIRect& r, std::string label, std::function<void()> onClicked) {
        const int id = static_cast<int>(m_buttons.size());
        UIButton b;
        b.id = id;
        b.rect = r;
        b.label = std::move(label);
        b.onClicked = std::move(onClicked);
        m_buttons.push_back(std::move(b));
        return id;
    }

    void UIContext::setButtonRect(int id, const UIRect& r) {
        if (id >= 0 && id < static_cast<int>(m_buttons.size())) {
            m_buttons[id].rect = r;
        }
    }

    void UIContext::update(const glm::vec2& mouse, bool pressedEdge, bool releasedEdge, bool isDown, bool& consumesPointer) {
        consumesPointer = false;

        // Find topmost hot (we have few buttons; linear scan is fine)
        m_hot = -1;
        for (const auto& b : m_buttons) {
            if (!b.visible || !b.enabled) continue;
            if (b.rect.contains(mouse)) {
                if (m_hot == -1 || b.z >= m_buttons[m_hot].z)
                    m_hot = b.id;
            }
        }

        // Hover callback
        if (m_hot >= 0 && m_buttons[m_hot].enabled) {
            if (m_buttons[m_hot].onHover) m_buttons[m_hot].onHover();
        }

        // Press start
        if (pressedEdge) {
            if (m_hot >= 0 && m_buttons[m_hot].enabled) {
                m_active = m_hot;
                consumesPointer = true; // prevent world picking/drag this frame
                if (m_buttons[m_active].onPressed) m_buttons[m_active].onPressed();
            }
            else {
                m_active = -1;
            }
        }

        // Release
        if (releasedEdge) {
            if (m_active >= 0 && m_active < static_cast<int>(m_buttons.size())) {
                if (m_buttons[m_active].onReleased) m_buttons[m_active].onReleased();
                if (m_active == m_hot && m_buttons[m_active].enabled) {
                    if (m_buttons[m_active].onClicked) m_buttons[m_active].onClicked();
                }
            }
            m_active = -1;
        }

        // Update visual states
        for (auto& b : m_buttons) {
            if (!b.enabled) { b.state = ButtonState::Disabled; continue; }
            if (b.id == m_active && isDown) { b.state = ButtonState::Pressed; }
            else if (b.id == m_hot) { b.state = ButtonState::Hover; }
            else { b.state = ButtonState::Normal; }
        }
    }

    void UIContext::draw(const UIDrawContext& ctx, float uiScale) const {
        const float borderPx = 2.0f * uiScale;
        const float textPx = 14.0f * uiScale;

        for (const auto& b : m_buttons) {
            if (!b.visible) continue;
            const glm::vec2 min(b.rect.x, b.rect.y);
            const glm::vec2 max(b.rect.x + b.rect.w, b.rect.y + b.rect.h);

            glm::vec4 col;
            switch (b.state) {
            case ButtonState::Normal:   col = colorNormal(); break;
            case ButtonState::Hover:    col = colorHover();  break;
            case ButtonState::Pressed:  col = colorPressed(); break;
            case ButtonState::Disabled: col = colorDisabled(); break;
            }

            if (ctx.rectFill)   ctx.rectFill(min, max, col);
            if (ctx.rectStroke) ctx.rectStroke(min, max, borderPx, glm::vec4(0, 0, 0, 1));

            // Simple label placement: left padding and vertical centering
            if (ctx.drawText) {
                const float padX = 10.0f * uiScale;
                const float padY = (b.rect.h - textPx) * 0.5f;
                glm::vec2 labelPos(b.rect.x + padX, b.rect.y + padY);
                glm::vec4 textColor(1, 1, 1, 1);
                if (b.state == ButtonState::Disabled) textColor = glm::vec4(0.7f, 0.7f, 0.7f, 1);
                ctx.drawText(b.label, labelPos, textPx, textColor);
            }
        }
    }

    glm::vec4 UIContext::colorNormal() { return glm::vec4(0.15f, 0.15f, 0.17f, 0.95f); }
    glm::vec4 UIContext::colorHover() { return glm::vec4(0.22f, 0.22f, 0.27f, 0.95f); }
    glm::vec4 UIContext::colorPressed() { return glm::vec4(0.10f, 0.10f, 0.12f, 0.95f); }
    glm::vec4 UIContext::colorDisabled() { return glm::vec4(0.15f, 0.15f, 0.15f, 0.60f); }

} // namespace UI
