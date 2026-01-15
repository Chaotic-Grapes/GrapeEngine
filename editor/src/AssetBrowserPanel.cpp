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

#include "AssetBrowserPanel.h"
#include "core/Logger.h"
#include "core/ProjectPaths.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "serialization/EntitySerializer.h"
#include "ecs/Entity.h"
#include "ecs/PrefabManager.h"
#include "InspectorPanel.h"
#include "services/Input.h"
#include "ScriptTemplates.h"
#include <fstream>
#include <cstring>
#include "EditorStyle.h"

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

    // TODO: Remove when editor is separated - use project-relative paths
    // Set initial path to game project root folder
    m_currentPath = Engine::ProjectPaths::GetProjectRoot();

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

    // Handle keyboard shortcuts - ONLY when Asset Browser window is focused
    // This prevents conflicts with other panels (e.g., Hierarchy) that also use DELETE key
    if (!m_selectedAssets.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        // DELETE key: delete selected assets
        if (Input::IsKeyDown(KEY_DELETE)) {
            _deleteSelectedAssets();
        }

        // Ctrl+C: copy
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
            _copySelectedAssets();
        }

        // Ctrl+X: cut (copy + mark for deletion after paste)
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
            _copySelectedAssets();
            m_clipboardIsCut = true;
        }
    }

    // Ctrl+V: paste (works even with no selection)
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        _pasteAssets();
    }

    // F2: rename (single selection only)
    if (m_selectedAssets.size() == 1 && ImGui::IsKeyPressed(ImGuiKey_F2)) {
        _startRename();
    }

    // Click on empty space in parent Asset Browser window to clear everything
    _selectEmptySpace();

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
    // Display clickable breadcrumb navigation with drop target support
    const std::filesystem::path pathObj(m_currentPath);
    std::vector<std::filesystem::path> pathParts;

    // Build path parts from root to current
    for (const auto& part : pathObj) {
        std::string partStr = part.string();
        if (!partStr.empty() && partStr != "." && partStr != ".." && partStr != "/" && partStr != "\\") {
            pathParts.emplace_back(partStr);
        }
    }

    // Display each part as clickable button with separators and drop targets
    std::string accumulatedPath;
    for (size_t i = 0; i < pathParts.size(); i++) {
        // Build accumulated path up to this part
        if (i > 0) accumulatedPath += "\\";
        accumulatedPath += pathParts[i].string();

        // Add ">" separator between breadcrumb parts
        if (i > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
        }

        // Display breadcrumb part
        if (i == pathParts.size() - 1) {
            // Last part (current directory): display as plain text
            ImGui::Text("%s", pathParts[i].string().c_str());

            // Make current directory text a drop target
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                _handleFolderDropTarget(m_currentPath);
            }
        }
        else {
            // Previous parts: display as clickable blue buttons
            ImGui::PushID(static_cast<int>(i));

            ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Accent);
            ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::Transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::Scale(EditorStyle::AccentHover, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::Scale(EditorStyle::AccentActive, 0.5f));

            // When clicked, navigate to that folder level
            if (ImGui::SmallButton(pathParts[i].string().c_str())) {
                m_currentPath = accumulatedPath;
                m_selectedAssets.clear();
                m_selectedAsset.clear();
            }

            ImGui::PopStyleColor(4);

            // Make breadcrumb button a drop target
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                _handleFolderDropTarget(accumulatedPath);
            }

            ImGui::PopID();
        }
    }

    // Add spacing after breadcrumbs
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();
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
    const bool hasSelection = !m_selectedAsset.empty();
    bool isFolderSelection = false;

    // If something is selected, check if it's a directory
    // Use error_code so is_directory doesn't throw on invalid paths
    if (hasSelection) {
        std::error_code ec;
        isFolderSelection = std::filesystem::is_directory(m_selectedAsset, ec) && !ec;
    }

    // Replace is allowed only when a file is selected (not a folder)
    const bool enableReplace = hasSelection && !isFolderSelection;

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
    // Plus button - always enabled now (for Create + Prefab management)
    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button("\xEE\x85\x85\xEE\x8C\x93")) {
        ImGui::OpenPopup("CreateAndPrefabs");
    }

    ImGui::PopFont();

    // Tooltip for button
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Create new assets and manage prefabs");
    }

    // UI things
    _renderPrefabPopup();
}

