/* Start Header *****************************************************************/
/*!
\file   AnimationEditorPanel.h
\author Muhammad Nur Fadzly Bin Zulkifli
\brief
Editor panel for animation graph + timeline authoring.
*/
/* End Header *******************************************************************/

#ifndef ANIMATION_EDITOR_PANEL_H
#define ANIMATION_EDITOR_PANEL_H

#include <imgui.h>

class AnimationEditorPanel {
public:
    /**
     * @brief Initializes the animation editor panel with the given fonts.
     * @param mainFont The main font to use for rendering text in the panel.
     * @param boldFont The bold font to use for rendering emphasized text in the panel.
     */
    void Initialize(ImFont* mainFont, ImFont* boldFont);
    
    /**
     * @brief Renders the animation editor panel, including the timeline and any relevant UI elements.
     */
    void Render();

private:
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    float m_timelineCursor = 0.0f;
};

#endif
