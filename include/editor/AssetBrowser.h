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
#include <imgui.h>
#include "../editor/AssetLibrary.h"
#include "../editor/PrefabEditor.h"
class InspectorWindow; // forward

// Forward declarations
struct ImFont;
class World;

class AssetBrowser {
public:
    // Initialize with symbols font for icons and world reference
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world);

    // Render the asset browser UI
    void Render();

    // Wire unified Inspector for prefab editing
    void SetInspector(InspectorWindow* inspector);

private:
    // References to external systems
    float m_fontScale = 1.0f;
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    World* m_world = nullptr;

    // Navigation state
    std::string m_assetsRootPath = "assets\\";  // Root assets folder
    std::string m_currentPath = "assets\\";     // Current browsing path
    std::string m_selectedAsset;                // Currently selected file path

    // Helper modules
    AssetLibrary m_assetLibrary;                // File operations helper
    PrefabEditor m_prefabEditor;                // Prefab editing helper

    // Unified inspector wiring
    InspectorWindow* m_inspector = nullptr;

    // Status notification
    std::string m_statusMessage = "";           // Success/error message text
    float m_statusTimer = 0.0f;                 // Countdown timer for message display
};

#endif