// Render the combined create/prefab popup menu
void AssetBrowserPanel::_renderPrefabPopup() {
    // Check if a prefab is selected for conditional enabling
    const bool isPrefab = !m_selectedAsset.empty() && std::filesystem::path(m_selectedAsset).extension() == ".prefab";

    if (ImGui::BeginPopup("CreateAndPrefabs")) {
        ImGui::PushFont(m_mainFont);

        // Load Prefab - only enabled when prefab is selected
        if (!isPrefab) ImGui::BeginDisabled();
        if (ImGui::MenuItem("Load Prefab")) {
            _loadPrefab();
        }
        if (!isPrefab) ImGui::EndDisabled();

        // Edit Prefab - only enabled when prefab is selected
        if (!isPrefab) ImGui::BeginDisabled();
        if (ImGui::MenuItem("Edit Prefab")) {
            _editPrefab();
        }
        if (!isPrefab) ImGui::EndDisabled();

        ImGui::Separator();

        // Create submenu - uses the same menu items as right-click context menu
        if (ImGui::BeginMenu("Create")) {
            // Use shared helper function
            if (_renderCreateMenuItems()) {
                m_openCreateDialog = true;
            }
            ImGui::EndMenu();
        }

        ImGui::PopFont();
        ImGui::EndPopup();
    }

    // Open create dialog if flagged
    if (m_openCreateDialog) {
        ImGui::OpenPopup("CreateAssetDialog");
        m_openCreateDialog = false;
    }

    // Asset creation dialog (modal) - shared with context menu
    if (ImGui::BeginPopupModal("CreateAssetDialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(m_mainFont);

        const char* dialogTitle = "Create Asset";
        if (m_creationType == AssetCreationType::SCRIPT) dialogTitle = "Create Script";
        else if (m_creationType == AssetCreationType::SCENE) dialogTitle = "Create Scene";
        else if (m_creationType == AssetCreationType::FOLDER) dialogTitle = "Create Folder";
        ImGui::Text("%s", dialogTitle);
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5));

        // Show script template selector when creating a script
        if (m_creationType == AssetCreationType::SCRIPT) {
            ImGui::Text("Template:");
            ImGui::SameLine();

            // Get template names
            int templateCount = 0;
            const char* const* templateNames = Editor::Templates::ScriptTemplates::GetTemplateNames(templateCount);

            // Show combo for template selection
            const char* currentTemplateName = nullptr;
            switch (m_selectedScriptTemplate) {
                case Editor::Templates::ScriptTemplateType::BasicSystem:
                    currentTemplateName = "BasicSystem";
                    break;
                case Editor::Templates::ScriptTemplateType::EditModeSystem:
                    currentTemplateName = "EditModeSystem";
                    break;
                case Editor::Templates::ScriptTemplateType::HotReloadSystem:
                    currentTemplateName = "HotReloadSystem";
                    break;
                case Editor::Templates::ScriptTemplateType::MetadataSystem:
                    currentTemplateName = "MetadataSystem";
                    break;
            }

            if (ImGui::BeginCombo("##ScriptTemplate", currentTemplateName)) {
                for (int i = 0; i < templateCount; i++) {
                    bool isSelected = (currentTemplateName && strcmp(templateNames[i], currentTemplateName) == 0);
                    if (ImGui::Selectable(templateNames[i], isSelected)) {
                        m_selectedScriptTemplate = Editor::Templates::ScriptTemplates::GetTemplateTypeFromName(templateNames[i]);
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            // Show description of selected template
            std::string description = Editor::Templates::ScriptTemplates::GetTemplateDescription(m_selectedScriptTemplate);
            ImGui::TextDisabled("%s", description.c_str());
            ImGui::Dummy(ImVec2(0, 5));
        }

        // Name input
        ImGui::Text("Name:");
        ImGui::SameLine();
        if (m_focusNameInput) {
            ImGui::SetKeyboardFocusHere();
            m_focusNameInput = false;
        }

        const bool enterPressed = ImGui::InputText("##AssetName", m_newAssetNameBuffer, sizeof(m_newAssetNameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Dummy(ImVec2(0, 5));

        // Buttons
        const bool createClicked = ImGui::Button("Create") || enterPressed;
        ImGui::SameLine();
        const bool cancelClicked = ImGui::Button("Cancel");

        if (createClicked && strlen(m_newAssetNameBuffer) > 0) {
            if (m_creationType == AssetCreationType::SCRIPT) {
                _createScript();
            }
            else if (m_creationType == AssetCreationType::SCENE) {
                _createScene();
            }
            else if (m_creationType == AssetCreationType::FOLDER) {
                _createFolder();
            }
            ImGui::CloseCurrentPopup();
        }

        if (cancelClicked) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopFont();
        ImGui::EndPopup();
    }
}

// Render the main content area (file list and file info panels)
void AssetBrowserPanel::_renderContentArea() {
    // Reserve space for status bar at bottom
    const float windowWidth = ImGui::GetContentRegionAvail().x;
    // Slightly shorten the status bar to give the content area a bit more height
    const float statusBarHeight = 24.0f;

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
void AssetBrowserPanel::_renderFileListPanel(const float windowWidth) {
    // Left side: File/folder list (65% width)
    // Left list child; third arg 'true' draws a frame (border) around the child
    ImGui::BeginChild("FileList", ImVec2(windowWidth * 0.65f, 0), true);

    // Custom folder display with multi-selection support
    ImGui::PushFont(m_mainFont);

    if (!std::filesystem::exists(m_currentPath) || !std::filesystem::is_directory(m_currentPath)) {
        ImGui::TextColored(EditorStyle::DangerText, "Folder not found");
    }
    else {
        // Iterate through directory entries
        for (const auto& entry : std::filesystem::directory_iterator(m_currentPath)) {
            std::string entryPath = entry.path().string();
            std::string entryName = entry.path().filename().string();
            const bool isSelected = m_selectedAssets.contains(entryPath);

            // Render icon
            ImGui::PushFont(m_symbolsFont);
            if (entry.is_directory()) {
                ImGui::Text("\xEE\x8B\x87"); // Folder icon
            }
            else {
                ImGui::Text("\xEE\xA1\xB3"); // File icon
            }
            ImGui::PopFont();

            ImGui::SameLine();

            // Handle rename mode
            if (m_renamingAsset == entryPath) {
                ImGui::PushItemWidth(-1);
                if (m_focusRenameInput) {
                    ImGui::SetKeyboardFocusHere();
                    m_focusRenameInput = false;
                }

                if (ImGui::InputText("##Rename", m_renameBuffer, sizeof(m_renameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                    // Apply rename
                    try {
                        std::filesystem::path newPath = entry.path().parent_path() / m_renameBuffer;
                        std::filesystem::rename(entry.path(), newPath);
                        m_selectedAssets.erase(entryPath);
                        m_selectedAssets.insert(newPath.string());
                        m_selectedAsset = newPath.string();
                        m_statusMessage = "Renamed to: " + std::string(m_renameBuffer);
                        m_statusTimer = 2.0f;
                    }
                    catch (const std::exception& e) {
                        m_statusMessage = "Rename failed: " + std::string(e.what());
                        m_statusTimer = 3.0f;
                    }
                    m_renamingAsset.clear();
                }

                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    m_renamingAsset.clear();
                }

                ImGui::PopItemWidth();
            }
            else {
                // Normal selectable
                if (ImGui::Selectable(entryName.c_str(), isSelected,
                    ImGuiSelectableFlags_AllowDoubleClick)) {

                    const bool ctrlPressed = ImGui::GetIO().KeyCtrl;
                    const bool shiftPressed = ImGui::GetIO().KeyShift;

                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && entry.is_directory()) {
                        // Double-click folder: navigate
                        m_currentPath = entryPath;
                        m_selectedAssets.clear();
                        m_selectedAsset.clear();
                    }
                    else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                        entry.path().extension() == ".prefab" && m_inspector) {
                        // Double-click prefab: open in inspector
                        m_inspector->InspectPrefab(entryPath);
                    }
                    else if (ctrlPressed) {
                        // Ctrl+click: toggle selection
                        if (isSelected) {
                            m_selectedAssets.erase(entryPath);
                        }
                        else {
                            m_selectedAssets.insert(entryPath);
                            m_anchorAsset = entryPath;
                        }
                        m_selectedAsset = m_selectedAssets.empty() ? "" : *m_selectedAssets.begin();
                    }
                    else if (shiftPressed && !m_anchorAsset.empty()) {
                        // Shift+click: range selection (simplified - select all between anchor and current)
                        m_selectedAssets.clear();
                        bool inRange = false;
                        for (const auto& e : std::filesystem::directory_iterator(m_currentPath)) {
                            std::string path = e.path().string();
                            if (path == m_anchorAsset || path == entryPath) {
                                m_selectedAssets.insert(path);
                                inRange = !inRange;
                                if (!inRange) break;
                            }
                            else if (inRange) {
                                m_selectedAssets.insert(path);
                            }
                        }
                        m_selectedAsset = entryPath;
                    }
                    else {
                        // Normal click: single selection
                        m_selectedAssets.clear();
                        m_selectedAssets.insert(entryPath);
                        m_selectedAsset = entryPath;
                        m_anchorAsset = entryPath;
                    }
                }

                // Handle right-click on item
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    // Select this item if not already selected
                    if (!m_selectedAssets.contains(entryPath)) {
                        m_selectedAssets.clear();
                        m_selectedAssets.insert(entryPath);
                        m_selectedAsset = entryPath;
                        m_anchorAsset = entryPath;
                    }
                    ImGui::OpenPopup("ItemContextMenu");
                }

                // Handle drag-drop
                _handleAssetDragDrop(entryPath);

                // Handle drop target for folders and auto-navigation
                if (entry.is_directory()) {
                    // Check if dragging over this folder for auto-navigation (only during drag)
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                        ImGui::GetDragDropPayload() != nullptr) {
                        static std::string s_hoveredFolder;
                        static float s_hoverStartTime = 0.0f;

                        if (s_hoveredFolder != entryPath) {
                            s_hoveredFolder = entryPath;
                            s_hoverStartTime = static_cast<float>(ImGui::GetTime());
                        }
                        else if (ImGui::GetTime() - s_hoverStartTime > 0.75f) {
                            // Auto-navigate after hovering for 0.75 seconds
                            m_currentPath = entryPath;
                            m_selectedAssets.clear();
                            m_selectedAsset.clear();
                            s_hoveredFolder.clear();
                        }
                    }
                    else {
                        // Not hovering or not dragging - reset state
                        static std::string s_hoveredFolder;
                        s_hoveredFolder.clear();
                    }

                    _handleFolderDropTarget(entryPath);
                }
            }
        }
    }

    ImGui::PopFont();

    // Create invisible button covering remaining empty space as drop target
    const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    if (contentAvail.y > 0) {
        ImGui::InvisibleButton("##EmptySpaceDropTarget", contentAvail);

        // Right-click on empty space to create new assets
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("AssetContextMenu");
        }

        _handleFolderDropTarget(m_currentPath);
    }

    // Render both context menus
    _renderContextMenu();
    _renderItemContextMenu();

    // Clicking empty space in file list clears EVERYTHING (File/folder list)
    AssetBrowserPanel::_selectEmptySpace();
    ImGui::EndChild();
}

// Render the right file info panel with delete button
void AssetBrowserPanel::_renderFileInfoPanel() {
    // Right side: File info panel (35% width)
    ImGui::BeginChild("FileInfo", ImVec2(0, 0), true);

    // Show multi-selection info or single file info
    if (m_selectedAssets.size() > 1) {
        ImGui::Text("Multiple Selection");
        ImGui::Separator();
        ImGui::Text("%zu items selected", m_selectedAssets.size());
    }
    else if (!m_selectedAsset.empty()) {
        m_assetLibrary._displaySelectedFileInfo(m_selectedAsset);
    }
    else {
        ImGui::TextDisabled("No file selected");
    }

    // Clicking empty space in file info clears EVERYTHING (File info)
    _selectEmptySpace();

    _renderDeleteButton();
    ImGui::EndChild();
}

// Render the delete button for selected assets
void AssetBrowserPanel::_renderDeleteButton() {
    // Only show delete button if something is selected
    if (!m_selectedAssets.empty()) {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5));

        // Style the delete button: icon font + transparent background + red text
        ImGui::PushFont(m_symbolsFont);
        // Button: fully transparent; Hover/Active: subtle grays; Text: red to indicate destructive action
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::Scale(EditorStyle::FrameBgHover, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::Scale(EditorStyle::FrameBgActive, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::DangerText);

        // Render delete icon button
        if (ImGui::SmallButton("\xEE\xA1\xB2\\##Delete2")) {
            _deleteSelectedAssets();
        }

        // Restore style overrides and font state
        ImGui::PopStyleColor(4);
        ImGui::PopFont();

        // Show tooltip on hover
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (m_selectedAssets.size() == 1) {
                const bool isFolder = std::filesystem::is_directory(*m_selectedAssets.begin());
                ImGui::SetTooltip(isFolder ? "Delete selected folder and all contents" : "Delete selected file");
            }
            else {
                ImGui::SetTooltip("Delete %zu selected items", m_selectedAssets.size());
            }
        }
    }
}

