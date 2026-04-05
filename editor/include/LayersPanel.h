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

    /**
     * @brief Initialize fonts and subscribe to scene modification events.
     * @param mainFont Primary UI font.
     * @param boldFont Bold UI font.
     * @param symbolsFont Icon/symbol font.
     */
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    /**
     * @brief Set the active ECS world used for layer operations.
     * @param world Pointer to the ECS world.
     */
    void SetWorld(ECS::World* world) { m_world = world; }

    /**
     * @brief Set the active scene whose layer data is displayed and edited.
     * @param scene Pointer to the scene.
     */
    void SetScene(Scenes::Scene* scene);

    /**
     * @brief Inject the file menu dependency for marking the scene dirty on edits.
     * @param fileMenu Pointer to the editor file menu.
     */
    void SetFileMenu(EditorFileMenu* fileMenu) { m_fileMenu = fileMenu; }

    /**
     * @brief Inject the hierarchy panel used to refresh entity layer assignments.
     * @param h Pointer to the hierarchy panel.
     */
    void SetHierarchy(HierarchyPanel* h) { m_hierarchy = h; }

    /**
     * @brief Inject the entity actions helper used for batch layer assignment.
     * @param e Pointer to the entity actions instance.
     */
    void SetEntityActions(EntityActions* e) { m_entityActions = e; }

    /**
     * @brief Inject the undo system for recording reversible layer edits.
     * @param u Pointer to the undo system.
     */
    void SetUndoSystem(Editor::UndoSystem* u) { m_undoSystem = u; }

    /**
     * @brief Move a layer from one position to another in the layer order.
     * @param fromId Source layer ID.
     * @param toId Destination layer ID.
     */
    void MoveLayer(uint16_t fromId, uint16_t toId);

    /**
     * @brief Create a new named layer and return its assigned ID.
     * @param name Display name for the new layer.
     * @return ID assigned to the newly created layer.
     */
    uint16_t CreateLayer(const std::string& name);

    /**
     * @brief Assign an entity to the given layer.
     * @param entity Entity to reassign.
     * @param layerId Target layer ID.
     */
    void SetLayer(EntityId entity, uint16_t layerId);

    /** @brief Render the full layers panel UI (header, list, collision matrix, footer). */
    void Render();

private:
    // Render helpers split out from the large Render() method

    /** @brief Render the panel header showing the layer count and action buttons. */
    void _renderHeader();

    /** @brief Render the layer-vs-layer collision matrix grid. */
    void _renderCollisionMatrix();

    /** @brief Render the scrollable list of layers with rename and visibility controls. */
    void _renderLayersList();

    /** @brief Render the footer buttons (import/export JSON, add/delete layer). */
    void _renderFooterButtons();

    /** @brief Render the import-confirmation dialog before applying JSON layer data. */
    void _renderImportConfirm();

    /** @brief Render the transient status popup showing the result of the last operation. */
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
