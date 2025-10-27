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
#include <nlohmann/json.hpp>

// Forward declaration
struct ImFont;
class World;

class AssetBrowser {
public:
    // Initialize with symbols font for icons and world reference
    void Initialize(ImFont* symbolsFont, World* world);

    // Render the asset browser UI
    void Render();

private:
    // Success notification
    std::string m_statusMessage = "";
    float m_statusTimer = 0.0f;

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

    // Replace the currently selected texture file
    void _replaceTexture();

    // Loads a prefab file from disk and instantiates it into the world
    void _loadPrefab();

    // Opens the selected prefab in an editor for modification
    void _editPrefab();

    // Updates all entities that were instantiated from prefabs to match
    // their latest prefab definitions (used for synchronization)
    void _updatePrefabInstances();

    // References and state for managing assets, prefabs and editor navigation
    ImFont* m_symbolsFont = nullptr;
    World* m_world = nullptr;
    std::string m_assetsRootPath = "assets\\";
    std::string m_currentPath = "assets\\";
    std::string m_selectedAsset;

    // Prefab editing state
    bool m_editingPrefab = false;            
    nlohmann::json m_prefabData;              
    std::string m_editingPrefabPath;
};

#endif