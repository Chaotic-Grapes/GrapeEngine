/* Start Header *****************************************************************/
/*!
\file   AnimationEditorPanel.cpp
\author Muhammad Nur Fadzly Bin Zulkifli
\brief
Implements the animation graph + timeline editor panel.
*/
/* End Header *******************************************************************/

#include "AnimationEditorPanel.h"
#include "EditorStyle.h"

// Note: This is a very basic implementation of the animation editor panel, with placeholder UI elements.
// The actual graph editing and timeline functionality would require more complex UI and data management, which is beyond the scope of this initial implementation.
// TODO: Implement actual state graph editing and timeline functionality in future iterations.
void AnimationEditorPanel::Initialize(ImFont* mainFont, ImFont* boldFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
}

void AnimationEditorPanel::Render() {
    ImGui::Begin("Animation");

    if (m_boldFont) ImGui::PushFont(m_boldFont);
    ImGui::Text("State Graph");
    if (m_boldFont) ImGui::PopFont();

    ImGui::BeginChild("AnimationGraph", ImVec2(0, 180), true);
    ImGui::TextDisabled("Select an entity with AnimationController2D to edit.");
    ImGui::Text("States: (placeholder)");
    ImGui::Text("Transitions: (placeholder)");
    ImGui::EndChild();

    ImGui::Spacing();

    if (m_boldFont) ImGui::PushFont(m_boldFont);
    ImGui::Text("Timeline");
    if (m_boldFont) ImGui::PopFont();

    ImGui::BeginChild("AnimationTimeline", ImVec2(0, 120), true);
    ImGui::Text("Scrub");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##AnimTimelineScrub", &m_timelineCursor, 0.0f, 1.0f);
    ImGui::TextDisabled("Notifies / hitboxes markers are shown here.");
    ImGui::EndChild();

    ImGui::End();
}
