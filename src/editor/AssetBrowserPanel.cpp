/* Start Header *****************************************************************/
/*!
\file   AssetBrowserPanel.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025

\brief
Implements the AssetBrowserPanel which renders the asset browser UI.

Provides:
- Breadcrumb navigation and folder traversal
- File listing with selection and info display
- Import replace and delete actions
- Prefab load and edit workflow
- Status bar updates and file-drop handling
- Integration with AssetLibrary and InspectorPanel
*/
/* End Header *******************************************************************/

#include "../editor/AssetBrowserPanel.h"
#include "core/Logger.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "serialization/EntitySerializer.h"
#include "ecs/Entity.h"
#include "../editor/InspectorPanel.h"
#include "services/Input.h"
#include <fstream>

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

// Initialize the Asset Browser with fonts and a world reference
// Also hook file-drop messages so imports work from the OS
void AssetBrowserPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;

    // Initialize helper modules
    m_assetLibrary.Initialize(mainFont, boldFont, symbolsFont);

    // Subscribe to file drop events
    Messaging::MessageSystem::Subscribe<Messaging::FileDropped>(
        [this](const Messaging::FileDropped& msg) {
            m_assetLibrary._handleFileDrop(msg.filePath, m_currentPath, m_selectedAsset, m_statusMessage, m_statusTimer);
        }
    );
}

// Update the world reference when scene changes
void AssetBrowserPanel::SetWorld(ECS::World* world) {
    m_world = world;
    m_assetLibrary.SetWorld(world);
}

// Connect the InspectorPanel so prefab actions in Asset Browser
// are inspected in the unified prefab editor UI
void AssetBrowserPanel::SetInspector(InspectorPanel* inspector) {
    m_inspector = inspector;
    // Propagate to AssetLibrary for double-click open behavior
    m_assetLibrary.SetInspector(inspector);
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------

// Render the Asset Browser window with breadcrumbs, actions and panels
// Uses child regions to split file list and file info side-by-side
void AssetBrowserPanel::Render() {
    ImGui::PushFont(m_mainFont);
    // Window flags: NoScrollbar removes the vertical scrollbar; child regions handle scrolling
    ImGui::Begin("Asset Browser", nullptr, ImGuiWindowFlags_NoScrollbar);

    // Render all the UI stuff
    _renderNavigationBar();
    _renderActionButtons();
    _renderContentArea();

    // Handle DELETE key press to delete selected asset
    if (!m_selectedAsset.empty() && Input::IsKeyDown(KEY_DELETE)) {
        m_assetLibrary._deleteSelectedAsset(m_selectedAsset, m_statusMessage, m_statusTimer);
    }

    // Click on empty space in parent Asset Browser window to clear everything
    AssetBrowserPanel::_selectEmptySpace();

    // Only render status bar if we have an active message
    if (m_statusTimer > 0.0f) {
        _renderStatusBar();
    }

    ImGui::End();
    ImGui::PopFont();

    // Prefab editing is absorbed by InspectorPanel; no separate window to render
}

// -------------------------------------------------------------------------
// UI Sections
// -------------------------------------------------------------------------

// Render the breadcrumb navigation bar at the top
void AssetBrowserPanel::_renderNavigationBar() {
    // Display clickable breadcrumb navigation
    std::string newPath;
    m_assetLibrary._displayBreadcrumbs(m_currentPath, m_selectedAsset, newPath);
    if (!newPath.empty() && newPath != m_currentPath) {
        m_currentPath = newPath;
        // Navigating to a different folder clears file selection and inspector
        if (!m_selectedAsset.empty()) {
            m_selectedAsset.clear();
        }
    }
}

// Render the action buttons (import, replace, prefab management)
void AssetBrowserPanel::_renderActionButtons() {
    // Import button (upload icon)
    // Push symbols font so the button renders an icon glyph
    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button("\xEF\x82\x9B")) {
        m_assetLibrary._importAsset(m_currentPath, m_selectedAsset, m_statusMessage, m_statusTimer);
    }
    ImGui::PopFont();

    // Tooltip
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Import new assets into current folder");
    }

    ImGui::SameLine();

    // Replace button (enabled only if a file is selected, not a folder)
    // True if something is selected at all
    bool hasSelection = !m_selectedAsset.empty();
    bool isFolderSelection = false;

    // If something is selected, check if it's a directory
    // Use error_code so is_directory doesn't throw on invalid paths
    if (hasSelection) {
        std::error_code ec;
        isFolderSelection = std::filesystem::is_directory(m_selectedAsset, ec) && !ec;
    }

    // Replace is allowed only when a file is selected (not a folder)
    bool enableReplace = hasSelection && !isFolderSelection;

    // Wrap controls in BeginDisabled/EndDisabled to gray-out and block interaction when not applicable
    if (!enableReplace) ImGui::BeginDisabled();

    // Use symbols font for the replace icon button
    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button("\xEE\xA3\x94")) {
        m_assetLibrary._replaceTexture(m_selectedAsset, m_statusMessage, m_statusTimer);
    }

    ImGui::PopFont();
    if (!enableReplace) ImGui::EndDisabled();

    // Show tooltip even when disabled
    // Allow tooltips even when the button is disabled to explain why
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (enableReplace) {
            ImGui::SetTooltip("Replace the selected file with another of the same extension");
        }
        else if (hasSelection && isFolderSelection) {
            ImGui::SetTooltip("Replace is disabled for folders");
        }
        else {
            ImGui::SetTooltip("Replace (disabled)");
        }
    }

    ImGui::SameLine();
    // UI things
    _renderPrefabButton();
}

