/* Start Header *****************************************************************/
/*!
\file    GUIRenderCommandBuffer.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Defines a lightweight render command buffer for GUI rendering.

The buffer decouples GUI logic from the renderer by collecting draw commands
in a stable order, which are then submitted in a later rendering pass.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_RENDER_COMMAND_BUFFER_H
#define GUI_RENDER_COMMAND_BUFFER_H

#include "Color.h"
#include "ecs/Entity.h"
#include "math/Vector2D.h"
#include <cstdint>
#include <vector>

namespace ECS {
    namespace UI {

        enum class GUIRenderCommandType : uint8_t {
            Panel,
            Text,
            Slider,
            Checkbox,
            Line
        };

        struct GUIRenderCommand {
            GUIRenderCommandType Type = GUIRenderCommandType::Panel;
            Vector2D Position{0.0f, 0.0f};
            Vector2D Size{0.0f, 0.0f};
            Vector2D End{0.0f, 0.0f};
            Vector2D Offset{0.0f, 0.0f};
            Vector2D ClipPosition{0.0f, 0.0f};
            Vector2D ClipSize{0.0f, 0.0f};
            Color PrimaryColor{1.0f, 1.0f, 1.0f, 1.0f};
            Color SecondaryColor{0.0f, 0.0f, 0.0f, 0.0f};
            Color TertiaryColor{0.0f, 0.0f, 0.0f, 0.0f};
            float ScalarA = 0.0f;
            float ScalarB = 0.0f;
            uint32_t TextId = 0;
            uint32_t FontId = 0;
            bool Flag = false;
            bool ClipEnabled = false;
        };

        class GUIRenderCommandBuffer {
        public:
            void Clear() { m_commands.clear(); }

            const std::vector<GUIRenderCommand>& Commands() const { return m_commands; }

            void SubmitPanel(Vector2D position, Vector2D size, const Color& color, float radius,
                             bool clipEnabled = false, Vector2D clipPos = {0.0f, 0.0f},
                             Vector2D clipSize = {0.0f, 0.0f}) {
                GUIRenderCommand cmd{};
                cmd.Type = GUIRenderCommandType::Panel;
                cmd.Position = position;
                cmd.Size = size;
                cmd.PrimaryColor = color;
                cmd.ScalarA = radius;
                cmd.ClipEnabled = clipEnabled;
                cmd.ClipPosition = clipPos;
                cmd.ClipSize = clipSize;
                m_commands.push_back(cmd);
            }

            void SubmitText(uint32_t fontId, uint32_t textId, Vector2D position,
                            const Color& color, float fontSize,
                            const Color& shadowColor, Vector2D shadowOffset,
                            bool clipEnabled = false, Vector2D clipPos = {0.0f, 0.0f},
                            Vector2D clipSize = {0.0f, 0.0f}) {
                GUIRenderCommand cmd{};
                cmd.Type = GUIRenderCommandType::Text;
                cmd.Position = position;
                cmd.PrimaryColor = color;
                cmd.SecondaryColor = shadowColor;
                cmd.Offset = shadowOffset;
                cmd.ScalarA = fontSize;
                cmd.FontId = fontId;
                cmd.TextId = textId;
                cmd.ClipEnabled = clipEnabled;
                cmd.ClipPosition = clipPos;
                cmd.ClipSize = clipSize;
                m_commands.push_back(cmd);
            }

            void SubmitSlider(Vector2D position, Vector2D size, float normalizedValue,
                              const Color& backgroundColor, const Color& handleColor,
                              bool clipEnabled = false, Vector2D clipPos = {0.0f, 0.0f},
                              Vector2D clipSize = {0.0f, 0.0f}) {
                GUIRenderCommand cmd{};
                cmd.Type = GUIRenderCommandType::Slider;
                cmd.Position = position;
                cmd.Size = size;
                cmd.PrimaryColor = backgroundColor;
                cmd.SecondaryColor = handleColor;
                cmd.ScalarA = normalizedValue;
                cmd.ClipEnabled = clipEnabled;
                cmd.ClipPosition = clipPos;
                cmd.ClipSize = clipSize;
                m_commands.push_back(cmd);
            }

            void SubmitCheckbox(Vector2D position, Vector2D size, bool checked,
                                const Color& boxColor, const Color& checkColor,
                                const Color& borderColor,
                                bool clipEnabled = false, Vector2D clipPos = {0.0f, 0.0f},
                                Vector2D clipSize = {0.0f, 0.0f}) {
                GUIRenderCommand cmd{};
                cmd.Type = GUIRenderCommandType::Checkbox;
                cmd.Position = position;
                cmd.Size = size;
                cmd.PrimaryColor = boxColor;
                cmd.SecondaryColor = checkColor;
                cmd.TertiaryColor = borderColor;
                cmd.Flag = checked;
                cmd.ClipEnabled = clipEnabled;
                cmd.ClipPosition = clipPos;
                cmd.ClipSize = clipSize;
                m_commands.push_back(cmd);
            }

            void SubmitLine(Vector2D start, Vector2D end, const Color& color, float thickness,
                            bool clipEnabled = false, Vector2D clipPos = {0.0f, 0.0f},
                            Vector2D clipSize = {0.0f, 0.0f}) {
                GUIRenderCommand cmd{};
                cmd.Type = GUIRenderCommandType::Line;
                cmd.Position = start;
                cmd.End = end;
                cmd.PrimaryColor = color;
                cmd.ScalarA = thickness;
                cmd.ClipEnabled = clipEnabled;
                cmd.ClipPosition = clipPos;
                cmd.ClipSize = clipSize;
                m_commands.push_back(cmd);
            }

        private:
            std::vector<GUIRenderCommand> m_commands;
        };

    } // namespace UI
} // namespace ECS

#endif
