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
- Folder navigation
- File selection
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
    AssetBrowser();
    ~AssetBrowser();

    // Initialize with symbols font for icons
    void Initialize(ImFont* symbolsFont);

    // Render the asset browser UI
    void Render();

private:
    // Display folder contents
    void _displayFolder(const std::filesystem::path& folderPath);

    // Display a single file entry
    void _displayFile(const std::filesystem::path& filePath);

    ImFont* m_symbolsFont = nullptr;
    std::string m_assetsRootPath = "assets/";
    std::string m_currentPath = "assets/";
    std::string m_selectedAsset;
};

#endif