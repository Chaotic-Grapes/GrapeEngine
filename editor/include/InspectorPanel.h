/* Start Header *****************************************************************/
/*!
\file   InspectorPanel.h
\author Foo Rui Qin    (80%)
        Samantha Leong (20%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   15th November 2025

\brief
Editor panel for viewing and editing game entities and prefab assets.

This inspector panel provides the interface for component inspection and modification.
It supports both runtime entity editing and prefab asset editing through a unified
system. The panel handles component addition and removal, property editing, prefab
instantiation and synchronization between live entities and serialized prefab data.
*/
/* End Header *******************************************************************/

#ifndef INSPECTOR_PANEL_H
#define INSPECTOR_PANEL_H

#include "ecs/World.h"
#include "ComponentPropertyEditor.h"
#include "ComponentWidgets.h"
#include "EditorComponentRegistry.h"
#include "ecs/PrefabManager.h"
#include <imgui.h>
#include "EditorStyle.h"
#include <string>
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "EditorFileMenu.h"

using EntityId = uint32_t;
class EditorFileMenu;

namespace Editor { class UndoSystem; }
namespace ECS { class PrefabManager; }

// Unified inspector panel capable of inspecting both entities and prefabs
class InspectorPanel {
public:
    // Defines the current inspection context
    enum class InspectionMode {
        None,    // No active selection
        Entity,  // Inspecting a runtime entity
        Prefab   // Inspecting a prefab asset
    };

    // -------------------------------------------------------------------------
    // Lifecycle Management
    // -------------------------------------------------------------------------

    // Initializes the inspector panel with required fonts and world reference
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);

    // Updates the world context for entity operations (so inspector can work on
    // different world instances dynamically)
    void SetWorld(ECS::World* world);

    // Set the prefab manager for displaying prefab instance information
    void SetPrefabManager(ECS::PrefabManager* manager) { m_prefabManager = manager; }

    // -------------------------------------------------------------------------
    // Selection Management
    // -------------------------------------------------------------------------

    // Sets the inspector to inspect a specific entity by ID
    void InspectEntity(EntityId id);

    // Sets the inspector to inspect a specific prefab file by path
    void InspectPrefab(const std::string& path);

    // Returns the currently inspected prefab path, empty if not inspecting a prefab
    std::string GetInspectedPrefabPath() const { return m_prefabPath; }

    // Clears the current selection and resets the inspector state
    void ClearSelection();

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    // Renders the inspector panel interface based on current mode
    // Either entity or prefab
    void Render();

    // -------------------------------------------------------------------------
    // State Query
    // -------------------------------------------------------------------------

    // Returns the current inspection mode to determine context
    InspectionMode GetMode() const { return m_mode; }

    // Set file menu reference for dirty tracking
    void SetFileMenu(EditorFileMenu * fileMenu) { m_fileMenu = fileMenu; }

    // -------------------------------------------------------------------------
    // Undo system support
    // -------------------------------------------------------------------------

    void SetUndoSystem(Editor::UndoSystem* undoSystem) { m_undoSystem = undoSystem; }

