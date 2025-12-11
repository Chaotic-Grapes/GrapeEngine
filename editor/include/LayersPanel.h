/* Start Header *****************************************************************/
/*!
\file    LayersPanel.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Declares LayersPanel which allows viewing and editing scene layers.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef LAYERS_PANEL_H
#define LAYERS_PANEL_H

#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include "scene/Scene.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
// Forward declare EditorFileMenu and other editor helpers to avoid include dependency
class EditorFileMenu;
class HierarchyPanel;
class EntityActions;
namespace Editor { class UndoSystem; }

class LayersPanel {
public:
    LayersPanel() = default;
    ~LayersPanel();

    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);
    void SetWorld(ECS::World* world) { m_world = world; }
    void SetScene(Scenes::Scene* scene);
    void SetFileMenu(EditorFileMenu* fileMenu) { m_fileMenu = fileMenu; }
    void SetHierarchy(HierarchyPanel* h) { m_hierarchy = h; }
    void SetEntityActions(EntityActions* e) { m_entityActions = e; }
    void SetUndoSystem(Editor::UndoSystem* u) { m_undoSystem = u; }
    void MoveLayer(uint16_t fromId, uint16_t toId);
    uint16_t CreateLayer(const std::string& name);
    void SetLayer(EntityId entity, uint16_t layerId);

    void Render();

private:
    // Render helpers split out from the large Render() method
    void _renderHeader();
    void _renderCollisionMatrix();
    void _renderLayersList();
    void _renderImportConfirm();
    void _renderStatusPopup();

private:
    Scenes::Scene* m_scene = nullptr;
    ECS::World* m_world = nullptr;
    EditorFileMenu* m_fileMenu = nullptr;

    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // Temporary UI state
    int m_selectedLayerId = -1;
    char m_newLayerName[128] = "New Layer";
    HierarchyPanel* m_hierarchy = nullptr;
    EntityActions* m_entityActions = nullptr;
    Editor::UndoSystem* m_undoSystem = nullptr;
    
    // Subscription to scene modification messages so the panel can refresh
    Messaging::SubscriptionHandle m_sceneModifiedSubscription;
    bool m_needsRefresh = false;
    // Buffer used for import/export JSON of layer masks
    char m_layerJsonBuffer[4096] = {};
    // Import state used when loading layers from file
    struct LayerImportEntry {
        uint16_t Id = 0;
        std::string Name;
        uint32_t Mask = 0xFFFFFFFFu;
        bool HasMask = false;
        bool HasVisible = false;
        bool Visible = true;
        bool HasLocked = false;
        bool Locked = false;
    };

    std::vector<LayerImportEntry> m_pendingImport;
    bool m_showImportConfirm = false;

    // Simple status popup
    std::string m_statusMessage;
    enum class StatusType { Info = 0, Success = 1, Error = 2 };
    StatusType m_statusType = StatusType::Info;

    // Persistent rename buffers per layer ID to prevent ImGui from losing edits each frame
    std::unordered_map<uint16_t, std::array<char, 256>> m_renameBuffers;
};

#endif
