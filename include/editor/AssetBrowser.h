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
#include "../editor/AssetLibrary.h"

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
    // Load prefab file from disk and instantiate it into the world
    void _loadPrefab();

    // Open prefab editor (loads JSON and sets m_editingPrefab flag)
    void _editPrefab();

    // Update all entities instantiated from this prefab to match latest definition
    void _updatePrefabInstances();

    // Helper for updating a single entity from prefab data
    bool _updateEntityFromPrefab(Entity& entity);

    // Generic component section renderer: wraps content in collapsing header
    // Uses lambda function for flexible component-specific rendering
    template <typename T>
    void _renderComponentSection(const std::string& headerName, const std::string& componentType, 
        nlohmann::json& data, T renderContent, bool canDelete = true);

    // Helper to check if prefab already has a component type
    bool _prefabHasComponent(const std::string& componentType);

    // Add a component to the currently editing prefab
    void _addComponentToPrefab(const std::string& componentType);

    // Remove a component from the currently editing prefab
    void _removeComponentFromPrefab(const std::string& componentType);

    // Display prefab editor window with property editing
    // (Will eventually move to Inspector when implemented)
    void _showPrefabEditor();

    // References to external systems
    float m_fontScale = 1.0f;
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;                // Material Symbols font for icons
    World* m_world = nullptr;                       // Game world reference for entity management
    std::vector<std::string> m_componentsToDelete;  // Track components marked for deletion

    // Navigation state
    std::string m_assetsRootPath = "assets\\";      // Root assets folder
    std::string m_currentPath = "assets\\";         // Current browsing path
    std::string m_selectedAsset;                    // Currently selected file path
    AssetLibrary m_assetLibrary;                    // File operations helper

    // Prefab editing state
    bool m_editingPrefab = false;                   // Flag to show/hide prefab editor
    nlohmann::json m_prefabData;                    // Loaded prefab JSON data
    std::string m_editingPrefabPath;                // Path to prefab being edited

    // Status notification
    std::string m_statusMessage = "";               // Success/error message text
    float m_statusTimer = 0.0f;                     // Countdown timer for message display
};

#endif