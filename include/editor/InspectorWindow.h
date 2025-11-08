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

struct ImFont;
namespace ECS {
    class World;
}

class InspectorWindow {
public:
    enum class InspectionMode {
        None,
        Entity,
        Prefab
    };

    // This sets up fonts and the world reference
    // It gets the inspector ready for work
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);
    // This draws the inspector window
    // It shows components and prefab details
    void Render(float fontScale);
    // This selects an entity to inspect
    // It reads components and prepares UI
    void InspectEntity(EntityId id);
    // This opens a prefab for editing
    // It loads JSON and sets mode
    void InspectPrefab(const std::string& prefabPath);
    // This clears the current selection
    // It resets status and mode
    void ClearSelection();
    // This updates the world pointer and clears selection
    // It keeps inspector in sync when scenes change
    void SetWorld(ECS::World* world) { m_world = world; ClearSelection(); }
    // This returns the current inspection mode
    // It shows if we are on entity or prefab
    InspectionMode GetMode() const { return m_mode; }

private:
    // This renders core entity component sections
    // It handles add remove and edit actions
    void _renderEntityCore();
    // This draws prefab editing sections
    // It shows apply and save actions
    void _renderPrefabInspector();
    // This pushes prefab changes to instances
    // It updates matching entities in the world
    void _applyPrefabChangesToInstances();
    // This applies prefab data to one entity
    // It overwrites components with prefab values
    void _applyPrefabDataToEntity(ECS::Entity entity);
    // This saves the inspected entity as a prefab
    // It writes a JSON file to assets
    void _saveEntityAsPrefab();
    // This adds a component to the entity
    // It updates data and UI state
    void _addComponentToEntity(const std::string& componentType);
    // This removes a component from the entity
    // It updates data and UI state
    void _removeComponentFromEntity(const std::string& componentType);
    // This adds a component to the prefab
    // It updates JSON data and UI state
    void _addComponentToPrefab(const std::string& componentType);
    // This removes a component from the prefab
    // It updates JSON data and UI state
    void _removeComponentFromPrefab(const std::string& componentType);
    // This checks if an entity has a component
    // It looks up by type name
    bool _entityHasComponent(EntityId id, const std::string& componentType);
    // This checks if a prefab has a component
    // It looks up by type name in JSON
    bool _prefabHasComponent(const std::string& componentType);
    // This returns default JSON data for a component
    // It seeds new components with sensible values
    nlohmann::json _getDefaultComponentData(const std::string& componentType);

    template <typename T>
    // This renders a component section with a header
    // It calls the provided renderer and supports deletion
    void _renderComponentSection(const std::string& headerName, const std::string& componentType,
        nlohmann::json& data, T renderContent, bool canDelete = true);

    // This renders a menu item to add a component
    // It shows a readable name and binds the type
    void _renderComponentMenuItem(const char* displayName, const char* componentType);

private:
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    ECS::World* m_world = nullptr;

    InspectionMode m_mode = InspectionMode::None;
    EntityId m_inspectedEntityId = 0;
    std::string m_inspectedPrefabPath;
    nlohmann::json m_prefabData;
    size_t m_lastSavedPrefabHash = 0;
    ComponentUI m_componentUI;

    std::vector<std::string> m_componentsToDelete;
    std::string m_statusMessage;
    float m_statusTimer = 0.0f;
};

#endif