// Render the status bar and update its timer
void AssetBrowserPanel::_renderStatusBar() {
	constexpr float statusBarHeight = 24.0f;
    ImGui::BeginChild("StatusBar", ImVec2(0, statusBarHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Pick text color based on whether the message indicates failure
    const ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
        ? EditorStyle::DangerText
        : EditorStyle::SuccessText;
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
                ECS::Entity entity;
                if (entityJson.contains("Entity")) {
                    // If JSON has Entity key (there's hierarchy)
                    entity = Serialization::EntitySerializer::DeserializeEntityHierarchy(*m_world, entityJson["Entity"]);
                }
                // Load just 1 entity without children
                else if (entityJson.contains("Components")) {
                    entity = Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);
                }

                // Add PrefabInstanceMetadata component to store prefab hash
                std::filesystem::path p(m_selectedAsset);
                std::string normalizedPath = p.lexically_normal().string();

                uint32_t hash = ECS::PrefabManager::ComputeHash(
                    ECS::PrefabManager::NormalizePath(normalizedPath)
                );

                ECS::Components::PrefabInstanceMetadata meta;
                meta.PrefabHash = hash;
                meta.Flags = 0;
                m_world->Add<ECS::Components::PrefabInstanceMetadata>(entity, meta);

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

        // Clear currently selected assets
        m_selectedAsset.clear();
        m_selectedAssets.clear();
        m_anchorAsset.clear();

        // Clear prefab editor if it exists (no need to clear property inspector here)
        if (m_inspector) {
            m_inspector->ClearSelection();
        }
    }
}

