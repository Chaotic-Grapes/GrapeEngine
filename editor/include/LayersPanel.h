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
#include "scene/Scene.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
// Forward declare EditorFileMenu and other editor helpers to avoid include dependency
class EditorFileMenu;
class HierarchyPanel;
class EntityActions;

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

    void Render();

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
    
    // Subscription to scene modification messages so the panel can refresh
    Messaging::SubscriptionHandle m_sceneModifiedSubscription;
    bool m_needsRefresh = false;
};

#endif
