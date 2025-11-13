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
*/
/* End Header *******************************************************************/

#pragma once

#include "ecs/World.h"
#include "../editor/ComponentInspectorUI.h"
#include <imgui.h>
#include <string>
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
    void _renderPrefabComponent(const std::string& typeName, nlohmann::json& componentData);
    void _renderPrefabActions();

    // -------------------------------------------------------------------------
    // Prefab Data Management
    // -------------------------------------------------------------------------
    void _loadPrefabData();
    void _savePrefabData();
    void _applyPrefabToInstances();
    void _applyPrefabDataToEntity(ECS::Entity entity);

    // -------------------------------------------------------------------------
    // Entity Property Management
    // -------------------------------------------------------------------------
    void _addComponentToEntity(const std::string& componentType);
    void _removeComponentFromEntity(const std::string& componentType);

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

    std::string m_statusMessage;
    float m_statusTimer = 0.0f;
};
