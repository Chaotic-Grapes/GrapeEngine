/* Start Header *****************************************************************/
/*!
\file   AssetBrowser.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the AssetBrowser class for browsing and managing game assets.

Features:
- File browser with breadcrumb navigation
- File selection with info display
- Display of assets folder structure
- Prefab editing and instance synchronization

References:
- Windows file dialog using Win32 API (commdlg.h)
- ImGui styling and layout functions (imgui.h)
- Breadcrumb navigation pattern & button customization adapted from ImGui examples
*/
/* End Header *******************************************************************/

#include "../editor/AssetBrowser.h"
#include "core/Logger.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "serialization/EntitySerializer.h"
#include "ecs/Entity.h"
#include "../editor/InspectorWindow.h"
#include <fstream>

// Initialize the AssetBrowser with editor fonts, world reference and event subscriptions
void AssetBrowser::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world) {
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

// Connect the InspectorWindow so prefab actions in Asset Browser
// are inspected in the unified inspector UI
void AssetBrowser::SetInspector(InspectorWindow* inspector) {
    m_inspector = inspector;
    // Propagate to AssetLibrary for double-click open behavior
    m_assetLibrary.SetInspector(inspector);
}

// Render the asset browser UI window
void AssetBrowser::Render() {
    ImGui::PushFont(m_mainFont);
    ImGui::Begin("Asset Browser");

    // Apply font scale to this window
    ImGui::SetWindowFontScale(m_fontScale);

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

    // Import button (upload icon)
    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button("\xEF\x82\x9B")) {
        m_assetLibrary._importAsset(m_currentPath, m_selectedAsset, m_statusMessage, m_statusTimer);
    }
    ImGui::PopFont();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Import new assets into current folder");
    }

    ImGui::SameLine();

    // Replace button (only enabled if a file is selected)
    bool hasSelection = !m_selectedAsset.empty();
    if (!hasSelection) ImGui::BeginDisabled();

    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button("\xEE\xA3\x94")) {
        m_assetLibrary._replaceTexture(m_selectedAsset, m_statusMessage, m_statusTimer);
    }

    ImGui::PopFont();
    if (!hasSelection) ImGui::EndDisabled();

    // Show tooltip even when disabled
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (hasSelection) {
            ImGui::SetTooltip("Replace the selected texture with a new file while keeping the original name");
        }
        else {
            ImGui::SetTooltip("Replace selected texture with a new file (disabled)");
        }
    }

    ImGui::SameLine();

    // Prefab button (only enabled if a prefab is selected)
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

    // Open ImGui popup named "Prefabs"
    if (ImGui::BeginPopup("Prefabs")) {
        // Display selectable option "Load Prefab" in the popup
        if (ImGui::Selectable("Load Prefab")) {
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
                        entity.AddComponent<Component::PrefabLink>(linkPath);

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

        // Edit prefab option: open in unified Inspector
        if (ImGui::Selectable("Edit Prefab")) {
            // Check if inspector instance is available
            if (!m_inspector) {
                LOG_WARNING("Inspector not available for prefab editing");
                m_statusMessage = "Inspector not available";
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

                        // If parsing succeeded, open in inspector and report success
                        m_inspector->InspectPrefab(m_selectedAsset);
                        m_statusMessage = "Prefab opened";
                        m_statusTimer = 3.0f;
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Failed to parse prefab file: " << e.what());
                    m_statusMessage = "Failed to open prefab";
                    m_statusTimer = 3.0f;
                }
            }
        }

        ImGui::EndPopup();
    }

    // Font scale controls
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 193);

    // Standard stuff
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderFloat("##Scale", &m_fontScale, 0.5f, 1.5f, "%.1fx")) {
        m_fontScale = std::clamp(m_fontScale, 0.5f, 1.5f);
    }

    // Show tooltip on hover
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Asset Browser UI Scale");
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset##Scale")) {
        m_fontScale = 1.0f;
    }

    // Reserve space for status bar at bottom
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float statusBarHeight = 26.0f;

    // Content region above status bar
    ImGui::BeginChild("ContentRegion", ImVec2(0, -statusBarHeight), false);

    // Left side: File/folder list (65% width)
    ImGui::BeginChild("FileList", ImVec2(windowWidth * 0.65f, 0), true);
    ImGui::SetWindowFontScale(m_fontScale);
    m_assetLibrary._displayFolder(m_currentPath, m_selectedAsset, m_currentPath);

    // Clicking empty space in file list clears EVERYTHING (File/folder list)
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered()) 
    {
        // Clear currently selected asset
        m_selectedAsset.clear();

        // Clear inspector if it exists
        if (m_inspector) {
            m_inspector->ClearSelection();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right side: File info panel (35% width)
    ImGui::BeginChild("FileInfo", ImVec2(0, 0), true);
    ImGui::SetWindowFontScale(m_fontScale);
    m_assetLibrary._displaySelectedFileInfo(m_selectedAsset);

    // Clicking empty space in file info clears EVERYTHING (File info)
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered()) {

        // So it's just the same thing (repetitive?)
        m_selectedAsset.clear();
        if (m_inspector) {
            m_inspector->ClearSelection();
        }
    }

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
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));

            // Render delete icon button
            if (ImGui::SmallButton("\xEE\xA1\xB2\##Delete2")) {
                m_assetLibrary._deleteSelectedAsset(m_selectedAsset, m_statusMessage, m_statusTimer);
            }

            // Restore style and font state
            ImGui::PopStyleColor(4);
            ImGui::PopFont();

            // Show tooltip on hover
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip(isFolder ? "Delete selected folder and all contents" : "Delete selected file");
            }
        }
    }

    ImGui::EndChild();

    // End content region above status bar
    ImGui::EndChild();

    // Status bar (fixed at bottom; does not overlap content)
    ImGui::BeginChild("StatusBar", ImVec2(0, statusBarHeight), false, ImGuiWindowFlags_NoScrollbar);
    if (m_statusTimer > 0.0f) {
        ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
            ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
            : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(color, "%s", m_statusMessage.c_str());
        m_statusTimer -= ImGui::GetIO().DeltaTime;
    }
    ImGui::EndChild();

    // Click on empty space in parent Asset Browser window to clear everything
    // !IsAnyItemHovered() prevents clearing when clicking breadcrumbs, buttons, sliders, etc.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered()) {

        m_selectedAsset.clear();

        if (m_inspector) {
            m_inspector->ClearSelection();
        }
    }

    ImGui::End();
    ImGui::PopFont();

    // Prefab editing is absorbed by Inspector; no separate window to render
}
