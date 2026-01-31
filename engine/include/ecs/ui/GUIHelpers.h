/* Start Header *****************************************************************/
/*!
\file    GUIHelpers.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Declares utility functions and helper classes for GUI system operations.

Includes:
- Factory functions for creating common GUI elements
- Conversion utilities between screen and world space
- Color interpolation helpers
- Text measurement and formatting
- Hit testing utilities

These helpers simplify the creation and manipulation of GUI elements
when working with the ECS system directly.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_HELPERS_H
#define GUI_HELPERS_H

#include "ecs/World.h"
#include "ecs/Components.h"
#include "math/Vector2D.h"
#include "Color.h"
#include <string>
#include "Export.h"

namespace ECS {
    namespace UI {

        /**
         * @brief Factory for creating common GUI elements
         */
        class GRAPEENGINE_API GUIFactory {
        public:
            /**
             * @brief Create a basic button
             * @param world The ECS world
             * @param position Button position
             * @param size Button size
             * @param label Button label text
             * @param actionID Action to trigger on click
             * @return Created button entity
             */
            static Entity CreateButton(
                World& world,
                Vector2D position,
                Vector2D size,
                const std::string& label,
                uint32_t actionID = 0,
                const std::string& name = "");

            /**
             * @brief Create a panel
             * @param world The ECS world
             * @param position Panel position
             * @param size Panel size
             * @param backgroundColor Panel background color
             * @return Created panel entity
             */
            static Entity CreatePanel(
                World& world,
                Vector2D position,
                Vector2D size,
                Color backgroundColor = {0.2f, 0.2f, 0.2f, 1.0f},
                const std::string& name = "");
                

            /**
             * @brief Create a label (text only)
             * @param world The ECS world
             * @param position Text position
             * @param text Text content
             * @param fontSize Font size
             * @return Created text entity
             */
            static Entity CreateLabel(
                World& world,
                Vector2D position,
                const std::string& text,
                float fontSize = 16.0f,
                const std::string& name = "");

            /**
             * @brief Create an input field
             * @param world The ECS world
             * @param position Input field position
             * @param size Input field size
             * @param placeholder Placeholder text
             * @return Created input field entity
             */
            static Entity CreateInputField(
                World& world,
                Vector2D position,
                Vector2D size,
                const std::string& placeholder = "",
                const std::string& name = "");

            /**
             * @brief Create a slider
             * @param world The ECS world
             * @param position Slider position
             * @param width Slider width
             * @param minValue Minimum value
             * @param maxValue Maximum value
             * @param initialValue Initial value
             * @param actionID Action to trigger on value change
             * @return Created slider entity
             */
            static Entity CreateSlider(
                World& world,
                Vector2D position,
                float width,
                float minValue = 0.0f,
                float maxValue = 100.0f,
                float initialValue = 50.0f,
                uint32_t actionID = 0,
                const std::string& name = "");

            /**
             * @brief Create a checkbox
             * @param world The ECS world
             * @param position Checkbox position
             * @param label Checkbox label
             * @param actionID Action to trigger on toggle
             * @return Created checkbox entity
             */
            static Entity CreateCheckbox(
                World& world,
                Vector2D position,
                const std::string& label = "",
                uint32_t actionID = 0,
                const std::string& name = "");

            /**
             * @brief Create a dropdown
             * @param world The ECS world
             * @param position Dropdown position
             * @param width Dropdown width
             * @param options Newline-separated option strings
             * @param actionID Action to trigger on selection
             * @return Created dropdown entity
             */
            static Entity CreateDropdown(
                World& world,
                Vector2D position,
                float width,
                const std::string& options,
                uint32_t actionID = 0,
                const std::string& name = "");

            /**
             * @brief Create a container
             * @param world The ECS world
             * @param position Container position
             * @param size Container size
             * @param layoutType Layout type for children
             * @return Created container entity
             */
            static Entity CreateContainer(
                World& world,
                Vector2D position,
                Vector2D size,
                Components::LayoutType layoutType = Components::LayoutType::VerticalBox,
                const std::string& name = "");

            /**
             * @brief Create a separator
             * @param world The ECS world
             * @param position Separator position
             * @param length Separator length
             * @param horizontal True for horizontal, false for vertical
             * @return Created separator entity
             */
            static Entity CreateSeparator(
                World& world,
                Vector2D position,
                float length,
                bool horizontal = true,
                const std::string& name = "");

            /**
             * @brief Attach a child to a GUI parent using ECS components
             * @param world The ECS world
             * @param parent Parent GUI entity
             * @param child Child GUI entity
             */
            static void AttachChild(World& world, Entity parent, Entity child);
        };

        /**
         * @brief Utility class for GUI conversions and hit testing
         */
        class GRAPEENGINE_API GUIUtils {
        public:
            /**
             * @brief Check if a point is inside a rectangle
             * @param point Point to test
             * @param rectPos Rectangle position
             * @param rectSize Rectangle size
             * @return true if point is inside rectangle
             */
            static bool PointInRect(
                Vector2D point,
                Vector2D rectPos,
                Vector2D rectSize);

            /**
             * @brief Check if a point is inside a circle
             * @param point Point to test
             * @param circlePos Circle center
             * @param radius Circle radius
             * @return true if point is inside circle
             */
            static bool PointInCircle(
                Vector2D point,
                Vector2D circlePos,
                float radius);

            /**
             * @brief Get intersection rectangle of two rectangles
             * @param rect1Pos First rectangle position
             * @param rect1Size First rectangle size
             * @param rect2Pos Second rectangle position
             * @param rect2Size Second rectangle size
             * @return Intersection rectangle (size 0,0 if no intersection)
             */
            static std::pair<Vector2D, Vector2D> GetRectIntersection(
                Vector2D rect1Pos, Vector2D rect1Size,
                Vector2D rect2Pos, Vector2D rect2Size);

            /**
             * @brief Lerp between two colors
             * @param colorA Start color
             * @param colorB End color
             * @param t Interpolation factor (0-1)
             * @return Interpolated color
             */
            static Color LerpColor(const Color& colorA, const Color& colorB, float t);

            /**
             * @brief Ease function for smooth transitions
             * @param t Time (0-1)
             * @param easeType Type of easing
             * @return Eased value
             */
            enum class EaseType {
                Linear = 0,
                EaseInQuad = 1,
                EaseOutQuad = 2,
                EaseInOutQuad = 3,
                EaseInCubic = 4,
                EaseOutCubic = 5,
                EaseInOutCubic = 6
            };

            static float Ease(float t, EaseType type = EaseType::Linear);

            /**
             * @brief Convert screen position to canvas position
             * @param screenPos Position in screen space
             * @param canvasSize Size of the GUI canvas
             * @return Position in canvas space
             */
            static Vector2D ScreenToCanvas(Vector2D screenPos, Vector2D canvasSize);

            /**
             * @brief Convert canvas position to screen position
             * @param canvasPos Position in canvas space
             * @param canvasSize Size of the GUI canvas
             * @return Position in screen space
             */
            static Vector2D CanvasToScreen(Vector2D canvasPos, Vector2D canvasSize);
        };

        /**
         * @brief Text rendering utilities
         */
        class GRAPEENGINE_API GUITextUtils {
        public:
            /**
             * @brief Measure text dimensions
             * @param text Text to measure
             * @param fontPath Path to font file
             * @param fontSize Font size
             * @return Text dimensions {width, height}
             */
            static Vector2D MeasureText(
                const std::string& text,
                const std::string& fontPath,
                float fontSize);

            /**
             * @brief Truncate text to fit in a maximum width
             * @param text Text to truncate
             * @param fontPath Path to font file
             * @param fontSize Font size
             * @param maxWidth Maximum width
             * @param suffix Suffix to add if truncated (e.g., "...")
             * @return Truncated text
             */
            static std::string TruncateText(
                const std::string& text,
                const std::string& fontPath,
                float fontSize,
                float maxWidth,
                const std::string& suffix = "...");

            /**
             * @brief Wrap text to multiple lines
             * @param text Text to wrap
             * @param fontPath Path to font file
             * @param fontSize Font size
             * @param maxWidth Maximum line width
             * @return Vector of wrapped lines
             */
            static std::vector<std::string> WrapText(
                const std::string& text,
                const std::string& fontPath,
                float fontSize,
                float maxWidth);

            /**
             * @brief Get text size with best fit (auto-scaled)
             * @param text Text to measure
             * @param fontPath Path to font file
             * @param minFontSize Minimum font size
             * @param maxFontSize Maximum font size
             * @param targetWidth Target width to fit
             * @param targetHeight Target height to fit
             * @return {actualSize, bestFitFontSize}
             */
            static std::pair<Vector2D, float> GetTextSizeWithBestFit(
                const std::string& text,
                const std::string& fontPath,
                float minFontSize,
                float maxFontSize,
                float targetWidth,
                float targetHeight);
        };

        /**
         * @brief Color palette and style utilities
         */
        class GRAPEENGINE_API GUIStyle {
        public:
            // Default colors
            static const Color PrimaryColor;      // Main accent color
            static const Color SecondaryColor;    // Secondary accent
            static const Color BackgroundColor;   // Default background
            static const Color TextColor;         // Default text color
            static const Color BorderColor;       // Default border color
            static const Color HighlightColor;    // Highlight/selection color
            static const Color DisabledColor;     // Disabled element color
            static const Color ErrorColor;        // Error/warning color
            static const Color SuccessColor;      // Success color

            /**
             * @brief Get color for a button in a specific state
             */
            static Color GetButtonColor(Components::ButtonState state);

            /**
             * @brief Get transition color based on time
             */
            static Color GetTransitionColor(
                const Color& startColor,
                const Color& endColor,
                float elapsedTime,
                float totalTime);
        };

    } // namespace UI
} // namespace ECS

#endif
