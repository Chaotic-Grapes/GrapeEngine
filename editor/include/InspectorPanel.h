/* Start Header *****************************************************************/
/*!
\file   InspectorPanel.h
\author Foo Rui Qin    (80%)
        Samantha Leong (20%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   11th March 2026

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
#include "EditorIcons.h"
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

    // Initialize the inspector panel with required fonts and world reference
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);

    // Update the world context when switching between scene instances
    void SetWorld(ECS::World* world);

    // Set the prefab manager for prefab path lookup and instance management
    void SetPrefabManager(ECS::PrefabManager* manager) { m_prefabManager = manager; }

    // Set the file menu reference for dirty state tracking
    void SetFileMenu(EditorFileMenu* fileMenu) { m_fileMenu = fileMenu; }

    // Set the undo system for recording component edits
    void SetUndoSystem(Editor::UndoSystem* undoSystem) { m_undoSystem = undoSystem; }

    // -------------------------------------------------------------------------
    // Selection Management
    // -------------------------------------------------------------------------

    // Set the inspector to inspect a specific entity by ID
    void InspectEntity(EntityId id);

    // Set the list of all currently selected entities for multi-select edits
    void SetSelectedEntities(const std::unordered_set<EntityId>& ids);

    // Set the inspector to inspect a specific prefab file by path
    void InspectPrefab(const std::string& path);

    // Return the currently inspected prefab path, empty if not inspecting a prefab
    std::string GetInspectedPrefabPath() const { return m_prefabPath; }

    // Clear the current selection and reset the inspector state
    void ClearSelection();

    // Request focus for the inspector window on the next render pass
    void RequestFocus();

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    // Render the inspector panel interface based on current inspection mode
    void Render();

    // -------------------------------------------------------------------------
    // State Query
    // -------------------------------------------------------------------------

    // Return the current inspection mode to determine context
    InspectionMode GetMode() const { return m_mode; }

private:
    // -------------------------------------------------------------------------
    // Entity Inspector Implementation
    // -------------------------------------------------------------------------

    // Render the complete entity inspector interface with header and components
    void _renderEntityInspector();

    // Render the entity header section with name display and prefab linkage
    void _renderEntityHeader(ECS::Entity entity);

    // Render all components attached to the entity using component sections
    void _renderEntityComponents(ECS::Entity entity);

    // Render the Add Component button and dropdown menu with all available types
    void _renderAddComponentButton(ECS::Entity entity);

    // -------------------------------------------------------------------------
    // Prefab Inspector Implementation
    // -------------------------------------------------------------------------

    // Render the complete prefab inspector interface with header and components
    void _renderPrefabInspector();

    // Render the prefab header section with file path information
    void _renderPrefabHeader();

    // Render all components defined in the prefab JSON data
    void _renderPrefabComponents();

    // Render prefab-specific action buttons including Save and Apply
    void _renderPrefabActions();

    // Return true if prefab data is stored in hierarchical {"Entity": {...}} format
    bool _isHierarchicalPrefab() const;

    // Return the selected prefab node from path, or root if path is empty
    nlohmann::json* _getSelectedPrefabNode();
    const nlohmann::json* _getSelectedPrefabNode() const;

    // Return the Components array for the selected prefab node
    nlohmann::json* _getSelectedPrefabComponents(bool createIfMissing);
    const nlohmann::json* _getSelectedPrefabComponents() const;

    // Resolve a readable display name for a prefab node in the selector popup
    std::string _getPrefabNodeDisplayName(const nlohmann::json& node) const;

    // Data structure for prefab node selection items in the popup menu
    struct PrefabNodeSelectionItem {
        std::vector<size_t> Path;   // Index path from root to this node
        std::string Label;          // Display name shown in the popup
        int Depth = 0;              // Depth in the hierarchy for indentation display
    };

    // Build root and full descendant list for popup node selection
    std::vector<PrefabNodeSelectionItem> _buildPrefabNodeSelectionItems() const;

    // -------------------------------------------------------------------------
    // Component Section Rendering (Template Implementation)
    // -------------------------------------------------------------------------

    // Render a collapsible component section with consistent styling and delete button
    template <typename T>
    void _renderComponentSection(const std::string& headerName, const std::string& componentType,
        nlohmann::json& data, T renderContent, bool canDelete, const nlohmann::json* defaults);

    // -------------------------------------------------------------------------
    // Component Menu Management
    // -------------------------------------------------------------------------

    // Render a single component option in the Add Component menu
    void _renderComponentMenuItem(const char* displayName, const char* componentType);

    // -------------------------------------------------------------------------
    // Prefab Data Management
    // -------------------------------------------------------------------------

    // Save modified prefab data back to disk with change tracking
    void _savePrefabData();

    // Export an entity as a new prefab asset in the assets/prefabs directory
    void _saveEntityAsPrefab(ECS::Entity entity);

    // Apply prefab changes to all existing instances in the current world
    void _applyPrefabToInstances();

    // Apply one prefab node's component data to an entity
    void _applyPrefabDataToEntity(ECS::Entity entity, const nlohmann::json& prefabNode, bool preserveRootTransform);

    // Recursively apply a prefab node and its descendants to an entity hierarchy
    void _applyPrefabHierarchyToEntity(ECS::Entity entity, const nlohmann::json& prefabNode, bool preserveRootTransform);

    // -------------------------------------------------------------------------
    // Entity Component Management
    // -------------------------------------------------------------------------

    // Add a component of the specified type to an entity using registry metadata
    bool _addComponentToEntity(const std::string& componentType, bool recordUndo = true);

    // Remove a component of the specified type from an entity
    void _removeComponentFromEntity(const std::string& componentType, bool recordUndo = true);

    // Return true if the entity has a specific component type
    bool _entityHasComponent(EntityId id, const std::string& componentType);

    // -------------------------------------------------------------------------
    // Prefab Component Management
    // -------------------------------------------------------------------------

    // Add a component definition with default values to the prefab JSON
    bool _addComponentToPrefab(const std::string& componentType);

    // Remove a component definition from the prefab JSON data
    void _removeComponentFromPrefab(const std::string& componentType);

    // Reset a component on all selected entities to its default values
    void _resetComponentOnSelectedEntities(const std::string& componentType, nlohmann::json& data, const nlohmann::json& defaults);

    // Return true if the prefab contains a specific component type definition
    bool _prefabHasComponent(const std::string& componentType);

    // -------------------------------------------------------------------------
    // Status Management
    // -------------------------------------------------------------------------

    // Render the status bar with messages and notifications
    void _renderStatusBar();

    // -------------------------------------------------------------------------
    // Member Variables
    // -------------------------------------------------------------------------

    ImFont* m_mainFont = nullptr;       // Main body font
    ImFont* m_boldFont = nullptr;       // Bold font for section headers
    ImFont* m_symbolsFont = nullptr;    // Symbols/icon font for icon-only buttons

    // System references
    ECS::World* m_world = nullptr;                      // ECS world containing all entities and components
    ComponentUI m_componentUI;                          // Component property editor for all component types
    EditorFileMenu* m_fileMenu = nullptr;               // File menu reference for dirty state tracking
    Editor::UndoSystem* m_undoSystem = nullptr;         // Undo system for recording component edits
    ECS::PrefabManager* m_prefabManager = nullptr;      // Prefab manager for path lookup and instance management

    // Selection state
    InspectionMode m_mode = InspectionMode::None;       // Current inspection context
    EntityId m_entityId = 0;                            // Primary inspected entity ID
    std::unordered_set<EntityId> m_selectedEntities;    // All selected entities for multi-edit support

    // File path and data
    std::string m_prefabPath;                           // Path to the currently inspected prefab asset
    nlohmann::json m_prefabData;                        // Loaded JSON data for editing
    size_t m_lastSavedPrefabHash = 0;                   // Hash of the last saved prefab state for dirty detection
    std::vector<size_t> m_selectedPrefabNodePath;       // Index path to selected prefab node (empty = root)

    // UI state
    std::vector<std::string> m_componentsToDelete;      // Components scheduled for removal at end of frame
    std::string m_statusMessage;                        // Current status bar message
    float m_statusTimer = 0.0f;                         // Remaining display time for the status message

    // Add Component popup search state
    char m_addComponentSearchBuffer[128] = { 0 };       // Input buffer for Add Component search
    std::string m_addComponentSearchFilter;             // Active filter string for matching component names

    // Component/property filter state
    char m_componentFilterBuffer[128] = { 0 };          // Input buffer for component/property filtering
    std::string m_componentFilter;                      // Active filter string for the component list
    bool m_focusComponentFilter = false;                // Focus request for the component filter input
    bool m_focusAddComponentSearch = false;             // Focus request for the Add Component search input
    bool m_openAddComponentPopup = false;               // Deferred popup open flag for keyboard shortcuts
    bool m_focusOnNextRender = false;                   // Window focus request for the inspector panel

    // Edit state snapshot for undo recording
    struct EditState {
        EntityId entityId = 0;                                      // Entity being edited
        Vector3D startPosition;                                     // Transform position at edit start
        Quaternion startRotation;                                   // Transform rotation at edit start
        Vector3D startScale;                                        // Transform scale at edit start
        std::vector<ECS::SerializedComponent> startComponents;      // Full component snapshot at edit start
        bool hasSnapshot = false;                                   // Whether a snapshot has been taken
        bool isEditing = false;                                     // Whether an edit is currently in progress
    };
    EditState m_editState; // Tracks pre-edit snapshot for undo recording
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
    const ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_DefaultOpen |
        ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth;
    const ImGuiID headerId = ImGui::GetID(headerName.c_str());
    const bool wasOpen = ImGui::GetStateStorage()->GetBool(
        headerId, (headerFlags & ImGuiTreeNodeFlags_DefaultOpen) != 0);
    const float headerRounding = wasOpen ? 0.0f : ImGui::GetStyle().FrameRounding;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, headerRounding);
    if (m_boldFont) ImGui::PushFont(m_boldFont);
    bool nodeOpen = ImGui::CollapsingHeader(headerName.c_str(), headerFlags);
    if (m_boldFont) ImGui::PopFont();
    ImGui::PopStyleVar();

    // Render delete button aligned to the right of the header
    float buttonSize = ImGui::GetFrameHeight();
    float buttonX = ImGui::GetWindowContentRegionMax().x - buttonSize - ImGui::GetStyle().FramePadding.x;
    float resetX = buttonX - buttonSize - ImGui::GetStyle().ItemSpacing.x;

    // Optional reset button to restore the component to defaults
    if (defaults) {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosX(resetX);

        const char* resetIcon = EditorIcons::Reset;

        // Reset component data back to its default JSON payload
        ImGui::SetCursorPosY(ImGui::GetCursorPosY());

        // Style the reset button with secondary colors and symbol font
        ImGui::PushID((std::string("ResetComponent") + componentType).c_str());
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);

        // Use symbol font for the reset icon
        if (m_symbolsFont) ImGui::PushFont(m_symbolsFont);
        const bool resetClicked = ImGui::Button(resetIcon, ImVec2(buttonSize, buttonSize));
        if (m_symbolsFont) ImGui::PopFont();

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset component to defaults");
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
        ImGui::PopID();

        if (resetClicked) {
            _resetComponentOnSelectedEntities(componentType, data, *defaults);
        }
    }

    // Only show delete button if component is removable
    if (canDelete) {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosX(buttonX);

        const char* trashIcon = EditorIcons::Delete;

        // Match the reset icon's vertical alignment so header actions line up
        ImGui::SetCursorPosY(ImGui::GetCursorPosY());

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
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::DangerText);

        // Remove component if button is clicked
        if (ImGui::Button((std::string(trashIcon) + "##RemoveComponent" + componentType).c_str(), ImVec2(buttonSize, buttonSize))) {
            m_componentsToDelete.push_back(componentType);
        }
        if (pushedFont) ImGui::PopFont();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Remove component");
        }

        // Pop styles and font after button rendering
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
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
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - gap * 2.f);
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