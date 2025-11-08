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

    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);
    void Render(float fontScale);
    void InspectEntity(EntityId id);
    void InspectPrefab(const std::string& prefabPath);
    void ClearSelection();
    void SetWorld(ECS::World* world) { m_world = world; ClearSelection(); }
    InspectionMode GetMode() const { return m_mode; }

private:
    void _renderEntityCore();
    void _renderPrefabInspector();
    void _applyPrefabChangesToInstances();
    void _applyPrefabDataToEntity(ECS::Entity entity);
    void _saveEntityAsPrefab();
    void _addComponentToEntity(const std::string& componentType);
    void _removeComponentFromEntity(const std::string& componentType);
    void _addComponentToPrefab(const std::string& componentType);
    void _removeComponentFromPrefab(const std::string& componentType);
    bool _entityHasComponent(EntityId id, const std::string& componentType);
    bool _prefabHasComponent(const std::string& componentType);
    nlohmann::json _getDefaultComponentData(const std::string& componentType);

    template <typename T>
    void _renderComponentSection(const std::string& headerName, const std::string& componentType,
        nlohmann::json& data, T renderContent, bool canDelete = true);

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