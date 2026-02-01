/* Start Header *****************************************************************/
/*!
\file    GUIContext.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Defines shared GUI runtime state for layout, input, and rendering systems.

This context holds per-frame GUI state that should not be serialized.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_CONTEXT_H
#define GUI_CONTEXT_H

#include "Color.h"
#include "ecs/Entity.h"
#include "ecs/ui/GUIEventQueue.h"
#include "ecs/ui/GUIRenderCommandBuffer.h"
#include "ecs/ui/GUIRuntimeState.h"
#include "ecs/ui/GUIStringCache.h"
#include "math/Vector2D.h"
#include <functional>
#include <unordered_map>
#include <vector>

namespace ECS {
    class World;

    namespace UI {

        class GUIContext {
        public:
            struct TooltipData {
                std::string Text;
                Vector2D Position{0.0f, 0.0f};
                float Timer = 0.0f;
                float Duration = 0.0f;
                bool Visible = false;
            };

            struct SpatialGridCell {
                std::vector<Entity> Elements;
            };

            struct VisualFeedbackConfig {
                float HoverScale = 1.05f;
                float ActiveScale = 0.98f;
                float DisabledAlpha = 0.5f;
                Color HoverOverlay{255U, 255U, 255U, 30U};
                Color FocusOverlay{100U, 150U, 255U, 40U};
                Color ActiveOverlay{200U, 200U, 200U, 50U};
                float TransitionSpeed = 10.0f;
                bool EnableSmoothTransitions = true;
            };

            struct ElementVisualState {
                float CurrentScale = 1.0f;
                float TargetScale = 1.0f;
                Color OverlayColor{0U, 0U, 0U, 0U};
                bool HasOverlay = false;
            };

            static constexpr uint32_t GridWidth = 10;
            static constexpr uint32_t GridHeight = 10;

            static GUIContext& Get() {
                static GUIContext context;
                return context;
            }

            void RegisterAction(uint32_t actionId, std::function<void(World&, Entity)> callback) {
                ActionRegistry[actionId] = std::move(callback);
            }

            void UnregisterAction(uint32_t actionId) {
                ActionRegistry.erase(actionId);
            }

            void ResetFrame() {
                EventQueue.Clear();
                RenderCommands.Clear();
                StringCache.Clear();
                for (auto& [id, state] : RuntimeStates) {
                    state.Hovered = false;
                    state.Pressed = false;
                    state.Released = false;
                }
            }

            // Shared runtime data
            GUIEventQueue EventQueue;
            GUIRenderCommandBuffer RenderCommands;
            GUIRuntimeStateMap RuntimeStates;
            GUIStringCache StringCache;

            std::unordered_map<uint32_t, std::function<void(World&, Entity)>> ActionRegistry;
            std::unordered_map<uint32_t, size_t> LayoutHashes;

            Entity FocusedElement{ NULL_ENTITY };
            Entity HoveredElement{ NULL_ENTITY };
            Entity DraggedElement{ NULL_ENTITY };
            Entity DraggedScrollView{ NULL_ENTITY };
            Entity PressedElement{ NULL_ENTITY };

            Vector2D CanvasSize{ 1920.0f, 1080.0f };
            Vector2D LayoutCanvasSize{ 1920.0f, 1080.0f };
            Vector2D CanvasOffset{ 0.0f, 0.0f };
            float CanvasScale = 1.0f;
            Vector2D MousePosition{ 0.0f, 0.0f };
            Vector2D MouseDragStart{ 0.0f, 0.0f };
            bool MouseDown = false;
            bool MousePressed = false;
            bool MouseReleased = false;

            TooltipData Tooltip;
            std::vector<Entity> Modals;

            std::string InputBuffer;
            uint32_t InputCaretPosition = 0;

            Vector2D LastMousePos{ 0.0f, 0.0f };
            float LastInteractionTime = 0.0f;
            float DoubleClickThreshold = 0.3f;
            float LastClickTime = 0.0f;
            Entity LastClickedElement{ NULL_ENTITY };

            SpatialGridCell SpatialGrid[GridWidth][GridHeight];
            bool SpatialGridDirty = true;

            VisualFeedbackConfig VisualConfig;
            std::unordered_map<uint32_t, ElementVisualState> VisualStates;

        private:
            GUIContext() = default;
        };

    } // namespace UI
} // namespace ECS

#endif
