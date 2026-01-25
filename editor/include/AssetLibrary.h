/* Start Header *****************************************************************/
/*!
\file   AssetLibrary.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   11th January 2026

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

    // This stores fonts used by the asset library UI
    // It prepares helpers for drawing labels and icons
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    // This connects the inspector window
    // It enables opening prefabs from file entries
    void SetInspector(InspectorPanel* inspector) { m_inspector = inspector; }
    void SetWorld(ECS::World* world) { m_world = world; }

private:
    // -------------------------------------------------------------------------
    // Navigation and Display
    // -------------------------------------------------------------------------

    // This shows the breadcrumb path bar
    // It lets you click to navigate and updates selection
    void _displayBreadcrumbs(const std::string& currentPath, std::string& selectedAsset, std::string& outNewPath);

    // This lists subfolders and files inside a folder
    // It handles selection and allows entering directories
    void _displayFolder(const std::filesystem::path& folderPath, std::string& selectedAsset, std::string& currentPath);

    // This draws one file entry
    // It lets you select and view info for that file
    void _displayFile(const std::filesystem::path& filePath, std::string& selectedAsset);

    // This shows details for the chosen file
    // It displays type size and quick actions
    void _displaySelectedFileInfo(const std::string& selectedAsset);

    // -------------------------------------------------------------------------
    // Import Replace Delete
    // -------------------------------------------------------------------------

    // This brings a new file into the current folder
    // It sets status text and updates selection
    void _importAsset(const std::string& currentPath, std::string& selectedAsset,
        std::string& statusMessage, float& statusTimer);

    // This swaps the selected texture with a new one
    // It triggers hot reload and shows status
    void _replaceTexture(const std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    // This handles a file dropped from the operating system
    // It copies or moves it into the project and updates status
    void _handleFileDrop(const std::string& sourcePath, const std::string& currentPath,
        std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    // This removes the chosen file or folder
    // It updates status and clears selection
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