/* Start Header *****************************************************************/
/*!
\file    GUIComponents.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Declares ECS-compatible GUI component structures for the comprehensive GUI framework.
These components follow the ECS pattern: pure data structures that are trivially copyable.

The GUI system is hierarchical and layout-based:
- GUIElement: Base component for all GUI elements (positioning, sizing, visibility)
- GUIContainer: Composite component for layout management
- GUIButton: Interactive button with state tracking
- GUIPanel: Container with styling options
- GUIText: Text rendering with anchoring
- GUIInputField: Text input with validation
- GUISlider: Range input element
- GUICheckbox: Boolean toggle
- GUIDropdown: Selection list
- GUIScrollView: Scrollable container

All components are POD structures with fixed-size buffers to maintain
trivially_copyable status for ECS performance requirements.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_COMPONENTS_H
#define GUI_COMPONENTS_H

#include "Color.h"
#include "math/Vector2D.h"
#include <cstdint>
#include <cstring>

namespace ECS {
    namespace GUI {
        
        // ============================================================================
        // Enums for GUI configuration
        // ============================================================================

        enum class HorizontalAlignment : uint8_t {
            Left = 0,
            Center = 1,
            Right = 2,
            Stretch = 3
        };

        enum class VerticalAlignment : uint8_t {
            Top = 0,
            Middle = 1,
            Bottom = 2,
            Stretch = 3
        };

        enum class LayoutType : uint8_t {
            Absolute = 0,      // Fixed position and size
            HorizontalBox = 1,  // Children arranged horizontally
            VerticalBox = 2,    // Children arranged vertically
            Grid = 3,           // Grid layout
            Docking = 4         // Docking layout (future)
        };

        enum class ButtonState : uint8_t {
            Normal = 0,
            Hovered = 1,
            Pressed = 2,
            Disabled = 3
        };

        enum class GUIElementType : uint8_t {
            Custom = 0,
            Container = 1,
            Button = 2,
            Panel = 3,
            Text = 4,
            Image = 5,
            InputField = 6,
            Slider = 7,
            Checkbox = 8,
            Dropdown = 9,
            ScrollView = 10,
            Separator = 11
        };

        // ============================================================================
        // Core GUI Components
        // ============================================================================

        /**
         * @brief Base component for all GUI elements
         * Provides positioning, sizing, visibility, and anchoring
         */
        struct GUIElement {
            // Layout and positioning
            Vector2D Position{ 0.0f, 0.0f };      // Local position (relative to parent)
            Vector2D Size{ 100.0f, 100.0f };      // Width and height in pixels
            Vector2D AnchorMin{ 0.0f, 0.0f };     // Anchor point (0,0)=TopLeft, (1,1)=BottomRight
            Vector2D AnchorMax{ 0.0f, 0.0f };     // Anchor max for stretching
            Vector2D Offset{ 0.0f, 0.0f };        // Offset from anchor point
            
            // Visibility and interaction
            bool Active = true;                   // Whether element is rendered and interactive
            bool Visible = true;                  // Whether element is visible (but may still receive input)
            bool Raycast = true;                  // Whether element blocks raycasts from other elements
            
            // Layout information
            HorizontalAlignment HAlign = HorizontalAlignment::Left;
            VerticalAlignment VAlign = VerticalAlignment::Top;
            GUIElementType ElementType = GUIElementType::Custom;
            
            // Padding and margins
            float PaddingLeft = 0.0f;
            float PaddingRight = 0.0f;
            float PaddingTop = 0.0f;
            float PaddingBottom = 0.0f;
            
            // Z-order for depth sorting
            int16_t ZOrder = 0;
            
            // Cached world position (computed by layout system)
            Vector2D WorldPosition{ 0.0f, 0.0f };
            bool DirtyLayout = true;              // Marks that layout needs recomputation
        };
        static_assert(std::is_trivially_copyable_v<GUIElement>, "GUIElement must be trivially copyable");

        /**
         * @brief Container component for managing child elements
         * Supports various layout types (Box, Grid, etc.)
         */
        struct GUIContainer {
            static constexpr size_t MaxChildren = 256;
            
            LayoutType Layout = LayoutType::VerticalBox;
            
            // Box layout properties
            float Spacing = 5.0f;                 // Space between children
            bool ChildForceExpandWidth = false;   // Make children expand to fill width
            bool ChildForceExpandHeight = false;  // Make children expand to fill height
            
            // Grid layout properties
            uint32_t GridColumns = 1;
            float GridCellPaddingX = 0.0f;
            float GridCellPaddingY = 0.0f;
            
            // Child entity IDs (0 = invalid/empty slot)
            uint32_t Children[MaxChildren] = { 0 };
            uint16_t ChildCount = 0;
            
            // Parent entity ID (0 = no parent)
            uint32_t Parent = 0;
            
            // Layout size calculation preferences
            bool PreferredWidthDynamic = false;   // Calculate width from content
            bool PreferredHeightDynamic = false;  // Calculate height from content
            float MinWidth = 0.0f;
            float MinHeight = 0.0f;
        };
        static_assert(std::is_trivially_copyable_v<GUIContainer>, "GUIContainer must be trivially copyable");

        /**
         * @brief Panel component for styled containers
         * Provides background color, borders, and shadow effects
         */
        struct GUIPanel {
            Color BackgroundColor{ 0.2f, 0.2f, 0.2f, 1.0f };
            Color BorderColor{ 0.0f, 0.0f, 0.0f, 1.0f };
            
            float BorderThickness = 0.0f;         // 0 = no border
            float BorderRadius = 0.0f;            // Rounded corners (0 = sharp)
            
            // Shadow
            bool CastShadow = false;
            Color ShadowColor{ 0.0f, 0.0f, 0.0f, 0.3f };
            Vector2D ShadowOffset{ 2.0f, -2.0f };
            float ShadowBlur = 4.0f;
            
            // Clipping
            bool ClipContent = false;             // Clip children to panel bounds
        };
        static_assert(std::is_trivially_copyable_v<GUIPanel>, "GUIPanel must be trivially copyable");

        /**
         * @brief Button component with state and interaction
         */
        struct GUIButton {
            ButtonState State = ButtonState::Normal;
            
            // Colors for different states
            Color ColorNormal{ 0.3f, 0.3f, 0.3f, 1.0f };
            Color ColorHovered{ 0.4f, 0.4f, 0.4f, 1.0f };
            Color ColorPressed{ 0.2f, 0.2f, 0.2f, 1.0f };
            Color ColorDisabled{ 0.15f, 0.15f, 0.15f, 0.5f };
            
            // Button properties
            bool Interactable = true;
            bool Pressed = false;                 // True if pressed this frame
            bool Released = false;                // True if released this frame
            bool Hovered = false;
            
            // Action callback ID (maps to GUISystem's action registry)
            uint32_t ActionID = 0;
            
            // Button label (for convenience, usually separate Text component)
            char Label[64] = { 0 };
            
            // Transition effects
            float TransitionDuration = 0.1f;      // Time to transition between states
            float TransitionTimer = 0.0f;         // Current transition time
        };
        static_assert(std::is_trivially_copyable_v<GUIButton>, "GUIButton must be trivially copyable");

        /**
         * @brief Text input field component
         */
        struct GUIInputField {
            static constexpr size_t MaxTextLength = 1024;
            
            char Content[MaxTextLength] = { 0 };
            char Placeholder[256] = { 0 };
            
            Color TextColor{ 1.0f, 1.0f, 1.0f, 1.0f };
            Color BackgroundColor{ 0.1f, 0.1f, 0.1f, 1.0f };
            Color CaretColor{ 1.0f, 1.0f, 1.0f, 1.0f };
            Color SelectionColor{ 0.2f, 0.5f, 1.0f, 0.5f };
            
            float FontSize = 16.0f;
            char FontPath[128] = { 0 };
            
            uint32_t MaxCharacters = 0;           // 0 = unlimited
            uint32_t CurrentCharCount = 0;
            
            bool Focused = false;
            bool Interactable = true;
            
            uint32_t CaretPosition = 0;
            uint32_t SelectionStart = 0;
            uint32_t SelectionEnd = 0;
            
            // Input validation
            bool MultiLine = false;
            bool PasswordMode = false;
            
            enum class InputType : uint8_t {
                Standard = 0,
                Integer = 1,
                Decimal = 2,
                Password = 3,
                Alphanumeric = 4
            } Type = InputType::Standard;
        };
        static_assert(std::is_trivially_copyable_v<GUIInputField>, "GUIInputField must be trivially copyable");

        /**
         * @brief Slider component for continuous value selection
         */
        struct GUISlider {
            float MinValue = 0.0f;
            float MaxValue = 100.0f;
            float CurrentValue = 50.0f;
            float StepSize = 1.0f;
            
            // Visual configuration
            Color BackgroundColor{ 0.2f, 0.2f, 0.2f, 1.0f };
            Color FillColor{ 0.4f, 0.8f, 1.0f, 1.0f };
            Color HandleColor{ 0.5f, 0.9f, 1.0f, 1.0f };
            
            float HandleSize = 20.0f;
            bool Interactable = true;
            bool ShowValue = false;
            
            // Callback
            uint32_t ActionID = 0;
            
            // State
            bool Dragging = false;
            float DragOffset = 0.0f;
        };
        static_assert(std::is_trivially_copyable_v<GUISlider>, "GUISlider must be trivially copyable");

        /**
         * @brief Checkbox component for boolean toggling
         */
        struct GUICheckbox {
            bool IsChecked = false;
            bool Interactable = true;
            
            Color CheckedColor{ 0.2f, 0.8f, 0.2f, 1.0f };
            Color UncheckedColor{ 0.3f, 0.3f, 0.3f, 1.0f };
            Color BorderColor{ 0.0f, 0.0f, 0.0f, 1.0f };
            
            float BorderThickness = 1.0f;
            float CheckSize = 20.0f;
            
            // Callback
            uint32_t ActionID = 0;
            
            // Label (usually separate Text component)
            char Label[64] = { 0 };
        };
        static_assert(std::is_trivially_copyable_v<GUICheckbox>, "GUICheckbox must be trivially copyable");

        /**
         * @brief Dropdown/Combo box component
         */
        struct GUIDropdown {
            static constexpr size_t MaxOptions = 64;
            
            // Option strings (newline-separated for simplicity)
            char Options[1024] = { 0 };
            uint32_t OptionCount = 0;
            uint32_t SelectedIndex = 0;
            
            bool IsOpen = false;
            bool Interactable = true;
            
            Color BackgroundColor{ 0.2f, 0.2f, 0.2f, 1.0f };
            Color HighlightColor{ 0.4f, 0.6f, 1.0f, 1.0f };
            
            float ItemHeight = 30.0f;
            float MaxHeight = 200.0f;            // Max height before scrolling
            
            // Callback
            uint32_t ActionID = 0;
            
            // State
            float ScrollPosition = 0.0f;
        };
        static_assert(std::is_trivially_copyable_v<GUIDropdown>, "GUIDropdown must be trivially copyable");

        /**
         * @brief Scroll view component for scrollable content areas
         */
        struct GUIScrollView {
            // Scroll offset (in pixels)
            Vector2D ScrollPosition{ 0.0f, 0.0f };
            
            // Content size (calculated from children)
            Vector2D ContentSize{ 0.0f, 0.0f };
            
            // Scroll bar configuration
            bool HorizontalScroll = false;
            bool VerticalScroll = true;
            float ScrollBarWidth = 10.0f;
            Color ScrollBarColor{ 0.5f, 0.5f, 0.5f, 0.8f };
            Color ScrollBarHoverColor{ 0.7f, 0.7f, 0.7f, 1.0f };
            
            // Scroll behavior
            float ScrollSensitivity = 20.0f;     // Pixels per scroll wheel tick
            float ScrollDamping = 0.95f;         // For smooth scrolling
            bool Inertia = false;                // Enable momentum scrolling
            
            // Clipping
            bool ClipContent = true;
            
            // State
            bool VerticalDragging = false;
            bool HorizontalDragging = false;
            float VerticalScrollVelocity = 0.0f;
            float HorizontalScrollVelocity = 0.0f;
        };
        static_assert(std::is_trivially_copyable_v<GUIScrollView>, "GUIScrollView must be trivially copyable");

        /**
         * @brief Separator/Divider component
         */
        struct GUISeparator {
            enum class Orientation : uint8_t {
                Horizontal = 0,
                Vertical = 1
            } Orient = Orientation::Horizontal;
            
            Color Color{ 0.5f, 0.5f, 0.5f, 1.0f };
            float Thickness = 1.0f;
            
            // Space around separator
            float Margin = 5.0f;
        };
        static_assert(std::is_trivially_copyable_v<GUISeparator>, "GUISeparator must be trivially copyable");

        /**
         * @brief GUI Text component (separate from general Text component)
         * Optimized for GUI rendering with anchoring and alignment
         */
        struct GUIText {
            static constexpr size_t MaxTextLength = 512;
            
            char Content[MaxTextLength] = { 0 };
            char FontPath[128] = { 0 };
            
            float FontSize = 16.0f;
            Color FontColor{ 1.0f, 1.0f, 1.0f, 1.0f };
            
            enum class TextAlignment : uint8_t {
                Left = 0,
                Center = 1,
                Right = 2,
                Justified = 3
            } Alignment = TextAlignment::Left;
            
            bool BestFit = false;                 // Auto-scale font to fit
            float MinFontSize = 10.0f;
            float MaxFontSize = 100.0f;
            
            bool RichText = false;                // Support for color tags, etc.
            bool WordWrap = true;
            
            // Shadow effect
            bool CastShadow = false;
            Color ShadowColor{ 0.0f, 0.0f, 0.0f, 0.3f };
            Vector2D ShadowOffset{ 1.0f, -1.0f };
            
            // Outline
            bool HasOutline = false;
            Color OutlineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
            float OutlineWidth = 1.0f;
        };
        static_assert(std::is_trivially_copyable_v<GUIText>, "GUIText must be trivially copyable");

        /**
         * @brief GUI Layout Group component (for automatic layout calculation)
         * Used by containers to manage child sizing
         */
        struct GUILayoutGroup {
            // Preferred size calculation
            float PreferredWidth = 0.0f;
            float PreferredHeight = 0.0f;
            
            // Flexible size (how much extra space the element can take)
            float FlexibleWidth = 0.0f;
            float FlexibleHeight = 0.0f;
            
            // Layout priority (higher = laid out later, so higher priority elements get priority)
            int LayoutPriority = 0;
            
            // Mark that this element's size has changed
            bool DirtyPreferredSize = true;
        };
        static_assert(std::is_trivially_copyable_v<GUILayoutGroup>, "GUILayoutGroup must be trivially copyable");

        /**
         * @brief GUI Tooltip component
         */
        struct GUITooltip {
            static constexpr size_t MaxLength = 256;
            
            char Text[MaxLength] = { 0 };
            float DelaySeconds = 1.0f;
            float ShowDuration = 5.0f;
            
            bool Visible = false;
            float ShowTimer = 0.0f;
            
            Vector2D Offset{ 10.0f, 10.0f };
            Color BackgroundColor{ 0.1f, 0.1f, 0.1f, 0.9f };
            Color TextColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        };
        static_assert(std::is_trivially_copyable_v<GUITooltip>, "GUITooltip must be trivially copyable");

    } // namespace GUI
} // namespace ECS

#endif