private:
    // -------------------------------------------------------------------------
    // Entity Inspector Implementation
    // -------------------------------------------------------------------------

    // Renders the complete entity inspector interface with header and components
    void _renderEntityInspector();

    // Renders the entity header section with name display and prefab linkage
    void _renderEntityHeader(ECS::Entity entity);

    // Renders all components attached to the entity using component sections
    void _renderEntityComponents(ECS::Entity entity);

    // Renders the Add Component button and dropdown menu with all available types
    void _renderAddComponentButton(ECS::Entity entity);

    // -------------------------------------------------------------------------
    // Prefab Inspector Implementation
    // -------------------------------------------------------------------------

    // Renders the complete prefab inspector interface with header and components
    void _renderPrefabInspector();

    // Renders the prefab header section with file path information
    void _renderPrefabHeader();

    // Renders all components defined in the prefab JSON data
    void _renderPrefabComponents();

    // Renders prefab specific action buttons including Save and Apply
    void _renderPrefabActions();

    // -------------------------------------------------------------------------
    // Component Section Rendering (Template Implementation)
    // -------------------------------------------------------------------------

    // Renders a collapsible component section with consistent styling and delete button
    template <typename T>
    void _renderComponentSection(const std::string& headerName, const std::string& componentType,
        nlohmann::json& data, T renderContent, bool canDelete, const nlohmann::json* defaults);

    // -------------------------------------------------------------------------
    // Component Menu Management
    // -------------------------------------------------------------------------

    // Renders a single component option in the Add Component menu
    void _renderComponentMenuItem(const char* displayName, const char* componentType);

    // -------------------------------------------------------------------------
    // Prefab Data Management
    // -------------------------------------------------------------------------

    // Saves modified prefab data back to disk with change tracking
    void _savePrefabData();

    // Exports an entity as a new prefab asset in the assets/prefabs directory
    void _saveEntityAsPrefab(ECS::Entity entity);

    // Applies prefab changes to all existing instances in the current world
    void _applyPrefabToInstances();

    // Applies prefab data to a specific entity instance
    void _applyPrefabDataToEntity(ECS::Entity entity);

    // -------------------------------------------------------------------------
    // Entity Component Management
    // -------------------------------------------------------------------------

    // Adds a component of specified type to an entity using registry metadata
    bool _addComponentToEntity(const std::string& componentType);

    // Removes a component of specified type from an entity
    void _removeComponentFromEntity(const std::string& componentType, bool recordUndo = true);

    // Checks if an entity has a specific component type using registry queries
    bool _entityHasComponent(EntityId id, const std::string& componentType);

    // -------------------------------------------------------------------------
    // Prefab Component Management
    // -------------------------------------------------------------------------

    // Adds a component definition to the prefab with default values
    bool _addComponentToPrefab(const std::string& componentType);

    // Removes a component definition from the prefab JSON data
    void _removeComponentFromPrefab(const std::string& componentType);

    // Checks if the prefab contains a specific component type definition
    bool _prefabHasComponent(const std::string& componentType);

    // -------------------------------------------------------------------------
    // Status Management
    // -------------------------------------------------------------------------

    // Renders the status bar with messages and notifications
    void _renderStatusBar();

    // -------------------------------------------------------------------------
    // Member Variables
    // -------------------------------------------------------------------------

    // Font references for UI styling
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // System references
    ECS::World* m_world = nullptr;
    ComponentUI m_componentUI;
    EditorFileMenu* m_fileMenu = nullptr;
    Editor::UndoSystem* m_undoSystem = nullptr;
    ECS::PrefabManager* m_prefabManager = nullptr; // Prefab manager (for prefab path lookup / instance management)

    // Selection state
    InspectionMode m_mode = InspectionMode::None;  // Current inspection context
    EntityId m_entityId = 0;                       // Currently inspected entity ID

    // File path and data
    std::string m_prefabPath;                      // Path to prefab asset file
    nlohmann::json m_prefabData;                   // Loaded JSON data for editing  
    size_t m_lastSavedPrefabHash = 0;              // Hash of last saved state

    // UI state
    std::vector<std::string> m_componentsToDelete; // Components scheduled for removal
    std::string m_statusMessage;                   // Current status message 
    float m_statusTimer = 0.0f;                    // Timer for status message

    // Add Component popup search state
    char m_addComponentSearchBuffer[128] = {0};    // Temporary input buffer for search
    std::string m_addComponentSearchFilter;        // Filter string used to match component names

    // Component/property filter state for the inspector list
    char m_componentFilterBuffer[128] = {0};       // Temporary input buffer for component/property filtering
    std::string m_componentFilter;                 // Active filter used in component/property list
    bool m_focusComponentFilter = false;           // Keyboard focus request for the filter input
    bool m_focusAddComponentSearch = false;        // Keyboard focus request for Add Component search
    bool m_openAddComponentPopup = false;          // Deferred popup open flag for keyboard shortcuts

    // Undo - edit tracking
    struct EditState {
        EntityId entityId = 0;
        Vector3D startPosition;
        Quaternion startRotation;
        Vector3D startScale;
        std::vector<ECS::SerializedComponent> startComponents;
        bool hasSnapshot = false;
        bool isEditing = false;
    };
    EditState m_editState;
};

// -------------------------------------------------------------------------
// Template Implementation
// -------------------------------------------------------------------------

