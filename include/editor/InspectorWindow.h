/* Start Header *****************************************************************/
/*!
\file   InspectorWindow.h
\author Foo Rui Qin (70%)
        Samanntha Leong (30%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Unified Inspector window that displays properties for either selected entities
or selected prefabs. Matches Unity's Inspector behavior.

Features:
- Single window for both entity and prefab inspection
- Component add/remove for both entities and prefabs
- "Open Prefab" button when editing prefab instances
- Identical UI layout whether editing entity or prefab

References:
- Unity Inspector window design
- ComponentInspectorUI for shared rendering
*/
/* End Header *******************************************************************/

#ifndef INSPECTOR_WINDOW_H
#define INSPECTOR_WINDOW_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "ecs/Entity.h"

// Forward declarations
struct ImFont;
class World;

class InspectorWindow {
public:
    enum class InspectionMode {
        None,           // Nothing selected
        Entity,         // Editing a live entity
        Prefab          // Editing a prefab file
    };

    // Initialize with fonts and world reference
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world);

    // Render the inspector window
    void Render(float fontScale);

    // Set what to inspect
    void InspectEntity(EntityId id);
    void InspectPrefab(const std::string& prefabPath);
    void ClearSelection();

    // Check if currently inspecting something
    InspectionMode GetMode() const { return m_mode; }

private:
    // Render entity inspection UI
    void _renderEntityInspector();

    // Render prefab inspection UI
    void _renderPrefabInspector();

    // Component management for entities
    void _addComponentToEntity(const std::string& componentType);
    void _removeComponentFromEntity(const std::string& componentType);

    // Component management for prefabs
    void _addComponentToPrefab(const std::string& componentType);
    void _removeComponentFromPrefab(const std::string& componentType);

    // Check if entity/prefab has component
    bool _entityHasComponent(EntityId id, const std::string& componentType);
    bool _prefabHasComponent(const std::string& componentType);

    // Get default component data
    nlohmann::json _getDefaultComponentData(const std::string& componentType);

    // Render component section with delete button
    template <typename T>
    void _renderComponentSection(const std::string& headerName, const std::string& componentType,
        nlohmann::json& data, T renderContent, bool canDelete = true);

    void _renderComponentMenuItem(const char* displayName, const char* componentType);

    // Save prefab changes to file
    void _savePrefab();

    // References
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    World* m_world = nullptr;

    // Inspection state
    InspectionMode m_mode = InspectionMode::None;
    EntityId m_inspectedEntityId = 0;
    std::string m_inspectedPrefabPath;
    nlohmann::json m_prefabData;

    // UI state
    std::vector<std::string> m_componentsToDelete;
    std::string m_statusMessage;
    float m_statusTimer = 0.0f;
};

#endif // INSPECTOR_WINDOW_H