// -------------------------------------------------------------------------
// Context Menu
// -------------------------------------------------------------------------

// Helper function to render create menu items (used by both context menu and + button submenu)
bool AssetBrowserPanel::_renderCreateMenuItems() {
    bool openDialog = false;

    // Create Script option
    if (ImGui::MenuItem("C# Script")) {
        m_creationType = AssetCreationType::SCRIPT;
        strcpy_s(m_newAssetNameBuffer, "NewScript");
        m_focusNameInput = true;
        openDialog = true;
    }

    // Create Scene option
    if (ImGui::MenuItem("Scene")) {
        m_creationType = AssetCreationType::SCENE;
        strcpy_s(m_newAssetNameBuffer, "NewScene");
        m_focusNameInput = true;
        openDialog = true;
    }

    // Create Folder option
    if (ImGui::MenuItem("Folder")) {
        m_creationType = AssetCreationType::FOLDER;
        strcpy_s(m_newAssetNameBuffer, "NewFolder");
        m_focusNameInput = true;
        openDialog = true;
    }

    return openDialog;
}

void AssetBrowserPanel::_renderContextMenu() {
    bool openCreateDialog = false;

    if (ImGui::BeginPopup("AssetContextMenu")) {
        ImGui::PushFont(m_mainFont);
        ImGui::Text("Create");
        ImGui::Separator();

        // Use shared helper function
        openCreateDialog = _renderCreateMenuItems();

        ImGui::PopFont();
        ImGui::EndPopup();
    }

    // Open the dialog outside of the popup to avoid nesting issues
    if (openCreateDialog) {
        ImGui::OpenPopup("CreateAssetDialog");
    }

    // Asset creation dialog (modal)
    if (ImGui::BeginPopupModal("CreateAssetDialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(m_mainFont);

        const char* dialogTitle = "Create Asset";
        if (m_creationType == AssetCreationType::SCRIPT) dialogTitle = "Create Script";
        else if (m_creationType == AssetCreationType::SCENE) dialogTitle = "Create Scene";
        else if (m_creationType == AssetCreationType::FOLDER) dialogTitle = "Create Folder";
        ImGui::Text("%s", dialogTitle);
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5));

        // Name input
        ImGui::Text("Name:");
        ImGui::SameLine();
        if (m_focusNameInput) {
            ImGui::SetKeyboardFocusHere();
            m_focusNameInput = false;
        }

        const bool enterPressed = ImGui::InputText("##AssetName", m_newAssetNameBuffer, sizeof(m_newAssetNameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Dummy(ImVec2(0, 5));

        // Buttons
        const bool createClicked = ImGui::Button("Create") || enterPressed;
        ImGui::SameLine();
        const bool cancelClicked = ImGui::Button("Cancel");

        if (createClicked && strlen(m_newAssetNameBuffer) > 0) {
            if (m_creationType == AssetCreationType::SCRIPT) {
                _createScript();
            }
            else if (m_creationType == AssetCreationType::SCENE) {
                _createScene();
            }
            else if (m_creationType == AssetCreationType::FOLDER) {
                _createFolder();
            }
            ImGui::CloseCurrentPopup();
        }

        if (cancelClicked) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopFont();
        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::_renderItemContextMenu() {
    if (ImGui::BeginPopup("ItemContextMenu")) {
        ImGui::PushFont(m_mainFont);

        // Show selected count
        if (m_selectedAssets.size() > 1) {
            ImGui::Text("%zu items selected", m_selectedAssets.size());
            ImGui::Separator();
        }

        // Rename (only for single selection)
        if (m_selectedAssets.size() == 1) {
            if (ImGui::MenuItem("Rename", "F2")) {
                _startRename();
            }
        }

        // Copy
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            _copySelectedAssets();
        }

        // Cut
        if (ImGui::MenuItem("Cut", "Ctrl+X")) {
            _copySelectedAssets();
            m_clipboardIsCut = true;
        }

        // Paste (if clipboard has items)
        if (!m_clipboardAssets.empty()) {
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {
                _pasteAssets();
            }
        }

        // Delete
        if (ImGui::MenuItem("Delete", "Del")) {
            _deleteSelectedAssets();
        }

        ImGui::PopFont();
        ImGui::EndPopup();
    }
}

void AssetBrowserPanel::_createScript() {
    // Create in current directory
    std::filesystem::path targetDir = m_currentPath;
    std::cout << "Creating script in directory: " << targetDir.string() << std::endl;

    // Ensure the directory exists
    if (!exists(targetDir)) {
        create_directories(targetDir);
    }

    // Create file path with .cs extension
    std::string fileName = m_newAssetNameBuffer;
    if (fileName.find(".cs") == std::string::npos) {
        fileName += ".cs";
    }
    std::filesystem::path filePath = targetDir / fileName;

    // Check if file already exists
    if (exists(filePath)) {
        m_statusMessage = "Script already exists: " + fileName;
        m_statusTimer = 3.0f;
        LOG_WARNING("Script file already exists: " << filePath.string());
        return;
    }

    // Generate script content using the selected template
    std::string className = m_newAssetNameBuffer;

    // Create namespace from the target directory by replacing
    // path separators with '.' and sanitizing invalid characters.
    std::string ns;
    try {
        // Try to make the path relative to the project root so namespaces
        // are nicer (e.g., "Assets.Scripts") when possible.
        std::filesystem::path projRoot = Engine::ProjectPaths::GetProjectRoot();
        std::filesystem::path rel = std::filesystem::relative(targetDir, projRoot);
        ns = rel.string();
    }
    catch (...) {
        ns = targetDir.string();
    }

    // Replace separators with dots and sanitize a few problematic chars
    for (char &c : ns) {
        if (c == '/' || c == '\\')
            c = '.';
        else if (c == ':' || c == ' ')
            c = '_';
    }

    // Trim leading/trailing dots if any
    while (!ns.empty() && ns.front() == '.')
        ns.erase(ns.begin());
    while (!ns.empty() && ns.back() == '.')
        ns.pop_back();

    // Fallback namespace if resulting string is empty
    if (ns.empty())
        ns = Engine::ProjectPaths::GetProjectRoot();

    // Generate script using the selected template
    std::string scriptContent = Editor::Templates::ScriptTemplates::GenerateScript(
        m_selectedScriptTemplate,
        className,
        ns
    );

    // Write file
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            m_statusMessage = "Failed to create script: " + fileName;
            m_statusTimer = 3.0f;
            LOG_ERROR("Failed to create script file: " << filePath.string());
            return;
        }

        file << scriptContent;
        file.close();

        m_statusMessage = "Created script: " + fileName;
        m_statusTimer = 3.0f;
        LOG_INFO("Created script: " << filePath.string());

        // Select the newly created file
        m_selectedAsset = filePath.string();

    }
    catch (const std::exception& e) {
        m_statusMessage = "Error creating script: " + std::string(e.what());
        m_statusTimer = 3.0f;
        LOG_ERROR("Exception creating script: " << e.what());
    }
}

