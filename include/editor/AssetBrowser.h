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
- Prefab editing with instance synchronization
- Generalized UI helpers for component property editing

References:
- ImGui documentation for UI widgets and styling
- Lambda functions for flexible component rendering
*/
/* End Header *******************************************************************/

#ifndef ASSETBROWSER_H
#define ASSETBROWSER_H

#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <imgui.h>

// Forward declarations
struct ImFont;
class World;
class Entity;

class AssetBrowser {
public:
    // Initialize with symbols font for icons and world reference
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world);

    // Render the asset browser UI
    void Render();

private:
    // Display clickable breadcrumb navigation trail
    void _displayBreadcrumbs();

    // Display folder contents (files and subfolders)
    void _displayFolder(const std::filesystem::path& folderPath);

    // Display a single file entry as selectable item
    void _displayFile(const std::filesystem::path& filePath);

    // Display info about selected file in right panel
    void _displaySelectedFileInfo();

    // Import new asset file into current folder
    void _importAsset();

    // Replace the currently selected texture file (with hot reload)
    void _replaceTexture();

    // Load prefab file from disk and instantiate it into the world
    void _loadPrefab();

    // Open prefab editor (loads JSON and sets m_editingPrefab flag)
    void _editPrefab();

    // Update all entities instantiated from this prefab to match latest definition
    void _updatePrefabInstances();

    // Helper for updating a single entity from prefab data
    bool _updateEntityFromPrefab(Entity& entity);

    // Render a property with X and Y fields (Position, Scale, Velocity, Size, etc.)
    void _renderVector2DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, float dragSpeed = 1.0f,
        float labelOffset = 20.0f);

    // Render a single float property with custom field label (Mass, Rotation, Volume, etc.)
    void _renderFloatRow(const std::string& label, const std::string& fieldLabel,
        nlohmann::json& data, const std::string& key, float dragSpeed = 1.0f,
        float labelOffset = 20.0f);

    // Render a text input property (Name, Tag, TexturePath, etc.)
    void _renderTextProperty(const std::string& label, nlohmann::json& data,
        const std::string& key, float labelOffset = 20.0f);

    // Render an integer drag property (SortingOrder, MaxParticles, FontSize, etc.)
    void _renderIntProperty(const std::string& label, nlohmann::json& data,
        const std::string& key, float labelOffset = 20.0f);

    // Render a color picker property (works for any RGBA color in JSON)
    void _renderColorProperty(const std::string& label, nlohmann::json& colorData,
        float labelOffset = 20.0f);

    // Render read-only text with label (for displaying non-editable info)
    void _renderReadOnlyText(const std::string& label, const std::string& value,
        float labelOffset = 10.0f);

    // Render two checkboxes on same row (FlipX/FlipY, Loop/PlayOnAwake, etc.)
    void _renderCheckboxRow(const std::string& label, nlohmann::json& data,
        const std::string& key1, const std::string& label1, const std::string& key2,
        const std::string& label2, float labelOffset = 30.0f);

    // Generic component section renderer: wraps content in collapsing header
    // Uses lambda function for flexible component-specific rendering
    template <typename T>
    void _renderComponentSection(const std::string& headerName, nlohmann::json& data,
        T renderContent);

    // Helper to check if prefab already has a component type
    bool _prefabHasComponent(const std::string& componentType);

    // Display prefab editor window with property editing
    // (Will eventually move to Inspector when implemented)
    void _showPrefabEditor();

    // References to external systems
    float m_fontScale = 1.0f;
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;   // Material Symbols font for icons
    World* m_world = nullptr;          // Game world reference for entity management

    // Navigation state
    std::string m_assetsRootPath = "assets\\";  // Root assets folder
    std::string m_currentPath = "assets\\";     // Current browsing path
    std::string m_selectedAsset;                // Currently selected file path

    // Prefab editing state
    bool m_editingPrefab = false;               // Flag to show/hide prefab editor
    nlohmann::json m_prefabData;                // Loaded prefab JSON data
    std::string m_editingPrefabPath;            // Path to prefab being edited

    // Status notification
    std::string m_statusMessage = "";           // Success/error message text
    float m_statusTimer = 0.0f;                 // Countdown timer for message display
};

#endif