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

    // Initialize fonts and prepare UI helpers for drawing labels and icons
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    // Connect the inspector to enable opening prefabs from file entries
    void SetInspector(InspectorPanel* inspector) { m_inspector = inspector; }

    // Update world reference when switching scenes
    void SetWorld(ECS::World* world) { m_world = world; }

private:
    // -------------------------------------------------------------------------
    // Navigation and Display
    // -------------------------------------------------------------------------

    // Render breadcrumb path bar and handle click-to-navigate
    void _displayBreadcrumbs(const std::string& currentPath, std::string& selectedAsset, std::string& outNewPath);

    // List subfolders and files inside a folder, handling selection and directory entry
    void _displayFolder(const std::filesystem::path& folderPath, std::string& selectedAsset, std::string& currentPath);

    // Render a single file entry and allow selection
    void _displayFile(const std::filesystem::path& filePath, std::string& selectedAsset);

    // Display type, size, and quick actions for the currently selected file
    void _displaySelectedFileInfo(const std::string& selectedAsset);

    // -------------------------------------------------------------------------
    // Import Replace Delete
    // -------------------------------------------------------------------------

    // Open an OS file dialog to import a new asset into the current folder
    void _importAsset(const std::string& currentPath, std::string& selectedAsset,
        std::string& statusMessage, float& statusTimer);

    // Replace the selected texture with a new file, triggering hot reload
    void _replaceTexture(const std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    // Handle a file dropped from the OS, copying it into the project
    void _handleFileDrop(const std::string& sourcePath, const std::string& currentPath,
        std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    // Shared import path used by both file dialog import and OS drag-drop import
    // Validates destination and resolves name conflicts before copying
    void _importFromSourcePath(const std::filesystem::path& sourcePath, const std::string& currentPath,
        std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    // Delete the selected file or folder and clear selection
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