void AssetBrowserPanel::_createScene() {
    // Create file path with .scn extension
    std::string fileName = m_newAssetNameBuffer;
    if (fileName.find(".scn") == std::string::npos) {
        fileName += ".scn";
    }
    std::filesystem::path filePath = std::filesystem::path(m_currentPath) / fileName;

    // Check if file already exists
    if (exists(filePath)) {
        m_statusMessage = "Scene already exists: " + fileName;
        m_statusTimer = 3.0f;
        LOG_WARNING("Scene file already exists: " << filePath.string());
        return;
    }

    // Create empty scene template
    std::string sceneContent =
        "{\n"
        "  \"Version\": \"1.0\",\n"
        "  \"Name\": \"" + std::string(m_newAssetNameBuffer) + "\",\n"
        "  \"Entities\": []\n"
        "}\n";

    // Write file
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            m_statusMessage = "Failed to create scene: " + fileName;
            m_statusTimer = 3.0f;
            LOG_ERROR("Failed to create scene file: " << filePath.string());
            return;
        }

        file << sceneContent;
        file.close();

        m_statusMessage = "Created scene: " + fileName;
        m_statusTimer = 3.0f;
        LOG_INFO("Created scene: " << filePath.string());

        // Select the newly created file
        m_selectedAsset = filePath.string();

    }
    catch (const std::exception& e) {
        m_statusMessage = "Error creating scene: " + std::string(e.what());
        m_statusTimer = 3.0f;
        LOG_ERROR("Exception creating scene: " << e.what());
    }
}

