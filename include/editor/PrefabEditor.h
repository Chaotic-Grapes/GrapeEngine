/* Start Header *****************************************************************/
/*!
\file   PrefabEditor.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Handles prefab loading, editing, and instance synchronization for the asset browser.

Features:
- Prefab loading and instantiation
- JSON-based prefab editing with component management
- Prefab instance synchronization across all entities
- Component add/remove with default values
- Generic component section rendering with lambdas

References:
- ImGui documentation for UI widgets and styling
- nlohmann/json for prefab data manipulation
- Lambda functions for flexible component rendering
*/
/* End Header *******************************************************************/

#ifndef PREFABEDITOR_H
#define PREFABEDITOR_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <imgui.h>

// Forward declarations
struct ImFont;
class World;
class Entity;

class PrefabEditor {
    friend class AssetBrowser;  // Only AssetBrowser can access private members

public:
    // Initialize with fonts and world reference
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world);

    // Render the prefab editor window if active
    void RenderEditor(float fontScale, std::string& statusMessage, float& statusTimer);

    // Check if currently editing a prefab
    bool IsEditing() const { return m_editingPrefab; }

private:
    // Load prefab file from disk and instantiate it into the world
    void _loadPrefab(const std::string& prefabPath, std::string& statusMessage, float& statusTimer);

    // Open prefab editor (loads JSON and sets m_editingPrefab flag)
    void _editPrefab(const std::string& prefabPath, std::string& statusMessage, float& statusTimer);

    // Update all entities instantiated from this prefab to match latest definition
    void _updatePrefabInstances();

    // Helper for updating a single entity from prefab data
    bool _updateEntityFromPrefab(Entity& entity);

    // Helper to check if prefab already has a component type
    bool _prefabHasComponent(const std::string& componentType);

    // Add a component to the currently editing prefab
    void _addComponentToPrefab(const std::string& componentType);

    // Remove a component from the currently editing prefab
    void _removeComponentFromPrefab(const std::string& componentType, std::string& statusMessage, float& statusTimer);

    // Get default JSON data for a newly added component
    nlohmann::json _getDefaultComponentData(const std::string& componentType);

    // Display prefab editor window with property editing
    void _showPrefabEditor(float fontScale, std::string& statusMessage, float& statusTimer);

    // Render an item in the “Add Component” menu
    void _renderComponentMenuItem(const char* displayName, const char* componentType);

    // Component-specific UI renderers (called per component type)
    void _renderTransformUI(nlohmann::json& data);
    void _renderSpriteRendererUI(nlohmann::json& data);
    void _renderRigidbody2DUI(nlohmann::json& data);
    void _renderCircleCollider2DUI(nlohmann::json& data);
    void _renderBoxCollider2DUI(nlohmann::json& data);
    void _renderLineRendererUI(nlohmann::json& data);
    void _renderShapeRenderer2DUI(nlohmann::json& data);

    // Generic component section renderer: wraps content in a collapsing header
    // Uses lambda function for flexible component-specific rendering
    template <typename T>
    void _renderComponentSection(const std::string& headerName, const std::string& componentType,
        nlohmann::json& data, T renderContent, bool canDelete = true);

    // References to external systems
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    World* m_world = nullptr;

    // Prefab editing state
    bool m_editingPrefab = false;
    nlohmann::json m_prefabData;
    std::string m_editingPrefabPath;
    std::vector<std::string> m_componentsToDelete;
};

#endif
