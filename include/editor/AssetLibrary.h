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
    // This stores fonts used by the asset library UI
    // It prepares helpers for drawing labels and icons
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    // This connects the inspector window
    // It enables opening prefabs from file entries
    void SetInspector(InspectorWindow* inspector) { m_inspector = inspector; }

private:
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

    // Member variables
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    InspectorWindow* m_inspector = nullptr;
};

#endif
