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

// Forward declarations
struct ImFont;
namespace ECS { class World; }  // Add namespace
class InspectorWindow;

class AssetBrowser {
public:
    // This sets up the fonts and world reference
    // It gets the asset browser ready to render and interact
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);

    // This draws the asset browser panel
    // It shows folders files and basic actions
    void Render();

    // This connects the inspector window
    // It lets double clicks open and edit prefabs
    void SetInspector(InspectorWindow* inspector);

    // This updates which world the browser uses
    // It keeps things in sync when scenes change
    void SetWorld(ECS::World* world) { m_world = world; }

private:
    // References to external systems
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    ECS::World* m_world = nullptr;  // Add namespace

    // Navigation state
    std::string m_assetsRootPath = "assets\\";  // Root assets folder
    std::string m_currentPath = "assets\\";     // Current browsing path
    std::string m_selectedAsset;                // Currently selected file path

    // Helper modules
    AssetLibrary m_assetLibrary;                // File operations helper
    InspectorWindow* m_inspector = nullptr;     // Unified inspector wiring

    // Status notification
    std::string m_statusMessage = "";           // Success/error message text
    float m_statusTimer = 0.0f;                 // Countdown timer for message display
};

#endif