void AssetBrowserPanel::_createFolder() {
    // Create folder path
    std::string folderName = m_newAssetNameBuffer;
    std::filesystem::path folderPath = std::filesystem::path(m_currentPath) / folderName;

    // Check if folder already exists
    if (exists(folderPath)) {
        m_statusMessage = "Folder already exists: " + folderName;
        m_statusTimer = 3.0f;
        LOG_WARNING("Folder already exists: " << folderPath.string());
        return;
    }

    // Create the folder
    try {
        if (create_directory(folderPath)) {
            m_statusMessage = "Created folder: " + folderName;
            m_statusTimer = 3.0f;
            LOG_INFO("Created folder: " << folderPath.string());

            // Select the newly created folder
            m_selectedAsset = folderPath.string();
        }
        else {
            m_statusMessage = "Failed to create folder: " + folderName;
            m_statusTimer = 3.0f;
            LOG_ERROR("Failed to create folder: " << folderPath.string());
        }
    }
    catch (const std::exception& e) {
        m_statusMessage = "Error creating folder: " + std::string(e.what());
        m_statusTimer = 3.0f;
        LOG_ERROR("Exception creating folder: " << e.what());
    }
}

// -------------------------------------------------------------------------
// Copy/Paste Operations
// -------------------------------------------------------------------------

