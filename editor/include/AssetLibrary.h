/* Start Header *****************************************************************/
/*!
\file   AssetLibrary.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   11th March 2026

\brief
Declares the AssetLibrary which powers all asset browser file operations.

Provides:
- Folder navigation and file listing
- Import and replace using OS dialogs
- Drag-drop import support
- File info display for the inspector panel
- Syncing assets between ../assets and build/assets
- Safe deletion for files and folders
*/
/* End Header *******************************************************************/

#ifndef ASSET_LIBRARY_H
#define ASSET_LIBRARY_H

#include <string>
#include <filesystem>
#include <imgui.h>

// So we can write something like ECS::World* m_world without including the World header
namespace ECS { class World; }

// Forward declarations
struct ImFont;
class AssetBrowserPanel;            // Forward declare to use as friend
class InspectorPanel;               // Forward declare to wire double-click open

class AssetLibrary {
    friend class AssetBrowserPanel; // Only AssetBrowserPanel can access private members

public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize fonts and editor references for asset UI helpers.
     * @param mainFont Primary text font.
     * @param boldFont Bold text font.
     * @param symbolsFont Symbol/icon font.
     */
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    /**
     * @brief Attach inspector integration for prefab open actions.
     * @param inspector Inspector panel used for prefab workflows.
     */
    void SetInspector(InspectorPanel* inspector) { m_inspector = inspector; }

    /**
     * @brief Update world reference after scene/world changes.
     * @param world Active ECS world.
     */
    void SetWorld(ECS::World* world) { m_world = world; }

private:
    // -------------------------------------------------------------------------
    // Navigation and Display
    // -------------------------------------------------------------------------

    /**
     * @brief Render clickable breadcrumb path UI.
     * @param currentPath Currently visible folder path.
     * @param selectedAsset Currently selected asset path.
     * @param outNewPath Output path when user navigates to another folder.
     */
    void _displayBreadcrumbs(const std::string& currentPath, std::string& selectedAsset, std::string& outNewPath);

    /**
     * @brief Render directory contents and handle selection/navigation.
     * @param folderPath Folder to enumerate.
     * @param selectedAsset Selected asset path, updated by interaction.
     * @param currentPath Current folder path, updated on navigation.
     */
    void _displayFolder(const std::filesystem::path& folderPath, std::string& selectedAsset, std::string& currentPath);

    /**
     * @brief Render one file entry and process selection input.
     * @param filePath File to display.
     * @param selectedAsset Selected asset path, updated on click.
     */
    void _displayFile(const std::filesystem::path& filePath, std::string& selectedAsset);

    /**
     * @brief Render metadata and actions for selected asset.
     * @param selectedAsset Path of currently selected asset.
     */
    void _displaySelectedFileInfo(const std::string& selectedAsset);

    // -------------------------------------------------------------------------
    // Import Replace Delete
    // -------------------------------------------------------------------------

    /**
     * @brief Import an asset via operating-system file picker.
     * @param currentPath Destination folder path.
     * @param selectedAsset Selected asset output path.
     * @param statusMessage UI status message buffer.
     * @param statusTimer Timer controlling status visibility.
     */
    void _importAsset(const std::string& currentPath, std::string& selectedAsset,
        std::string& statusMessage, float& statusTimer);

    /**
     * @brief Replace currently selected texture with imported file.
     * @param selectedAsset Path to texture being replaced.
     * @param statusMessage UI status message buffer.
     * @param statusTimer Timer controlling status visibility.
     */
    void _replaceTexture(const std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    /**
     * @brief Import file dropped from external OS drag-and-drop.
     * @param sourcePath Source file dropped into editor.
     * @param currentPath Destination folder path.
     * @param selectedAsset Selected asset output path.
     * @param statusMessage UI status message buffer.
     * @param statusTimer Timer controlling status visibility.
     */
    void _handleFileDrop(const std::string& sourcePath, const std::string& currentPath,
        std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    /**
     * @brief Shared import routine used by dialog and file-drop workflows.
     * @param sourcePath Source file path to import.
     * @param currentPath Destination folder path.
     * @param selectedAsset Selected asset output path.
     * @param statusMessage UI status message buffer.
     * @param statusTimer Timer controlling status visibility.
     */
    void _importFromSourcePath(const std::filesystem::path& sourcePath, const std::string& currentPath,
        std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    /**
     * @brief Delete selected asset (file or folder) and clear selection.
     * @param selectedAsset Selected asset path to delete.
     * @param statusMessage UI status message buffer.
     * @param statusTimer Timer controlling status visibility.
     */
    void _deleteSelectedAsset(std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    // Fonts used for all UI text and icons
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // System references
    InspectorPanel* m_inspector = nullptr;
    ECS::World* m_world = nullptr;
};

#endif