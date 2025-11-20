/* Start Header *****************************************************************/
/*!
\file   AssetBrowserPanel.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025

\brief
Declares the AssetBrowserPanel which renders the asset browser UI.

Provides:
- Breadcrumb navigation and folder traversal
- File listing with selection and info display
- Import replace and delete actions
- Prefab load and edit workflow
- Status bar updates and file-drop handling
- Integration with AssetLibrary and InspectorPanel
*/
/* End Header *******************************************************************/

#ifndef ASSET_BROWSER_PANEL_H
#define ASSET_BROWSER_PANEL_H

#include "ecs/World.h"
#include "AssetLibrary.h"
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

    // Initialize fonts world reference and prepare the asset library
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);

    // Update world reference when switching scenes
    void SetWorld(ECS::World* world);

    // Connect inspector so double-click can open prefabs
    void SetInspector(InspectorPanel* inspector);

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    // Render the entire asset browser window
    // Draws navigation file list file info and status bar
    void Render();

private:
    // -------------------------------------------------------------------------
    // UI Sections
    // -------------------------------------------------------------------------

    // Render breadcrumb bar and path controls
    void _renderNavigationBar();

    // Render import replace and delete buttons
    void _renderActionButtons();

    // Render button for opening prefabs
    void _renderPrefabButton();

    // Render popup for loading and editing prefab assets
    void _renderPrefabPopup();

    // Render the main two panel layout
    void _renderContentArea();

    // Render the file list on the left
    void _renderFileListPanel(float windowWidth);

    // Render detailed info for the selected asset
    void _renderFileInfoPanel();

    // Render delete button at bottom right
    void _renderDeleteButton();

    // Render status message bar
    void _renderStatusBar();

    // -------------------------------------------------------------------------
    // Prefab Operations and Selection
    // -------------------------------------------------------------------------

    // Load a prefab file and open it in the inspector
    void _loadPrefab();

    // Switch the inspector into edit mode for the selected prefab
    void _editPrefab();

    // Handle clicking empty space to clear selection
    void _selectEmptySpace();

    // -------------------------------------------------------------------------
    // Context Menu
    // -------------------------------------------------------------------------

    // Render right-click context menu for creating assets
    void _renderContextMenu();

    // Create a new C# script file with template
    void _createScript();

    // Create a new scene file
    void _createScene();

    // Create a new folder
    void _createFolder();

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    // Fonts used for all UI text and icons
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // System references
    ECS::World* m_world = nullptr;
    InspectorPanel* m_inspector = nullptr;

    // Asset browsing state
    AssetLibrary m_assetLibrary;
    std::string m_currentPath = "assets";
    std::string m_selectedAsset;

    // Status bar state
    std::string m_statusMessage;
    float m_statusTimer = 0.0f;

    // Context menu state
    bool m_showContextMenu = false;
    char m_newAssetNameBuffer[128] = "";
    bool m_focusNameInput = false;
    enum class AssetCreationType { None, Script, Scene, Folder };
    AssetCreationType m_creationType = AssetCreationType::None;
};

#endif