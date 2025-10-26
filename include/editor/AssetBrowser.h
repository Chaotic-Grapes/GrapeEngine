/* Start Header *****************************************************************/
/*!
\file   AssetBrowser.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Declares the AssetBrowser class for browsing and managing game assets in the
level editor.

Features:
- File browser UI showing assets folder structure
- Breadcrumb navigation
- File selection with info display
- Asset import, replacement and hot reload
*/
/* End Header *******************************************************************/

#ifndef ASSETBROWSER_H
#define ASSETBROWSER_H

#include <string>
#include <filesystem>

// Forward declaration
struct ImFont;

class AssetBrowser {
public:
    // Initialize with symbols font for icons
    void Initialize(ImFont* symbolsFont);

    // Render the asset browser UI
    void Render();

private:
    // Display clickable breadcrumb navigation trail
    void _displayBreadcrumbs();

    // Display folder contents
    void _displayFolder(const std::filesystem::path& folderPath);

    // Display a single file entry
    void _displayFile(const std::filesystem::path& filePath);

    // Display info about selected file
    void _displaySelectedFileInfo();

    // Import new texture file into current folder
    void _importTexture();

    ImFont* m_symbolsFont = nullptr;
    std::string m_assetsRootPath = "assets/";
    std::string m_currentPath = "assets/";
    std::string m_selectedAsset;
};

#endif