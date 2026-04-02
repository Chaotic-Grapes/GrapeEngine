/* Start Header *****************************************************************/
/*!
\file   AssetBrowserPanel.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   11th March 2026

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
    /**
     * @brief Available layouts for rendering asset entries.
     */
    enum class ViewMode : uint8_t {
        List = 0,
        Grid = 1
    };

    /**
     * @brief Callback signature invoked when asset selection changes.
     */
    using AssetSelectionCallback = std::function<void(const std::string&)>;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize panel dependencies and UI fonts.
     * @param mainFont Primary font used for body text.
     * @param boldFont Bold font used for highlighted labels.
     * @param symbolsFont Icon/symbol font used for glyph rendering.
     * @param world Active ECS world used by panel operations.
     */
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);

    /**
     * @brief Update the world reference after scene/world changes.
     * @param world Newly active ECS world.
     */
    void SetWorld(ECS::World* world);

    /**
     * @brief Attach inspector integration for prefab editing workflows.
     * @param inspector Inspector panel instance to notify.
     */
    void SetInspector(InspectorPanel* inspector);

    /**
     * @brief Provide editor settings for persisted panel preferences.
     * @param settings Editor settings object backing UI options.
     */
    void SetEditorSettings(EditorSettings* settings);

    /**
     * @brief Register a callback for selection-change notifications.
     * @param callback Function invoked with the selected asset path.
     */
    void SetSelectionChangedCallback(AssetSelectionCallback callback) { m_selectionCallback = std::move(callback); }

    /**
     * @brief Register a callback for scene-file open requests.
     * @param callback Function invoked with scene path to open.
     */
    void SetSceneOpenCallback(std::function<void(const std::string&)> callback) { m_sceneOpenCallback = std::move(callback); }

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    /**
     * @brief Render the full asset browser UI for the current frame.
     */
    void Render();

