/* Start Header *****************************************************************/
/*!
\file   AssetLibrary.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Handles file system operations and asset management utilities for the asset browser.

Features:
- File and folder browsing operations
- Asset import and replacement with hot reload
- File information display
- OS file drop handling
- Asset deletion

References:
- Windows file dialog using Win32 API (commdlg.h)
- std::filesystem for cross-platform file operations
*/
/* End Header *******************************************************************/

#ifndef ASSETLIBRARY_H
#define ASSETLIBRARY_H

#include <string>
#include <filesystem>
#include <imgui.h>

// Forward declarations
struct ImFont;
class AssetBrowser;            // Forward declare to use as friend
class InspectorWindow;         // Forward declare to wire double-click open

class AssetLibrary {
    friend class AssetBrowser; // Only AssetBrowser can access private members

public:
    // Initialize with fonts for rendering
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    // Wire the unified Inspector to enable double-click open for prefabs
    void SetInspector(InspectorWindow* inspector) { m_inspector = inspector; }

private:
    // Display clickable breadcrumb navigation trail
    void _displayBreadcrumbs(const std::string& currentPath, std::string& selectedAsset, std::string& outNewPath);

    // Display folder contents (files and subfolders)
    void _displayFolder(const std::filesystem::path& folderPath, std::string& selectedAsset, std::string& currentPath);

    // Display a single file entry as selectable item
    void _displayFile(const std::filesystem::path& filePath, std::string& selectedAsset);

    // Display info about selected file in right panel
    void _displaySelectedFileInfo(const std::string& selectedAsset);

    // Import new asset file into current folder
    void _importAsset(const std::string& currentPath, std::string& selectedAsset,
        std::string& statusMessage, float& statusTimer);

    // Replace the currently selected texture file (with hot reload)
    void _replaceTexture(const std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    // Handle file dropped from OS
    void _handleFileDrop(const std::string& sourcePath, const std::string& currentPath,
        std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    // Delete the currently selected file or folder
    void _deleteSelectedAsset(std::string& selectedAsset, std::string& statusMessage, float& statusTimer);

    // Member variables
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    InspectorWindow* m_inspector = nullptr;
};

#endif
