/* Start Header *****************************************************************/
/*!
\file    GUIStyleRegistry.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Defines a registry for named GUI styles.

Styles are stored by StringTable ID and can override colors or fonts
when rendering GUI elements.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_STYLE_REGISTRY_H
#define GUI_STYLE_REGISTRY_H

#include "Color.h"
#include "math/Vector2D.h"
#include <cstdint>
#include <unordered_map>

namespace ECS {
    namespace UI {

        struct GUIStyle {
            bool HasPanelColor = false;
            Color PanelColor{0.2f, 0.2f, 0.2f, 1.0f};

            bool HasTextColor = false;
            Color TextColor{1.0f, 1.0f, 1.0f, 1.0f};

            bool HasFontPath = false;
            uint32_t FontPath = 0;

            bool HasTextFontSize = false;
            float TextFontSize = 16.0f;

            bool HasTextShadow = false;
            Color TextShadowColor{0.0f, 0.0f, 0.0f, 0.0f};
            Vector2D TextShadowOffset{0.0f, 0.0f};

            bool HasButtonColors = false;
            Color ButtonNormal{0.3f, 0.3f, 0.3f, 1.0f};
            Color ButtonHovered{0.4f, 0.4f, 0.4f, 1.0f};
            Color ButtonPressed{0.2f, 0.2f, 0.2f, 1.0f};
            Color ButtonDisabled{0.15f, 0.15f, 0.15f, 0.5f};

            bool HasInputColors = false;
            Color InputBackground{0.1f, 0.1f, 0.1f, 1.0f};
            Color InputText{1.0f, 1.0f, 1.0f, 1.0f};
            Color InputPlaceholder{0.7f, 0.7f, 0.7f, 0.6f};

            bool HasSliderColors = false;
            Color SliderBackground{0.2f, 0.2f, 0.2f, 1.0f};
            Color SliderHandle{0.5f, 0.9f, 1.0f, 1.0f};

            bool HasCheckboxColors = false;
            Color CheckboxChecked{0.2f, 0.8f, 0.2f, 1.0f};
            Color CheckboxUnchecked{0.3f, 0.3f, 0.3f, 1.0f};
            Color CheckboxBorder{0.0f, 0.0f, 0.0f, 1.0f};

            bool HasDropdownColors = false;
            Color DropdownBackground{0.2f, 0.2f, 0.2f, 1.0f};
            Color DropdownHighlight{0.4f, 0.6f, 1.0f, 1.0f};
        };

        class GUIStyleRegistry {
        public:
            static void RegisterStyle(uint32_t styleId, const GUIStyle& style) {
                Styles()[styleId] = style;
            }

            static const GUIStyle* GetStyle(uint32_t styleId) {
                auto& styles = Styles();
                const auto it = styles.find(styleId);
                return it != styles.end() ? &it->second : nullptr;
            }

        private:
            static std::unordered_map<uint32_t, GUIStyle>& Styles() {
                static std::unordered_map<uint32_t, GUIStyle> styles;
                return styles;
            }
        };

    } // namespace UI
} // namespace ECS

#endif