// Render the prefab management button and popup
void AssetBrowserPanel::_renderPrefabButton() {
    // Prefab button (only enabled if a prefab is selected)
    // Only enable prefab popup when a .prefab file is selected
    bool isPrefab = !m_selectedAsset.empty() && std::filesystem::path(m_selectedAsset).extension() == ".prefab";
    if (!isPrefab) ImGui::BeginDisabled();

    // Contains load and edit prefab buttons
    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button("\xEE\x85\x85\xEE\x8C\x93")) {
        ImGui::OpenPopup("Prefabs");
    }

    ImGui::PopFont();
    if (!isPrefab) ImGui::EndDisabled();

    // Tooltip for prefab button
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (isPrefab) {
            ImGui::SetTooltip("Prefab management (load/edit)");
        }
        else {
            ImGui::SetTooltip("Prefab management (disabled)");
        }
    }

    // UI things
    _renderPrefabPopup();
}

// Render the prefab popup menu with load/edit options
void AssetBrowserPanel::_renderPrefabPopup() {
    // Open ImGui popup named "Prefabs"
    // Begin a modal-style popup to choose prefab actions
    if (ImGui::BeginPopup("Prefabs")) {
        // Display selectable option "Load Prefab" in the popup
        if (ImGui::Selectable("Load Prefab")) {
            _loadPrefab();
        }

        // Edit prefab option: open in unified Inspector
        if (ImGui::Selectable("Edit Prefab")) {
            _editPrefab();
        }

        ImGui::EndPopup();
    }
}

// Render the main content area (file list and file info panels)
void AssetBrowserPanel::_renderContentArea() {
    // Reserve space for status bar at bottom
    float windowWidth = ImGui::GetContentRegionAvail().x;
    // Slightly shorten the status bar to give the content area a bit more height
    float statusBarHeight = 24.0f;

    // Content region above status bar (fills remaining height; docking preserves split ratios)
    // Child fills remaining height; negative height reserves space for the fixed status bar
    ImGui::BeginChild("ContentRegion", ImVec2(0, -statusBarHeight), false);

    _renderFileListPanel(windowWidth);
    ImGui::SameLine();
    _renderFileInfoPanel();

    // End content region above status bar
    ImGui::EndChild();
}

// Render the left file/folder list panel
void AssetBrowserPanel::_renderFileListPanel(float windowWidth) {
    // Left side: File/folder list (65% width)
    // Left list child; third arg 'true' draws a frame (border) around the child
    ImGui::BeginChild("FileList", ImVec2(windowWidth * 0.65f, 0), true);
    m_assetLibrary._displayFolder(m_currentPath, m_selectedAsset, m_currentPath);

    // Clicking empty space in file list clears EVERYTHING (File/folder list)
    AssetBrowserPanel::_selectEmptySpace();
    ImGui::EndChild();
}

// Render the right file info panel with delete button
void AssetBrowserPanel::_renderFileInfoPanel() {
    // Right side: File info panel (35% width)
    ImGui::BeginChild("FileInfo", ImVec2(0, 0), true);
    m_assetLibrary._displaySelectedFileInfo(m_selectedAsset);

    // Clicking empty space in file info clears EVERYTHING (File info)
    AssetBrowserPanel::_selectEmptySpace();

    _renderDeleteButton();
    ImGui::EndChild();
}

// Render the delete button for selected assets
void AssetBrowserPanel::_renderDeleteButton() {
    // Only show delete button if something is selected
    if (!m_selectedAsset.empty()) {
        std::filesystem::path selectedPath(m_selectedAsset);

        // Check if file/folder still exists
        if (std::filesystem::exists(selectedPath)) {
            bool isFolder = std::filesystem::is_directory(selectedPath);

            ImGui::Dummy(ImVec2(0, 5));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 5));

            // Style the delete button: icon font + transparent background + red text
            ImGui::PushFont(m_symbolsFont);
            // Button: fully transparent; Hover/Active: subtle grays; Text: red to indicate destructive action
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));

            // Render delete icon button
            if (ImGui::SmallButton("\xEE\xA1\xB2\##Delete2")) {
                m_assetLibrary._deleteSelectedAsset(m_selectedAsset, m_statusMessage, m_statusTimer);
            }

            // Restore style overrides and font state
            ImGui::PopStyleColor(4);
            ImGui::PopFont();

            // Show tooltip on hover
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip(isFolder ? "Delete selected folder and all contents" : "Delete selected file");
            }
        }
    }
}

