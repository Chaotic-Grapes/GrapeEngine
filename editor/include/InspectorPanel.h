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
        nlohmann::json& data, T renderContent, bool canDelete = true);

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
    void _addComponentToEntity(const std::string& componentType);

    // Removes a component of specified type from an entity
    void _removeComponentFromEntity(const std::string& componentType);

    // Checks if an entity has a specific component type using registry queries
    bool _entityHasComponent(EntityId id, const std::string& componentType);

    // -------------------------------------------------------------------------
    // Prefab Component Management
    // -------------------------------------------------------------------------

    // Adds a component definition to the prefab with default values
    void _addComponentToPrefab(const std::string& componentType);

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

    // Undo - edit tracking
    struct EditState {
        EntityId entityId = 0;
        Vector3D startPosition;
        Quaternion startRotation;
        Vector3D startScale;
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
    nlohmann::json& data, T renderContent, bool canDelete)
{
    // DefaultOpen: Component starts expanded for immediate editing access
    // Framed: Adds visual border around header for clear section separation  
    // SpanFullWidth: Header uses entire available width regardless of content
    bool nodeOpen = ImGui::CollapsingHeader(headerName.c_str(),
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth);

    // Create collapsing header with specific behavior flags
    if (nodeOpen) {
        const char* deleteIcon = "\xEE\xA1\xB2";         // Trash icon
        ImVec2 contentCursorPos = ImGui::GetCursorPos(); // Save position before button for content alignment

        // CalcTextSize returns pixel dimensions of icon text
        ImGui::PushFont(m_symbolsFont);
        float iconWidth = ImGui::CalcTextSize(deleteIcon).x;

        // Button width = icon width + padding on both sides (FramePadding.x * 2)
        float btnWidth = iconWidth + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::PopFont();

        // X position = total content width - button width = right edge alignment
        // Y position = original cursor Y + 5px offset for vertical centering in header
        float deleteButtonX = EditorUI::GetContentWidth() - btnWidth;
        ImGui::SetCursorPos(ImVec2(deleteButtonX, contentCursorPos.y + 5.0f));

        // Disable interaction if not deletable
        if (!canDelete) ImGui::BeginDisabled();
        ImGui::PushFont(m_symbolsFont);

        // Button: Transparent background (RGBA 0,0,0,0 = fully transparent)
        // ButtonHovered: Dark gray with 30% opacity when mouse over
        // ButtonActive: Medium gray with 50% opacity when clicked
        // Text: Red color if deletable, gray if disabled (Transform component)
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::Scale(EditorStyle::FrameBgHover, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::Scale(EditorStyle::FrameBgActive, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, canDelete ? EditorStyle::DangerText : EditorStyle::TextDisabled);

        // Create button with unique ID to avoid ImGui ID conflicts
        // ## ensures each button has distinct identifier
        bool clicked = ImGui::SmallButton((std::string(deleteIcon) + "##Delete" + componentType).c_str());

        ImGui::PopStyleColor(4); // Restore original ImGui colors (pop 4 pushed colors)
        ImGui::PopFont();
        if (!canDelete) ImGui::EndDisabled();

        // AllowWhenDisabled flag shows tooltip even for disabled buttons
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip(canDelete ? "Remove Component" : "Transform cannot be removed");
        }

        // Schedule component for deferred deletion if clicked to prevent modifying data 
        // during UI iteration
        if (clicked && canDelete) {
            m_componentsToDelete.push_back(componentType);
        }

        // Restore cursor to original position saved before button
        ImGui::SetCursorPos(contentCursorPos);
        ImGui::Dummy(ImVec2(0.0f, 5.0f));

        // After header + trash icon, display component-specific UI
        renderContent(data);
    }
}

#endif