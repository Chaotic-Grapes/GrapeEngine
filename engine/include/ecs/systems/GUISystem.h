/* Start Header *****************************************************************/
/*!
\file    GUISystem.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Declares the GUISystem for managing GUI elements in the ECS framework.

The GUISystem is responsible for:
- Layout calculation and positioning of GUI elements
- Input handling (mouse/touch interaction with GUI elements)
- State management for interactive elements (buttons, sliders, etc.)
- Rendering of GUI elements through the renderer
- Tooltip and context menu management
- Focus management for input fields

The system operates as a standard ECS system and can be registered with SystemManager.
It processes GUI components in two phases:
1. Layout Update: Calculates world positions and sizes of all GUI elements
2. Interaction Update: Handles input and state changes
3. Rendering: Submits geometry to the renderer

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_SYSTEM_H
#define GUI_SYSTEM_H

#include "Export.h"
#include "ecs/ISystem.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include "ecs/ui/GUIEventQueue.h"
#include "ecs/ui/GUIRenderCommandBuffer.h"
#include "ecs/ui/GUIRuntimeState.h"
#include "ecs/ui/GUIStringCache.h"
#include "math/Vector2D.h"
#include "Color.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>

namespace ECS {

    // Avoid forward-declaring Renderer/Shader inside ECS namespace; use graphics types instead
    class RendererSystem;

    /**
     * @brief Comprehensive GUI System for ECS-based UI management
     * 
     * Responsibilities:
     * - Layout calculation (hierarchical positioning and sizing)
     * - Input event handling (mouse clicks, hovers, text input)
     * - State management for interactive GUI elements
     * - Rendering of all GUI components
     * - Dialog and modal window support
     * - Tooltip management
     * - Focus and drag tracking
     * 
     * Usage:
     * @code
     * // In engine initialization
     * GUISystem* guiSystem = systemManager.RegisterSystem<GUISystem>();
     * 
     * // Create a GUI element in your scene
     * Entity panel = world.Create();
     * auto& guiElement = world.Emplace<ECS::Components::GUIElement>(panel);
     * guiElement.Position = {100, 100};
     * guiElement.Size = {300, 200};
     * 
     * auto& guiPanel = world.Emplace<ECS::Components::GUIPanel>(panel);
     * guiPanel.BackgroundColor = Color(0.2f, 0.2f, 0.2f, 1.0f);
     * 
     * // System automatically handles layout, input, and rendering
     * @endcode
     */
    class GRAPEENGINE_API GUISystem : public ISystem {
    public:
        GUISystem() = default;
        ~GUISystem() override = default;

        // ====================================================================
        // ISystem Interface
        // ====================================================================
        
        void OnCreate(World& world) override;
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override;
        
        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::PreRender; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }

        // ====================================================================
        // Public GUI Management API
        // ====================================================================

        /**
         * @brief Register a GUI action callback
         * @param actionID Unique action identifier
         * @param callback Function to call when action is triggered
         */
        void RegisterAction(uint32_t actionID, std::function<void(World&, Entity)> callback);

        /**
         * @brief Unregister a GUI action
         * @param actionID The action to remove
         */
        void UnregisterAction(uint32_t actionID);

        /**
         * @brief Get the currently focused GUI element (for input fields)
         * @return Entity of focused element, or null if none
         */
        Entity GetFocusedElement() const { return m_focusedElement; }

        /**
         * @brief Set focus to a specific GUI element
         * @param entity The element to focus
         */
        void SetFocus(Entity entity) { m_focusedElement = entity; }

        /**
         * @brief Clear focus from all elements
         */
        void ClearFocus() { m_focusedElement = NULL_ENTITY; }

        /**
         * @brief Get the currently hovered element
         * @return Entity of hovered element, or null if none
         */
        Entity GetHoveredElement() const { return m_hoveredElement; }

        /**
         * @brief Access GUI events captured this frame
         */
        const UI::GUIEventQueue& GetEventQueue() const { return m_eventQueue; }

        /**
         * @brief Show a tooltip at a specific location
         * @param text Tooltip text
         * @param position Screen position
         * @param duration How long to show (0 = indefinite)
         */
        void ShowTooltip(const std::string& text, Vector2D position, float duration = 5.0f);

        /**
         * @brief Hide the current tooltip
         */
        void HideTooltip();

        /**
         * @brief Request a modal dialog
         * Dialog will be rendered on top of all other elements and blocks interaction with other elements
         * @param entity Entity to show as modal
         */
        void ShowModal(Entity entity);

        /**
         * @brief Close a modal dialog
         * @param entity The modal entity to close
         */
        void CloseModal(Entity entity);

        /**
         * @brief Check if mouse is over any GUI element
         * @return true if pointer is over any interactive GUI element
         */
        bool IsMouseOverGUI() const { return !m_hoveredElement.IsNull(); }

        /**
         * @brief Check if a specific element is being hovered
         * @param entity Entity to check
         * @return true if entity is currently hovered
         */
        bool IsElementHovered(Entity entity) const { return m_hoveredElement == entity; }

        /**
         * @brief Check if a specific element is being clicked/pressed
         * @param entity Entity to check
         * @return true if entity is currently being pressed by mouse
         */
        bool IsElementPressed(Entity entity) const;

        /**
         * @brief Check if a specific element is focused (for input fields)
         * @param entity Entity to check
         * @return true if entity has input focus
         */
        bool IsElementFocused(Entity entity) const { return m_focusedElement == entity; }

        /**
         * @brief Check if any GUI element should consume input
         * When true, world input should be blocked (camera movement, etc.)
         * @return true if GUI is active and consuming input
         */
        bool ShouldBlockInput() const;

        /**
         * @brief Get the entity under mouse cursor using spatial partitioning
         * @param mousePos Screen space mouse position
         * @return Entity under cursor, or NULL_ENTITY if none
         */
        Entity GetElementAtPoint(Vector2D mousePos);

        /**
         * @brief Set GUI canvas size (screen resolution)
         * @param width Canvas width in pixels
         * @param height Canvas height in pixels
         */
        void SetCanvasSize(float width, float height);

        /**
         * @brief Get current canvas size
         * @return Canvas size {width, height}
         */
        Vector2D GetCanvasSize() const { return m_canvasSize; }

        // ====================================================================
        // RendererSystem Access
        // ====================================================================

        /**
         * @brief Get the RendererSystem for submitting GUI rendering commands
         * @param world The ECS world
         * @return Pointer to RendererSystem, or nullptr if not available
         */
        RendererSystem* GetRendererSystem(World& world) const;

        // ====================================================================
        // Internal Methods (called by system during update)
        // ====================================================================

    private:

        /**
         * @brief Update layout for all GUI elements
         * Calculates world positions and sizes based on anchoring and layout groups
         */
        void UpdateLayout(World& world);

        /**
         * @brief Update world position for a single element and its children
         */
        void UpdateElementLayout(World& world, Entity entity, 
                                Vector2D parentPos, Vector2D parentSize);

        /**
         * @brief Calculate layout for a container's children
         */
        void CalculateContainerLayout(World& world, Entity containerEntity,
                                     const Components::GUIElement& containerElement,
                                     const Components::GUIContainer& container);

        /**
         * @brief Handle all input events (mouse, keyboard, touch)
         */
        void HandleInput(World& world, float deltaTime);

        /**
         * @brief Update interactive element states (buttons, sliders, etc.)
         */
        void UpdateInteraction(World& world, float deltaTime);

        /**
         * @brief Render all GUI elements
         */
        void RenderGUI(World& world);

        /**
         * @brief Render a specific GUI element
         */
        void RenderElement(World& world, Entity entity);

        /**
         * @brief Submit button rendering to RendererSystem
         */
        void RenderButton(Entity entity, const Components::GUIElement& element,
                         const Components::GUIButton& button);

        /**
         * @brief Submit panel rendering to RendererSystem
         */
        void RenderPanel(const Components::GUIElement& element,
                        const Components::GUIPanel& panel);

        /**
         * @brief Submit text rendering to RendererSystem
         */
        void RenderText(Entity entity, const Components::GUIElement& element,
                       const Components::GUIText& text);

        /**
         * @brief Submit slider rendering to RendererSystem
         */
        void RenderSlider(const Components::GUIElement& element,
                         const Components::GUISlider& slider);

        /**
         * @brief Submit checkbox rendering to RendererSystem
         */
        void RenderCheckbox(const Components::GUIElement& element,
                           const Components::GUICheckbox& checkbox);

        /**
         * @brief Submit dropdown rendering to RendererSystem
         */
        void RenderDropdown(const Components::GUIElement& element,
                           const Components::GUIDropdown& dropdown);

        /**
         * @brief Submit separator rendering to RendererSystem
         */
        void RenderSeparator(const Components::GUIElement& element,
                            const Components::GUISeparator& separator);

        /**
         * @brief Submit buffered GUI commands to RendererSystem
         */
        void FlushRenderCommands(RendererSystem* rendererSystem);

        /**
         * @brief Check if a point is inside a GUI element's bounds
         */
        bool IsPointInElement(Vector2D point, const Components::GUIElement& element) const;

        /**
         * @brief Raycast to find topmost GUI element at position
         */
        Entity RaycastGUI(World& world, Vector2D point, Entity skipEntity = NULL_ENTITY);

        /**
         * @brief Update button state based on interaction
         */
        void UpdateButtonState(World& world, Entity entity, Components::GUIButton& button,
                              bool mouseOver, bool mousePressed);

        /**
         * @brief Update slider interaction
         */
        void UpdateSliderInteraction(World& world, Entity entity, Components::GUISlider& slider,
                                    const Components::GUIElement& element,
                                    bool mouseOver, Vector2D mousePos);

        /**
         * @brief Update text input field
         */
        void UpdateInputField(Entity entity, Components::GUIInputField& input,
                             bool focused, char inputChar);

        /**
         * @brief Validate input field text
         */
        bool ValidateInputField(const Components::GUIInputField& input, 
                               const std::string& text) const;

        /**
         * @brief Update scroll view
         */
        void UpdateScrollView(Entity entity, Components::GUIScrollView& scroll,
                             const Components::GUIElement& element,
                             World& world);

        /**
         * @brief Sort GUI elements by Z order
         */
        std::vector<Entity> GetSortedGUIElements(World& world);

        /**
         * @brief Build spatial partitioning grid for hit-testing acceleration
         */
        void BuildSpatialGrid(World& world);

        /**
         * @brief Apply visual feedback states (hover/focus/active colors/scales)
         */
        void ApplyVisualFeedback(World& world, RendererSystem* rendererSystem);

        /**
         * @brief Check if any modal is active
         */
        bool HasActiveModal() const { return !m_modals.empty(); }

        // ====================================================================
        // Member Variables
        // ====================================================================

        // Action registry
        std::unordered_map<uint32_t, std::function<void(World&, Entity)>> m_actionRegistry;

        // Per-frame GUI data
        UI::GUIEventQueue m_eventQueue;
        UI::GUIRenderCommandBuffer m_renderCommandBuffer;
        UI::GUIRuntimeStateMap m_runtimeStates;
        UI::GUIStringCache m_stringCache;

        // Input and focus state
        Entity m_focusedElement{ NULL_ENTITY };
        Entity m_hoveredElement{ NULL_ENTITY };
        Entity m_draggedElement{ NULL_ENTITY };
        Entity m_draggedScrollView{ NULL_ENTITY };
        Entity m_pressedElement{ NULL_ENTITY };  // Currently pressed element

        // Canvas size
        Vector2D m_canvasSize{ 1920.0f, 1080.0f };

        // Mouse state tracking
        Vector2D m_mousePosition{ 0.0f, 0.0f };
        Vector2D m_mouseDragStart{ 0.0f, 0.0f };
        bool m_mouseDown = false;
        bool m_mousePressed = false;
        bool m_mouseReleased = false;

        // Tooltip management
        struct TooltipData {
            std::string Text;
            Vector2D Position{ 0.0f, 0.0f };
            float Timer = 0.0f;
            float Duration = 0.0f;
            bool Visible = false;
        } m_tooltip;

        // Modal dialogs (rendered on top, block interaction with background)
        std::vector<Entity> m_modals;

        // Text input state
        std::string m_inputBuffer;
        uint32_t m_inputCaretPosition = 0;

        // Render data (cached between frames)
        Vector2D m_lastMousePos{ 0.0f, 0.0f };
        float m_lastInteractionTime = 0.0f;

        // Double-click tracking
        float m_doubleClickThreshold = 0.3f;
        float m_lastClickTime = 0.0f;
        Entity m_lastClickedElement{ NULL_ENTITY };

        // ====================================================================
        // Spatial Partitioning Grid
        // ====================================================================
        
        /**
         * @brief Spatial grid cell for accelerated hit-testing
         */
        struct SpatialGridCell {
            std::vector<Entity> elements;  // Entities in this cell
        };

        // Spatial grid parameters
        static constexpr uint32_t GRID_WIDTH = 10;   // 10x10 grid cells
        static constexpr uint32_t GRID_HEIGHT = 10;
        SpatialGridCell m_spatialGrid[GRID_WIDTH][GRID_HEIGHT];
        bool m_spatialGridDirty = true;  // Rebuild on next update

        // ====================================================================
        // Visual Feedback State
        // ====================================================================

        /**
         * @brief Visual feedback configuration for UI elements
         */
        struct VisualFeedbackConfig {
            // Scale modifiers
            float hoverScale = 1.05f;       // 5% scale on hover
            float activeScale = 0.98f;      // 2% scale reduction when pressed
            float disabledAlpha = 0.5f;     // 50% alpha when disabled

            // Color overlays
            Color hoverOverlay{255U, 255U, 255U, 30U};    // White 30 alpha
            Color focusOverlay{100U, 150U, 255U, 40U};    // Blue 40 alpha
            Color activeOverlay{200U, 200U, 200U, 50U};   // Gray 50 alpha
            
            // Transition speed
            float transitionSpeed = 10.0f;  // Lerp speed for smooth feedback
            bool enableSmoothTransitions = true;
        } m_visualFeedbackConfig;

        /**
         * @brief Per-element visual feedback state
         */
        struct ElementVisualState {
            float currentScale = 1.0f;
            float targetScale = 1.0f;
            Color overlayColor{0U, 0U, 0U, 0U};
            bool hasOverlay = false;
        };
        std::unordered_map<uint32_t, ElementVisualState> m_visualStates;
    };

} // namespace ECS

#endif
