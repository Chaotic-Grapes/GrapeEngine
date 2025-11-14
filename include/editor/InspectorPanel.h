/* Start Header *****************************************************************/
/*!
\file   InspectorPanel.h
\author Foo Rui Qin    (80%)
        Samantha Leong (20%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Declares the unified InspectorPanel that adapts to selection context.

RESTORED FEATURES:
- Proper component rendering with delete buttons via _renderComponentSection
- Full "Add Component" menu with all component types
- Entity-prefab linking with "Open Prefab" button
- Working prefab inspector with save/apply functionality
- Status messages and proper UI layout
- Hash-based change tracking for prefabs
*/
/* End Header *******************************************************************/

#pragma once

#include "ecs/World.h"
#include "../editor/ComponentInspectorUI.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using EntityId = uint32_t;

// Unified inspector panel that can inspect both entities and prefabs
class InspectorPanel {
public:
    // Inspection modes
    enum class InspectionMode {
        None,
        Entity,
        Prefab
    };

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);
    void SetWorld(ECS::World* world);

    // -------------------------------------------------------------------------
    // Selection Management
    // -------------------------------------------------------------------------
    void InspectEntity(EntityId id);
    void InspectPrefab(const std::string& path);
    void ClearSelection();

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------
    void Render(float fontScale);

    // -------------------------------------------------------------------------
    // Query current mode
    // -------------------------------------------------------------------------
    InspectionMode GetMode() const { return m_mode; }

private:
    // -------------------------------------------------------------------------
    // Entity Inspector
    // -------------------------------------------------------------------------
    void _renderEntityInspector();
    void _renderEntityHeader(ECS::Entity entity);
    void _renderEntityComponents(ECS::Entity entity);
    void _renderAddComponentButton(ECS::Entity entity);

    // -------------------------------------------------------------------------
    // Prefab Inspector
    // -------------------------------------------------------------------------
    void _renderPrefabInspector();
    void _renderPrefabHeader();
    void _renderPrefabComponents();
    void _renderPrefabActions();

    // -------------------------------------------------------------------------
    // Component Section Rendering (Template - implementation below)
    // -------------------------------------------------------------------------
    template <typename T>
    void _renderComponentSection(const std::string& headerName, const std::string& componentType,
        nlohmann::json& data, T renderContent, bool canDelete = true);

    // -------------------------------------------------------------------------
    // Component Menu Item
    // -------------------------------------------------------------------------
    void _renderComponentMenuItem(const char* displayName, const char* componentType);

    // -------------------------------------------------------------------------
    // Prefab Data Management
    // -------------------------------------------------------------------------
    void _loadPrefabData();
    void _savePrefabData();
    void _applyPrefabToInstances();
    void _applyPrefabDataToEntity(ECS::Entity entity);

    // -------------------------------------------------------------------------
    // Entity Component Management
    // -------------------------------------------------------------------------
    void _addComponentToEntity(const std::string& componentType);
    void _removeComponentFromEntity(const std::string& componentType);
    bool _entityHasComponent(EntityId id, const std::string& componentType);

    // -------------------------------------------------------------------------
    // Prefab Component Management
    // -------------------------------------------------------------------------
    void _addComponentToPrefab(const std::string& componentType);
    void _removeComponentFromPrefab(const std::string& componentType);
    bool _prefabHasComponent(const std::string& componentType);

    // -------------------------------------------------------------------------
    // Default Component Data
    // -------------------------------------------------------------------------
    nlohmann::json _getDefaultComponentData(const std::string& componentType);

    // -------------------------------------------------------------------------
    // Status Bar
    // -------------------------------------------------------------------------
    void _renderStatusBar();

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    ECS::World* m_world = nullptr;
    ComponentUI m_componentUI;

    InspectionMode m_mode = InspectionMode::None;
    EntityId m_entityId = 0;
    std::string m_prefabPath;
    nlohmann::json m_prefabData;
    size_t m_lastSavedPrefabHash = 0;

    std::vector<std::string> m_componentsToDelete;
    std::string m_statusMessage;
    float m_statusTimer = 0.0f;
};

// -------------------------------------------------------------------------
// Template Implementation (must be in header for template instantiation)
// -------------------------------------------------------------------------

template <typename T>
void InspectorPanel::_renderComponentSection(const std::string& headerName, const std::string& componentType,
    nlohmann::json& data, T renderContent, bool canDelete) {

    bool nodeOpen = ImGui::CollapsingHeader(
        headerName.c_str(),
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth
    );

    if (nodeOpen) {
        const char* deleteIcon = "\xEE\xA1\xB2";
        ImVec2 contentCursorPos = ImGui::GetCursorPos();

        // Calculate button position
        ImGui::PushFont(m_symbolsFont);
        float iconWidth = ImGui::CalcTextSize(deleteIcon).x;
        float btnWidth = iconWidth + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::PopFont();
        float rightVisible = ImGui::GetWindowContentRegionMax().x + ImGui::GetScrollX();
        ImGui::SetCursorPosX(rightVisible - btnWidth);

        // Render delete button
        if (!canDelete) ImGui::BeginDisabled();
        ImGui::PushFont(m_symbolsFont);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, canDelete ? ImVec4(0.7f, 0.2f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        bool clicked = ImGui::SmallButton((std::string(deleteIcon) + "##Delete" + componentType).c_str());
        ImGui::PopStyleColor(4);
        ImGui::PopFont();
        if (!canDelete) ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip(canDelete ? "Remove Component" : "Transform cannot be removed");
        }

        if (clicked && canDelete) {
            m_componentsToDelete.push_back(componentType);
        }

        // Restore cursor and render content
        ImGui::SetCursorPos(contentCursorPos);
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        renderContent(data);
    }
}