// Render the status bar and update its timer
void AssetBrowserPanel::_renderStatusBar() {
    float statusBarHeight = 24.0f;
    ImGui::BeginChild("StatusBar", ImVec2(0, statusBarHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Pick text color based on whether the message indicates failure
    ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
        ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
        : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
    ImGui::SetCursorPosX(3);
    ImGui::TextColored(color, "%s", m_statusMessage.c_str());

    // Read DeltaTime for timer countdown
    float dt = ImGui::GetIO().DeltaTime;

    // If dt is huge, it means the frame was frozen by:
    // - File dialog
    // - Breakpoint
    // - Window move
    // - Alt-tab
    // - System stall
    if (dt > 0.2f) {
        dt = 0.0f;  // Don't let a jump instantly wipe the timer
    }

    // Decrement the status timer safely
    m_statusTimer -= dt;
    ImGui::EndChild();
}

// -------------------------------------------------------------------------
// Prefab Operations and Selection
// -------------------------------------------------------------------------

// Load the selected prefab into the scene
void AssetBrowserPanel::_loadPrefab() {
    // Only proceed if an asset is selected and world pointer is valid
    if (!m_selectedAsset.empty() && m_world) {
        try {
            // Open the selected prefab file
            std::ifstream file(m_selectedAsset);
            // Failure, log it
            if (!file.is_open()) {
                LOG_ERROR("Cannot open file: " << m_selectedAsset);
                m_statusMessage = "Failed to open prefab";
                m_statusTimer = 3.0f;
            }
            else {
                nlohmann::json entityJson;
                file >> entityJson; // Read JSON content
                file.close();       // Close file after reading

                // Deserialize JSON into an entity in the current world
                auto entity = Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);

                // Add PrefabLink component to store normalized path of prefab
                std::filesystem::path p(m_selectedAsset);
                std::string linkPath = p.lexically_normal().string();
                m_world->Set<ECS::Components::PrefabLink>(entity, ECS::Components::PrefabLink(linkPath));

                // Log info and update status message on successful load
                LOG_INFO("Loaded prefab: " << std::filesystem::path(m_selectedAsset).filename().string());
                m_statusMessage = "Prefab loaded successfully";
                m_statusTimer = 3.0f;
            }
        }
        catch (const std::exception& e) {
            // Catch JSON parse or deserialization errors
            LOG_ERROR("Failed to parse prefab file: " << e.what());
            m_statusMessage = "Failed to load prefab";
            m_statusTimer = 3.0f;
        }
    }
}

// Open the selected prefab in the prefab editor for editing
void AssetBrowserPanel::_editPrefab() {
    // Check if prefab editor instance is available
    if (!m_inspector) {
        LOG_WARNING("Prefab editor not available for prefab editing");
        m_statusMessage = "Prefab editor not available";
        m_statusTimer = 3.0f;
    }
    // Check if prefab is selected
    else if (m_selectedAsset.empty()) {
        m_statusMessage = "Failed to open prefab: none selected";
        m_statusTimer = 3.0f;
    }
    else {
        try {
            // Validate the prefab can be opened and parsed
            std::ifstream file(m_selectedAsset);
            if (!file.is_open()) {
                LOG_ERROR("Cannot open prefab file: " << m_selectedAsset);
                m_statusMessage = "Failed to open prefab";
                m_statusTimer = 3.0f;
            }
            else {
                nlohmann::json prefabJson;
                file >> prefabJson; // Parse JSON content
                file.close();       // Close file after reading

                // If parsing succeeded, open in prefab editor and report success
                m_inspector->InspectPrefab(m_selectedAsset);
                m_statusMessage = "Prefab opened";
                m_statusTimer = 3.0f;
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("Failed to parse prefab file: " << e.what());
            m_statusMessage = "Failed to open prefab";
            m_statusTimer = 3.0f;
        }
    }
}

void AssetBrowserPanel::_selectEmptySpace() {
    // Click on empty space in parent Asset Browser window to clear everything
    // !IsAnyItemHovered() prevents clearing when clicking breadcrumbs, buttons, sliders, etc.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered()) {

        // Clear currently selected asset
        m_selectedAsset.clear();

        // Clear prefab editor if it exists (no need to clear property inspector here)
        if (m_inspector) {
            m_inspector->ClearSelection();
        }
    }
}