void AssetBrowserPanel::_copySelectedAssets() {
    m_clipboardAssets.clear();
    m_clipboardAssets.assign(m_selectedAssets.begin(), m_selectedAssets.end());
    m_clipboardIsCut = false;

    m_statusMessage = "Copied " + std::to_string(m_clipboardAssets.size()) + " item(s)";
    m_statusTimer = 2.0f;
}

void AssetBrowserPanel::_pasteAssets() {
    if (m_clipboardAssets.empty()) {
        m_statusMessage = "Clipboard is empty";
        m_statusTimer = 2.0f;
        return;
    }

    try {
        if (m_clipboardIsCut) {
            // Move operation
            _moveAssetsToDirectory(m_clipboardAssets, m_currentPath);
            m_clipboardAssets.clear();
            m_clipboardIsCut = false;
        }
        else {
            // Copy operation
            _copyAssetsToDirectory(m_clipboardAssets, m_currentPath);
        }
    }
    catch (const std::exception& e) {
        m_statusMessage = "Paste failed: " + std::string(e.what());
        m_statusTimer = 3.0f;
        LOG_ERROR("Paste operation failed: " << e.what());
    }
}

void AssetBrowserPanel::_deleteSelectedAssets() {
    if (m_selectedAssets.empty()) return;

    size_t deleteCount = 0;
    for (const auto& assetPath : m_selectedAssets) {
        try {
            std::filesystem::path path(assetPath);
            if (exists(path)) {
                if (is_directory(path)) {
                    remove_all(path);
                }
                else {
                    std::filesystem::remove(path);
                }
                deleteCount++;
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("Failed to delete " << assetPath << ": " << e.what());
        }
    }

    m_selectedAssets.clear();
    m_selectedAsset.clear();
    m_statusMessage = "Deleted " + std::to_string(deleteCount) + " item(s)";
    m_statusTimer = 2.0f;
}

void AssetBrowserPanel::_startRename() {
    if (m_selectedAssets.size() != 1) return;

    m_renamingAsset = *m_selectedAssets.begin();
    const std::filesystem::path path(m_renamingAsset);
    const std::string filename = path.filename().string();

    strncpy_s(m_renameBuffer, filename.c_str(), sizeof(m_renameBuffer) - 1);
    m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
    m_focusRenameInput = true;
}

// -------------------------------------------------------------------------
// Drag-Drop Operations
// -------------------------------------------------------------------------

void AssetBrowserPanel::_handleAssetDragDrop(const std::string& assetPath) {
    // Drag source: make selected assets draggable
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        // If this asset is part of selection, drag all selected assets
        std::vector<std::string> draggedAssets;
        if (m_selectedAssets.contains(assetPath)) {
            draggedAssets.assign(m_selectedAssets.begin(), m_selectedAssets.end());
        }
        else {
            draggedAssets.push_back(assetPath);
        }

        // Serialize paths as null-terminated strings concatenated together
        std::string serialized;
        for (const auto& path : draggedAssets) {
            serialized += path;
            serialized += '\0';
        }

        // Set payload with serialized string buffer
        ImGui::SetDragDropPayload("ASSET_PATHS", serialized.data(), serialized.size());

        // Drag preview
        ImGui::PushFont(m_symbolsFont);
        ImGui::Text("\xEF\x8E\xB2");
        ImGui::PopFont();
        ImGui::SameLine();
        if (draggedAssets.size() == 1) {
            const std::filesystem::path path(draggedAssets[0]);
            ImGui::Text("%s", path.filename().string().c_str());
        }
        else {
            ImGui::Text("%zu items", draggedAssets.size());
        }

        ImGui::EndDragDropSource();
    }
}

