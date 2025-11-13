/* Start Header *****************************************************************/
/*!
\file   AssetBrowserPanel.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Declares the AssetBrowserPanel class for browsing and managing game assets.
*/
/* End Header *******************************************************************/

#pragma once

#include "ecs/World.h"
#include "../editor/AssetLibrary.h"
#include <imgui.h>
#include <string>

// Forward declarations
class InspectorPanel;

// Asset browser panel for file navigation and asset management
class AssetBrowserPanel {
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);
    void SetWorld(ECS::World* world);
    void SetInspector(InspectorPanel* inspector);

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------
    void Render();

private:
    // -------------------------------------------------------------------------
    // UI Sections
    // -------------------------------------------------------------------------
    void _renderNavigationBar();
    void _renderActionButtons();
    void _renderPrefabButton();
    void _renderPrefabPopup();
    void _renderContentArea();
    void _renderFileListPanel(float windowWidth);
    void _renderFileInfoPanel();
    void _renderDeleteButton();
    void _renderStatusBar();

    // -------------------------------------------------------------------------
    // Prefab Operations & Selection Invalidation
    // -------------------------------------------------------------------------
    void _loadPrefab();
    void _editPrefab();
    void _selectEmptySpace();

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    ECS::World* m_world = nullptr;
    InspectorPanel* m_inspector = nullptr;

    AssetLibrary m_assetLibrary;
    std::string m_currentPath = "assets";
    std::string m_selectedAsset;
    std::string m_statusMessage;
    float m_statusTimer = 0.0f;
};