// Renders collapsible component section with header, content area and delete button
// Template allows any render function to be passed for component-specific UI
template <typename T>
void InspectorPanel::_renderComponentSection(const std::string& headerName, const std::string& componentType,
    nlohmann::json& data, T renderContent, bool canDelete, const nlohmann::json* defaults)
{
    // DefaultOpen: Component starts expanded for immediate editing access
    // Framed: Adds visual border around header for clear section separation
    // SpanFullWidth: Header uses entire available width regardless of content
    ImGui::SetNextItemAllowOverlap();
    if (m_boldFont) ImGui::PushFont(m_boldFont);
    bool nodeOpen = ImGui::CollapsingHeader(headerName.c_str(),
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth);
    if (m_boldFont) ImGui::PopFont();

    // Render delete button aligned to the right of the header
    float buttonSize = ImGui::GetFrameHeight();
    float buttonX = ImGui::GetWindowContentRegionMax().x - buttonSize - ImGui::GetStyle().FramePadding.x;
    float resetX = buttonX - buttonSize - ImGui::GetStyle().ItemSpacing.x;

    // Optional reset button to restore the component to defaults
    if (defaults) {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosX(resetX);

        const char* resetIcon = "\xEF\x91\xBF"; // Reset icon (material: restart_alt)

        // Reset component data back to its default JSON payload
        const float lineHeight = ImGui::GetTextLineHeight();
        const float frameHeight = ImGui::GetFrameHeight();
        const float y = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(y - (frameHeight - lineHeight) * 0.5f);

        // Style the reset button with secondary colors and symbol font
        ImGui::PushID((std::string("ResetComponent") + componentType).c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

        // Use symbol font for the reset icon
        if (m_symbolsFont) ImGui::PushFont(m_symbolsFont);
        const bool resetClicked = ImGui::Button(resetIcon, ImVec2(buttonSize, buttonSize));
        if (m_symbolsFont) ImGui::PopFont();

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset component to defaults");
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        ImGui::PopID();

        if (resetClicked) {
            data = *defaults;
        }
    }

    // Only show delete button if component is removable
    if (canDelete) {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosX(buttonX);

        // Match the reset icon's vertical alignment so header actions line up
        const float lineHeight = ImGui::GetTextLineHeight();
        const float frameHeight = ImGui::GetFrameHeight();
        const float y = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(y - (frameHeight - lineHeight) * 0.5f);

        // Style the remove button with danger colors and symbol font
        bool pushedFont = false;
        if (m_symbolsFont) {
            ImGui::PushFont(m_symbolsFont);
            pushedFont = true;
        }

        // Use a unicode cross symbol for the button label
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::Scale(EditorStyle::FrameBgHover, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::Scale(EditorStyle::FrameBgActive, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::DangerText);

        // Remove component if button is clicked
        if (ImGui::Button((std::string("\xEE\xA1\xB2##RemoveComponent") + componentType).c_str(), ImVec2(buttonSize, buttonSize))) {
            m_componentsToDelete.push_back(componentType);
        }

        // Pop styles and font after button rendering
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
        if (pushedFont) {
            ImGui::PopFont();
        }
    }

    // Create collapsing header with specific behavior flags
    if (nodeOpen) {
        // After header, display component-specific UI
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float boxPaddingX = 10.0f;
        const float boxPaddingY = 6.0f;
        const float boxRounding = 6.0f;

        // Calculate box positions for background rendering
        const ImVec2 windowPos = ImGui::GetWindowPos();

        // Get available width for the content box
        const float boxWidth = ImGui::GetContentRegionAvail().x;
        const float gap = ImGui::GetStyle().ItemSpacing.y;

        // Adjust cursor position to account for spacing
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - gap);
        ImVec2 boxMinScreen = ImGui::GetCursorScreenPos();

        // Split draw list into layers for background and content
        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1);

        // Render content within padded box area
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0.0f, boxPaddingY));
        ImGui::Indent(boxPaddingX);

        // Register defaults for per-field reset buttons inside this component
        if (defaults) {
            EditorUI::RegisterDefaultDataScope(data, *defaults);
        }
        renderContent(data);
        if (defaults) {
            EditorUI::ClearDefaultDataScope();
        }

        ImGui::Unindent(boxPaddingX);
        ImGui::Dummy(ImVec2(0.0f, boxPaddingY));
        ImGui::EndGroup();

        // Calculate box max position based on content size
        ImVec2 boxMaxScreen = ImGui::GetItemRectMax();
        boxMaxScreen.x = boxMinScreen.x + boxWidth;

        // Render background box with rounded corners
        drawList->ChannelsSetCurrent(0);
        drawList->AddRectFilled(
            boxMinScreen,
            boxMaxScreen,
            ImGui::GetColorU32(EditorStyle::Scale(EditorStyle::FrameBg, 0.85f)),
            boxRounding,
            ImDrawFlags_RoundCornersBottom
        );
        drawList->AddRect(
            boxMinScreen,
            boxMaxScreen,
            ImGui::GetColorU32(EditorStyle::Scale(EditorStyle::Border, 0.85f)),
            boxRounding,
            ImDrawFlags_RoundCornersBottom
        );

        // Merge draw channels back together
        drawList->ChannelsMerge();
        ImGui::SetCursorScreenPos(ImVec2(boxMinScreen.x, boxMaxScreen.y));

        ImGui::Spacing();
        ImGui::Spacing();
    }
}

#endif
