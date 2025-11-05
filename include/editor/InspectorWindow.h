/* Start Header *****************************************************************/
/*!
\file   InspectorWindow.h
\author Foo Rui Qin    (80%)
        Samantha Leong (20%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Unified Inspector window that displays properties for either selected entities
or selected prefabs. Matches Unity's Inspector behavior.

HOW IT WORKS:
* Click an entity in Hierarchy > Shows entity's components
  - If entity is a prefab instance > Shows "Prefab: name.prefab [Open]" header
  - Click [Open] > Switches to prefab editing mode
* Click a .prefab in Asset Browser > Shows prefab's components + [Apply] button
*/
/* End Header *******************************************************************/

#ifndef INSPECTOR_WINDOW_H
#define INSPECTOR_WINDOW_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "ecs/Entity.h"
#include "../editor/ComponentInspectorUI.h"

// Forward declarations
struct ImFont;
class World;

class InspectorWindow {
public:
    // What we're currently inspecting
    enum class InspectionMode {
        None,           // Nothing selected
        Entity,         // Editing a live entity (from Hierarchy)
        Prefab          // Editing a prefab file (from Asset Browser or clicked "Open")
    };

    // Initialize with fonts and world reference
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world);

    // Render the inspector window
    void Render(float fontScale);

    // Set what to inspect (called by Hierarchy or Asset Browser)
    void InspectEntity(EntityId id);
    void InspectPrefab(const std::string& prefabPath);
    void ClearSelection();

    // Check if currently inspecting something
    InspectionMode GetMode() const { return m_mode; }

private:
    // Render entity inspection UI (shows all components on entity)
    void _renderEntityCore();

    // Render prefab inspection UI (shows components in prefab file)
    void _renderPrefabInspector();

    // Add a component to the selected entity
    void _addComponentToEntity(const std::string& componentType);

    // Remove a component from the selected entity
    void _removeComponentFromEntity(const std::string& componentType);

    // Add a component to the prefab JSON
    void _addComponentToPrefab(const std::string& componentType);

    // Remove a component from the prefab JSON
    void _removeComponentFromPrefab(const std::string& componentType);

    // Check if entity has a specific component
    bool _entityHasComponent(EntityId id, const std::string& componentType);

    // Check if prefab JSON has a specific component
    bool _prefabHasComponent(const std::string& componentType);

    // Get default JSON data when adding a new component
    nlohmann::json _getDefaultComponentData(const std::string& componentType);

    // Render a component section with collapsible header and delete button
    // (Used by both entity and prefab inspection)
    template <typename T>
    void _renderComponentSection(const std::string& headerName, const std::string& componentType,
        nlohmann::json& data, T renderContent, bool canDelete = true);

    // Render a single menu item in the add component popup
    void _renderComponentMenuItem(const char* displayName, const char* componentType);

    // Save prefab changes to disk and update all instances in the scene
    void _savePrefab();

    // Save the currently selected entity as a prefab asset
    void _saveEntityAsPrefab();

    // Font references
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    World* m_world = nullptr;

    // Inspection state
    InspectionMode m_mode = InspectionMode::None;
    EntityId m_inspectedEntityId = 0;                // Which entity we're inspecting
    std::string m_inspectedPrefabPath;               // Which prefab file we're editing
    nlohmann::json m_prefabData;                     // Prefab JSON data
    size_t m_lastSavedPrefabHash = 0;                // Track last-saved prefab content hash to avoid redundant saves
    ComponentUI m_componentUI;                       // Instance of ComponentUI used to render different component UIs

    // UI state
    std::vector<std::string> m_componentsToDelete;   // Queue of components to delete
    std::string m_statusMessage;                     // Status toast message
    float m_statusTimer = 0.0f;                      // How long to show status
};

#endif // INSPECTOR_WINDOW_H