void AssetBrowserPanel::_handleFolderDropTarget(const std::string& folderPath) {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
            // Deserialize null-terminated strings from payload
            std::vector<std::string> draggedAssets;
            const char* data = static_cast<const char*>(payload->Data);
            const char* end = data + payload->DataSize;

            while (data < end) {
                std::string path(data);
                if (!path.empty()) {
                    draggedAssets.push_back(path);
                }
                data += path.size() + 1; // Move past null terminator
            }

            try {
                if (ImGui::GetIO().KeyCtrl) {
                    // Ctrl: copy
                    _copyAssetsToDirectory(draggedAssets, folderPath);
                }
                else {
                    // No modifier: move
                    _moveAssetsToDirectory(draggedAssets, folderPath);
                }
            }
            catch (const std::exception& e) {
                m_statusMessage = "Drop failed: " + std::string(e.what());
                m_statusTimer = 3.0f;
                LOG_ERROR("Drop operation failed: " << e.what());
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void AssetBrowserPanel::_moveAssetsToDirectory(const std::vector<std::string>& assets,
    const std::string& targetDir) {
    size_t moveCount = 0;

    for (const auto& assetPath : assets) {
        try {
            std::filesystem::path srcPath(assetPath);
            std::filesystem::path destPath = std::filesystem::path(targetDir) / srcPath.filename();

            // Skip if source and destination are the same
            if (std::filesystem::equivalent(srcPath.parent_path(), targetDir)) {
                continue;
            }

            // Handle name conflicts
            if (std::filesystem::exists(destPath)) {
                std::string baseName = destPath.stem().string();
                std::string extension = destPath.extension().string();
                int counter = 1;

                do {
                    destPath = std::filesystem::path(targetDir) /
                        (baseName + "_" + std::to_string(counter) + extension);
                    counter++;
                } while (std::filesystem::exists(destPath));
            }

            std::filesystem::rename(srcPath, destPath);
            moveCount++;
        }
        catch (const std::exception& e) {
            LOG_ERROR("Failed to move " << assetPath << ": " << e.what());
        }
    }

    m_statusMessage = "Moved " + std::to_string(moveCount) + " item(s)";
    m_statusTimer = 2.0f;
}

void AssetBrowserPanel::_copyAssetsToDirectory(const std::vector<std::string>& assets,
    const std::string& targetDir) {
    size_t copyCount = 0;

    for (const auto& assetPath : assets) {
        try {
            std::filesystem::path srcPath(assetPath);
            std::filesystem::path destPath = std::filesystem::path(targetDir) / srcPath.filename();

            // Skip if source and destination are the same
            if (std::filesystem::equivalent(srcPath.parent_path(), targetDir)) {
                continue;
            }

            // Handle name conflicts
            if (std::filesystem::exists(destPath)) {
                std::string baseName = destPath.stem().string();
                std::string extension = destPath.extension().string();
                int counter = 1;

                do {
                    destPath = std::filesystem::path(targetDir) /
                        (baseName + "_copy" + std::to_string(counter) + extension);
                    counter++;
                } while (std::filesystem::exists(destPath));
            }

            if (std::filesystem::is_directory(srcPath)) {
                std::filesystem::copy(srcPath, destPath,
                    std::filesystem::copy_options::recursive);
            }
            else {
                std::filesystem::copy_file(srcPath, destPath);
            }
            copyCount++;
        }
        catch (const std::exception& e) {
            LOG_ERROR("Failed to copy " << assetPath << ": " << e.what());
        }
    }

    m_statusMessage = "Copied " + std::to_string(copyCount) + " item(s)";
    m_statusTimer = 2.0f;
}