private:
    // -------------------------------------------------------------------------
    // UI Sections
    // -------------------------------------------------------------------------

    /**
     * @brief Draw the breadcrumb path bar and navigation controls.
     */
    void _renderNavigationBar();

    /**
     * @brief Draw toolbar actions such as import, replace, and delete.
     */
    void _renderActionButtons();

    /**
     * @brief Draw prefab-related quick action button(s).
     */
    void _renderPrefabButton();

    /**
     * @brief Draw the prefab popup used for load/edit operations.
     */
    void _renderPrefabPopup();

    /**
     * @brief Render the split content layout for list and inspector panes.
     */
    void _renderContentArea();

    /**
     * @brief Render the main file-list viewport region.
     * @param windowWidth Current panel width used for layout decisions.
     */
    void _renderFileListPanel(float windowWidth);

    /**
     * @brief Collect and sort directory entries for the active folder.
     * @return Sorted entries ordered by type and then by name.
     */
    std::vector<std::filesystem::directory_entry> _getSortedEntriesForCurrentPath() const;

    /**
     * @brief Render all entries as list rows.
     * @param entries Entries to draw for the active folder.
     */
    void _renderFileListEntries(const std::vector<std::filesystem::directory_entry>& entries);

    /**
     * @brief Compute a grid tile size suitable for current entry names.
     * @param entries Entries used to estimate required label area.
     * @param nameFont Font used to measure name labels.
     * @param nameFontSize Pixel size of filename text.
     * @param iconFontSize Pixel size of icon glyphs.
     * @param textPaddingX Horizontal text padding inside each tile.
     * @param tilePaddingY Vertical padding applied within each tile.
     * @param contentGap Spacing between icon and filename content.
     * @return Tile dimensions used by grid rendering.
     */
    ImVec2 _calculateGridTileSize(const std::vector<std::filesystem::directory_entry>& entries, ImFont* nameFont,
        float nameFontSize, float iconFontSize, float textPaddingX, float tilePaddingY, float contentGap) const;

    /**
     * @brief Render one grid tile entry with interaction handling.
     * @param entries Source entries for the current folder.
     * @param index Entry index to draw.
     * @param columns Number of grid columns currently visible.
     * @param tileWidth Width of each grid tile.
     * @param tileHeight Height of each grid tile.
     * @param tileSpacing Horizontal and vertical tile spacing.
     * @param tileRounding Corner radius used for tile background.
     * @param iconFontSize Pixel size for icon rendering.
     * @param nameFontSize Pixel size for filename rendering.
     * @param textPaddingX Horizontal text padding.
     * @param contentGap Gap between icon and text.
     * @param nameFont Font used to draw names.
     */
    void _renderFileGridEntry(const std::vector<std::filesystem::directory_entry>& entries, size_t index, int columns,
        float tileWidth, float tileHeight, float tileSpacing, float tileRounding, float iconFontSize, float nameFontSize,
        float textPaddingX, float contentGap, ImFont* nameFont);

    /**
     * @brief Render all entries in a tiled grid layout.
     * @param entries Entries to render.
     */
    void _renderFileGridEntries(const std::vector<std::filesystem::directory_entry>& entries);

    /**
     * @brief Render label text for a grid tile entry.
     * @param entry Directory entry represented by the tile.
     * @param entryPath Absolute or project-relative entry path.
     * @param isRenaming True when inline rename UI is active.
     * @param contentTop Top y-coordinate of content area.
     * @param iconSize Rendered icon size.
     * @param contentGap Gap between icon and text block.
     * @param textPaddingX Horizontal text padding.
     * @param tileWidth Width allocated to the tile.
     * @param nameFont Font used for filename rendering.
     * @param nameFontSize Pixel size used for filename text.
     * @param nameSize Measured label dimensions.
     * @param displayName Final name string displayed to user.
     */
    void _renderGridEntryLabel(const std::filesystem::directory_entry& entry, const std::string& entryPath,
        bool isRenaming, float contentTop, const ImVec2& iconSize, float contentGap, float textPaddingX, float tileWidth,
        ImFont* nameFont, float nameFontSize, const ImVec2& nameSize, const std::string& displayName);

    /**
     * @brief Process interaction events for a grid tile.
     * @param entries All entries in the current folder.
     * @param entryPath Path of the interacted entry.
     * @param isSelected True when entry is currently selected.
     * @param isDirectory True when entry is a directory.
     * @param extLower Lowercased file extension for type handling.
     * @param tileLeftClick True when tile received a left click.
     * @param tileDoubleClick True when tile received a double-click.
     * @param tileRightClick True when tile received a right click.
     * @param tileHovered True when cursor hovers the tile.
     */
    void _handleGridEntryInteractions(const std::vector<std::filesystem::directory_entry>& entries,
        const std::string& entryPath, bool isSelected, bool isDirectory, const std::string& extLower, bool tileLeftClick,
        bool tileDoubleClick, bool tileRightClick, bool tileHovered);

    /**
     * @brief Render metadata/details for current selection.
     */
    void _renderFileInfoPanel();

    /**
     * @brief Render contextual delete button in panel footer.
     */
    void _renderDeleteButton();

    /**
     * @brief Render transient status message area.
     */
    void _renderStatusBar();

    /**
     * @brief Render modal dialog for creating new assets.
     */
    void _renderCreateDialog();

    // -------------------------------------------------------------------------
    // Prefab Operations and Selection
    // -------------------------------------------------------------------------

    /**
     * @brief Load selected prefab and open it in inspector.
     */
    void _loadPrefab();

    /**
     * @brief Enter inspector edit mode for selected prefab.
     */
    void _editPrefab();

    /**
     * @brief Handle empty-space clicks and clear selection state.
     */
    void _selectEmptySpace();

    /**
     * @brief Clear multiselection and selection anchor.
     */
    void _clearSelection();

    /**
     * @brief Select a single entry and optionally update anchor.
     * @param entryPath Path to the entry to select.
     * @param updateAnchor True to update shift-selection anchor.
     */
    void _setSingleSelection(const std::string& entryPath, bool updateAnchor);

    /**
     * @brief Notify listeners that the asset selection changed.
     */
    void _notifySelectionChanged() const;

    /**
     * @brief Handle double-click navigation/open behavior for an entry.
     * @param entryPath Path of the activated entry.
     * @param extLower Lowercased extension for file type routing.
     * @param isDirectory True when entry is a directory.
     * @return True when the double-click was consumed.
     */
    bool _handleEntryDoubleClick(const std::string& entryPath, const std::string& extLower, bool isDirectory);

    /**
     * @brief Apply click-selection rules with keyboard modifiers.
     * @param entries Entries visible in the current folder.
     * @param entryPath Path of clicked entry.
     * @param isSelected True when entry was already selected.
     * @param isDoubleClick True when interaction was a double-click.
     * @param isDirectory True when entry is a directory.
     * @param extLower Lowercased extension for open behavior checks.
     */
    void _applyEntrySelection(const std::vector<std::filesystem::directory_entry>& entries, const std::string& entryPath,
        bool isSelected, bool isDoubleClick, bool isDirectory, const std::string& extLower);

    /**
     * @brief Auto-open hovered folders while dragging payloads.
     * @param folderPath Hovered folder path.
     * @param isHoveredWithPayload True while payload hovers target folder.
     */
    void _handleFolderHoverAutoOpen(const std::string& folderPath, bool isHoveredWithPayload);

    /**
     * @brief Commit inline rename operation to the filesystem.
     * @param entry Directory entry currently being renamed.
     * @param entryPath Original path of the entry.
     * @return True when rename succeeded.
     */
    bool _commitRename(const std::filesystem::directory_entry& entry, const std::string& entryPath);

    // -------------------------------------------------------------------------
    // Context Menu
    // -------------------------------------------------------------------------

    /**
     * @brief Render context menu for creating new assets.
     */
    void _renderContextMenu();

    /**
     * @brief Render context menu actions for selected entries.
     */
    void _renderItemContextMenu();

    /**
     * @brief Create a script file from selected template.
     */
    void _createScript();

    /**
     * @brief Create a new scene asset in current directory.
     */
    void _createScene();

    /**
     * @brief Create a new folder in current directory.
     */
    void _createFolder();

    /**
     * @brief Draw create-menu entries and return whether one was chosen.
     * @return True when a create action was selected.
     */
    bool _renderCreateMenuItems();

    /**
     * @brief Open generated C# project and optionally focus a script.
     * @param fileToOpen Optional script path to focus after opening.
     */
    void _openProjectFile(const std::string& fileToOpen = std::string());

    /**
     * @brief Reveal an asset in the platform file explorer.
     * @param assetPath Path of asset to reveal.
     */
    void _openInExplorer(const std::string& assetPath);

    // -------------------------------------------------------------------------
    // Copy/Paste Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Copy current selection to internal clipboard.
     */
    void _copySelectedAssets();

    /**
     * @brief Paste clipboard assets into current directory.
     */
    void _pasteAssets();

    /**
     * @brief Delete all currently selected assets.
     */
    void _deleteSelectedAssets();

    /**
     * @brief Begin rename workflow for single selected asset.
     */
    void _startRename();

    // -------------------------------------------------------------------------
    // Drag-Drop Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Start drag payload for selected asset(s).
     * @param assetPath Primary asset path initiating drag.
     */
    void _handleAssetDragDrop(const std::string& assetPath);

    /**
     * @brief Accept drop payload onto a folder target.
     * @param folderPath Destination folder path.
     */
    void _handleFolderDropTarget(const std::string& folderPath);

    /**
     * @brief Move assets to a destination directory.
     * @param assets Asset paths to move.
     * @param targetDir Destination folder path.
     */
    void _moveAssetsToDirectory(const std::vector<std::string>& assets, const std::string& targetDir);

    /**
     * @brief Copy assets to a destination directory.
     * @param assets Asset paths to copy.
     * @param targetDir Destination folder path.
     */
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
    bool m_clipboardIsCut = false; // True if assets were cut (vs copied)

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

    // Current view mode; defaults to Grid
    ViewMode m_viewMode = ViewMode::Grid;
    EditorSettings* m_editorSettings = nullptr;
};

#endif