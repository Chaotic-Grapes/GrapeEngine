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
#include "EditorConfiguration.h"
#include "ScriptTemplates.h"
#include <imgui.h>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <functional>
#include <vector>

// Forward declarations
class InspectorPanel;

// Asset browser panel for file navigation and asset management
class AssetBrowserPanel {
public:
	// View modes for asset display (list vs grid)
    enum class ViewMode : uint8_t {
        List = 0,
        Grid = 1
    };

    // Callback for asset selection changes (used by other editor systems).
    using AssetSelectionCallback = std::function<void(const std::string&)>;
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    // Initialize fonts world reference and prepare the asset library
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);

    // Update world reference when switching scenes
    void SetWorld(ECS::World* world);

    // Connect inspector so double-click can open prefabs
    void SetInspector(InspectorPanel* inspector);

	// Update editor settings reference for view mode and other config
    void SetEditorSettings(EditorSettings* settings);

    // Register a callback for asset selection changes
    void SetSelectionChangedCallback(AssetSelectionCallback callback) { m_selectionCallback = std::move(callback); }

    // Register a callback for scene file double-click opens
    void SetSceneOpenCallback(std::function<void(const std::string&)> callback) { m_sceneOpenCallback = std::move(callback); }

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

    // Render the main file list panel, switching between list and grid view based on view mode
    void _renderFileListPanel(float windowWidth);

    // Return directory entries for the current path sorted by type then name
    std::vector<std::filesystem::directory_entry> _getSortedEntriesForCurrentPath() const;

    // Render all entries in the current directory as list rows
    void _renderFileListEntries(const std::vector<std::filesystem::directory_entry>& entries);

    // Calculate the tile dimensions that fit the longest filename in the current entry set
    ImVec2 _calculateGridTileSize(const std::vector<std::filesystem::directory_entry>& entries, ImFont* nameFont,
        float nameFontSize, float iconFontSize, float textPaddingX, float tilePaddingY, float contentGap) const;
    
    // Render a single grid tile entry at the given index with icon and label
    void _renderFileGridEntry(const std::vector<std::filesystem::directory_entry>& entries, size_t index, int columns,
        float tileWidth, float tileHeight, float tileSpacing, float tileRounding, float iconFontSize, float nameFontSize,
        float textPaddingX, float contentGap, ImFont* nameFont);

    // Render all entries in the current directory as a grid of tiles
    void _renderFileGridEntries(const std::vector<std::filesystem::directory_entry>& entries);
    
    // Render the truncated or wrapped name label below a grid tile's icon
    void _renderGridEntryLabel(const std::filesystem::directory_entry& entry, const std::string& entryPath, 
        bool isRenaming, float contentTop, const ImVec2& iconSize, float contentGap, float textPaddingX, float tileWidth, 
        ImFont* nameFont, float nameFontSize, const ImVec2& nameSize, const std::string& displayName);

    // Handle click, double-click, right-click and hover interactions for a grid tile
    void _handleGridEntryInteractions(const std::vector<std::filesystem::directory_entry>& entries, 
        const std::string& entryPath, bool isSelected, bool isDirectory, const std::string& extLower, bool tileLeftClick,
        bool tileDoubleClick, bool tileRightClick, bool tileHovered);

    // Render detailed info for the selected asset
    void _renderFileInfoPanel();

    // Render delete button at bottom right
    void _renderDeleteButton();

    // Render status message bar
    void _renderStatusBar();

    // Render the create asset dialog popup
    void _renderCreateDialog();

    // -------------------------------------------------------------------------
    // Prefab Operations and Selection
    // -------------------------------------------------------------------------

    // Load a prefab file and open it in the inspector
    void _loadPrefab();

    // Switch the inspector into edit mode for the selected prefab
    void _editPrefab();

    // Handle clicking empty space to clear selection
    void _selectEmptySpace();

    // Clear the current multi-selection and reset the anchor
    void _clearSelection();

    // Set selection anchor and notify listeners when a single entry is selected
    void _setSingleSelection(const std::string& entryPath, bool updateAnchor);

    // Notify the registered callback that the selection has changed
    void _notifySelectionChanged() const;

    // Handle double-click on an entry, opening folders or assets as appropriate
    bool _handleEntryDoubleClick(const std::string& entryPath, const std::string& extLower, bool isDirectory);

    // Apply selection logic for an entry, accounting for shift/ctrl modifiers and double-clicks
    void _applyEntrySelection(const std::vector<std::filesystem::directory_entry>& entries, const std::string& entryPath,
        bool isSelected, bool isDoubleClick, bool isDirectory, const std::string& extLower);

    // Track hover time on a folder and auto-navigate into it when dragging an asset over it
    void _handleFolderHoverAutoOpen(const std::string& folderPath, bool isHoveredWithPayload);

    // Validate and apply the pending rename, updating the file on disk
    bool _commitRename(const std::filesystem::directory_entry& entry, const std::string& entryPath);

    // -------------------------------------------------------------------------
    // Context Menu
    // -------------------------------------------------------------------------

    // Render right-click context menu for creating assets
    void _renderContextMenu();

    // Render right-click context menu for selected items
    void _renderItemContextMenu();

    // Create a new C# script file with template
    void _createScript();

    // Create a new scene file
    void _createScene();

    // Create a new folder
    void _createFolder();

    // Right-click context menu for asset creation
    bool _renderCreateMenuItems();

    // Open the generated C# project file (optionally focusing a script file).
    void _openProjectFile(const std::string& fileToOpen = std::string());

    // Show the selected asset in Windows Explorer
    void _openInExplorer(const std::string& assetPath);

    // -------------------------------------------------------------------------
    // Copy/Paste Operations
    // -------------------------------------------------------------------------

    // Copy selected assets to clipboard
    void _copySelectedAssets();

    // Paste clipboard assets to current directory
    void _pasteAssets();

    // Delete selected assets
    void _deleteSelectedAssets();

    // Rename selected asset (single selection only)
    void _startRename();

    // -------------------------------------------------------------------------
    // Drag-Drop Operations
    // -------------------------------------------------------------------------

    // Handle dragging selected assets
    void _handleAssetDragDrop(const std::string& assetPath);

    // Handle dropping assets onto a folder
    void _handleFolderDropTarget(const std::string& folderPath);

    // Move assets to target directory
    void _moveAssetsToDirectory(const std::vector<std::string>& assets, const std::string& targetDir);

    // Copy assets to target directory
    void _copyAssetsToDirectory(const std::vector<std::string>& assets, const std::string& targetDir);

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
    std::string m_currentPath;
    std::string m_selectedAsset;
    std::unordered_set<std::string> m_selectedAssets; // Multi-selection support
    std::string m_anchorAsset;                        // For shift-selection
    AssetSelectionCallback m_selectionCallback;       // Selection change callback
    std::function<void(const std::string&)> m_sceneOpenCallback;

    // Clipboard state
    std::vector<std::string> m_clipboardAssets;
    bool m_clipboardIsCut = false;

    // Status bar state
    std::string m_statusMessage;
    float m_statusTimer = 0.0f;

    // Context menu state
    bool m_openCreateDialog = false;
    char m_newAssetNameBuffer[128] = "";
    bool m_focusNameInput = false;

    // Asset creation dialog state
    enum class AssetCreationType : uint8_t { NONE, SCRIPT, SCENE, FOLDER }; // Tracks which asset type the user is currently creating
    AssetCreationType m_creationType = AssetCreationType::NONE;             // Defaults to NONE
    Editor::Templates::ScriptTemplateType m_selectedScriptTemplate = Editor::Templates::ScriptTemplateType::BasicSystem; // Persistence

    // Rename state
    std::string m_renamingAsset;
    char m_renameBuffer[256] = "";
    bool m_focusRenameInput = false;

	// View mode state (list vs grid)
    ViewMode m_viewMode = ViewMode::Grid;
    EditorSettings* m_editorSettings = nullptr;
};

#endif
