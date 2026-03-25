/* Start Header *****************************************************************/
/*!
\file   EditorFileMenu.cpp
\author Foo Rui Qin    (60%)
        Muhammad Nur Fadzly Bin Zulkifli (20%)
        Samantha Leong Sher Yen (20%)
\par    ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
        s.leong@digipen.edu
\date   16th November 2025

\brief
Implements the EditorFileMenu class which draws and handles the File menu in the
editor. This includes creating new scenes, opening existing scenes through an OS
file dialog and saving the active scene to disk. The menu acts as the gateway
between the editor UI and the SceneManager, ensuring scene operations remain
centralized and consistent with the currently active scene.
*/
/* End Header *******************************************************************/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN  // Prevent Windows from defining min and max macros
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <ShlObj.h>
#endif

#include "EditorFileMenu.h"
#include "HierarchyPanel.h"
#include "UndoSystem.h"
#include "EditorStyle.h"
#include "EditorIcons.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// This define lets GLFW expose native Win32 window handles so we can
// pass the editor window as the owner of the file dialog
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "core/Logger.h"
#include "core/Application.h"
#include "core/ProjectPaths.h"
#include "scene/SceneManager.h"
#include "scene/Scene.h"
#include "platform/IPlatformContext.h"
#include "ComponentWidgets.h"
#include "services/Input.h"
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstdio>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include "serialization/ConfigurationSerializer.h"
#include <filesystem>
#include <nlohmann/json.hpp>

namespace {
    // Treat dot-prefixed paths and OS-hidden entries as hidden content.
    bool IsHiddenPath(const std::filesystem::path& path) {
        // Use the leaf name so rules work for both files and folders.
        const std::string name = path.filename().string();
        // Unix-style hidden naming convention.
        if (!name.empty() && name.front() == '.') {
            return true;
        }
#ifdef _WIN32
        // Windows hidden attribute check.
        const DWORD attrs = GetFileAttributesA(path.string().c_str());
        // Only trust attributes when the query succeeded.
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            return (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
        }
#endif
        // Default to visible if no hidden signal is found.
        return false;
    }
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

// Connects this menu to the SceneManager so it can create, load and save scenes
void EditorFileMenu::Initialize(Scenes::SceneManager* sceneManager) {
    m_sceneManager = sceneManager;
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------

// Draws the "File" menu in the menu bar and handles user actions such as
// New Scene, Open Scene and Save Scene As
void EditorFileMenu::RenderFileMenu() {

    // Flag to open popup when exiting with unsaved changes (this some bug with ImGui or something)
    // Hack
    static bool openExitPopup = false;
    _finalizeExportIfDone();

    // File menu (New, Open, Save As, Exit)
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open Project...")) {
            if (m_requestProjectBrowser) {
                m_requestProjectBrowser();
            }
        }
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) { CreateNewScene(); }
        if (ImGui::MenuItem("Open Scene", "Ctrl+O")) { OpenSceneDialog(); }

        // Determine whether there is an active scene to enable/disable save actions
        bool hasActiveScene = false;
        if (m_sceneManager) {
            size_t idx = m_sceneManager->GetActiveIndex();
            hasActiveScene = (idx != static_cast<size_t>(-1));
        }

        // Determine if we're in edit mode (not playing)
        bool isInEditMode = true;
        if (m_getEditorState) {
            isInEditMode = (m_getEditorState() == EditorState::Edit);
        }

        // Determine if save should be enabled:
        // - Must have an active scene
        // - Must be in edit mode (not playing)
        // - Must have unsaved changes
        bool canSave = hasActiveScene && isInEditMode && m_hasUnsavedChanges;

        // Show "Save Scene*" in BOLD when there are unsaved changes
        if (m_hasUnsavedChanges && m_boldFont) {
            ImGui::PushFont(m_boldFont);
            // When not in edit mode or no unsaved changes, grey out the button
            if (!isInEditMode) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
            }
            bool clicked = ImGui::MenuItem("Save Scene*", "Ctrl+S", false, canSave);
            if (!isInEditMode) {
                ImGui::PopStyleColor();
            }
            ImGui::PopFont();
            if (clicked) SaveScene();
        }
        else {
            // Normal font when no unsaved changes - always grey out
            if (!isInEditMode) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
            }
            bool clicked = ImGui::MenuItem("Save Scene", "Ctrl+S", false, canSave);
            if (!isInEditMode) {
                ImGui::PopStyleColor();
            }
            if (clicked) SaveScene();
        }

        // "Save Scene As..." is also disabled in play mode
        if (!isInEditMode) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
        }
        bool saveAsClicked = ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S", false, hasActiveScene && isInEditMode);
        if (!isInEditMode) {
            ImGui::PopStyleColor();
        }
        if (saveAsClicked) SaveSceneAsDialog();

        ImGui::Separator();
        if (ImGui::MenuItem("Export Project...")) {
            // Open the export analyzer window.
            m_showBuildSizeAnalyzer = true;
            // Force a rescan every time the window is opened from the menu.
            m_buildSizeNeedsRefresh = true;
            // Clear stale status text from prior runs.
            m_buildSizeStatusMessage.clear();
            // Reset status severity.
            m_buildSizeStatusIsError = false;
        }

        ImGui::Separator();
        // Make the Exit menu item visually distinct (danger background)
        ImGui::PushStyleColor(ImGuiCol_Header, EditorStyle::DangerButton);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorStyle::DangerButtonHover);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorStyle::DangerButtonActive);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        bool exitClicked = ImGui::MenuItem("Exit");
        ImGui::PopStyleColor(4);
        if (exitClicked) {
            // If there are unsaved changes prompt the user to save before exiting
            if (m_hasUnsavedChanges) {
                openExitPopup = true;
            }
            else {
                if (Engine::CORE) Engine::CORE->Close();
            }
        }
        ImGui::EndMenu();
    }

    // Trigger the exit popup if needed
    // Hack
    if (openExitPopup) {
        ImGui::OpenPopup("Unsaved Changes##ExitPopup");
        openExitPopup = false;
    }

    // If user attempted to exit while there are unsaved changes show a modal
    if (ImGui::BeginPopupModal("Unsaved Changes##ExitPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("The current scene has unsaved changes. Do you want to save before exiting?");
        ImGui::Separator();

        // Yes: Save then Exit
        if (ImGui::Button("Yes", ImVec2(80, 0))) {
            SaveScene();
            if (Engine::CORE) Engine::CORE->Close();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        // No: Exit without saving
        // No: Exit without saving (styled as a danger button)
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::DangerButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::DangerButtonHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::DangerButtonActive);
        if (ImGui::Button("No", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
            if (Engine::CORE) Engine::CORE->Close();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        // Cancel: Do nothing and close the popup
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    _renderBuildSizeAnalyzerWindow();
    _renderExportSummaryPopup();
}

std::string EditorFileMenu::_formatBytes(std::uintmax_t bytes) const {
    // Human-readable unit table.
    static const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    // Work in floating point once we leave byte units.
    double size = static_cast<double>(bytes);
    // Current unit index in the units table.
    std::size_t unitIndex = 0;
    // Scale by 1024 until size is in a readable range.
    while (size >= 1024.0 && unitIndex < (std::size(units) - 1)) {
        size /= 1024.0;
        ++unitIndex;
    }

    // Output formatter used for the UI text.
    std::ostringstream out;
    // Keep bytes as integer text.
    if (unitIndex == 0) {
        out << bytes << ' ' << units[unitIndex];
    }
    else {
        // Show two decimals for KB and above.
        out.setf(std::ios::fixed);
        out.precision(2);
        out << size << ' ' << units[unitIndex];
    }
    // Return formatted size string.
    return out.str();
}

void EditorFileMenu::_recomputeBuildSizeSelectionTotals() {
    // Reset aggregate counters before accumulation.
    m_buildSizeSelectedBytes = 0;
    m_buildSizeSelectedCount = 0;

    // Sum selected files only.
    for (const auto& asset : m_buildSizeAssets) {
        if (!asset.Selected) {
            continue;
        }
        // Add selected file size.
        m_buildSizeSelectedBytes += asset.SizeBytes;
        // Count selected file.
        ++m_buildSizeSelectedCount;
    }
}

void EditorFileMenu::_setAllBuildSizeSelections(bool selected) {
    // Apply one state to the entire selection set.
    for (auto& asset : m_buildSizeAssets) {
        asset.Selected = selected;
    }
    // Recompute footer totals.
    _recomputeBuildSizeSelectionTotals();
    // Persist bulk selection changes.
    _saveBuildSizeSelectionCache();
}

void EditorFileMenu::_loadBuildSizeSelectionCache() {
    // Start from an empty in-memory cache.
    m_buildSizeSelectionCache.clear();

    if (!Engine::ProjectPaths::IsInitialized()) {
        return;
    }

    const std::filesystem::path projectRoot = Engine::ProjectPaths::GetProjectRoot();
    if (projectRoot.empty()) {
        return;
    }

    // Cache file lives at project root.
    const std::filesystem::path cachePath = projectRoot / ".export.json";
    // No cache yet is not an error.
    if (!std::filesystem::exists(cachePath)) {
        return;
    }

    try {
        // Open persisted JSON selection map.
        std::ifstream in(cachePath);
        if (!in.is_open()) {
            return;
        }

        // Parse file contents into JSON.
        nlohmann::json payload;
        in >> payload;
        // Ignore malformed layouts.
        if (!payload.contains("assetSelection") || !payload["assetSelection"].is_object()) {
            return;
        }

        // Restore every persisted relative-path selection value.
        for (const auto& [relativePath, selectedNode] : payload["assetSelection"].items()) {
            if (!selectedNode.is_boolean()) {
                continue;
            }
            m_buildSizeSelectionCache[relativePath] = selectedNode.get<bool>();
        }

        // Mark that we loaded persisted state successfully.
        m_buildSizeSelectionLoaded = true;
    }
    catch (const std::exception& e) {
        LOG_WARNING("Failed to read Export Project selection cache: " << e.what());
    }
}

void EditorFileMenu::_saveBuildSizeSelectionCache() const {
    if (!Engine::ProjectPaths::IsInitialized()) {
        return;
    }

    const std::filesystem::path projectRoot = Engine::ProjectPaths::GetProjectRoot();
    if (projectRoot.empty()) {
        return;
    }

    // Cache file path at project root.
    const std::filesystem::path cachePath = projectRoot / ".export.json";

    try {
        // Build fresh JSON payload.
        nlohmann::json payload;
        payload["assetSelection"] = nlohmann::json::object();
        // Serialize current check states.
        for (const auto& asset : m_buildSizeAssets) {
            payload["assetSelection"][asset.RelativePath] = asset.Selected;
        }

        // Overwrite previous cache atomically enough for this use-case.
        std::ofstream out(cachePath, std::ios::trunc);
        if (!out.is_open()) {
            LOG_WARNING("Failed to write Export Project selection cache at: " << cachePath.string());
            return;
        }

        // Pretty-print for easy manual inspection.
        out << payload.dump(2);
    }
    catch (const std::exception& e) {
        LOG_WARNING("Failed to save Export Project selection cache: " << e.what());
    }
}

void EditorFileMenu::_refreshBuildSizeAnalyzerAssets() {
    // Rebuild analyzer data from scratch.
    m_buildSizeAssets.clear();
    m_buildSizeSelectionLoaded = false;

    if (!Engine::ProjectPaths::IsInitialized()) {
        m_buildSizeStatusIsError = true;
        m_buildSizeStatusMessage = "Project paths are not initialized.";
        m_buildSizeNeedsRefresh = false;
        _recomputeBuildSizeSelectionTotals();
        return;
    }

    const std::filesystem::path projectRoot = Engine::ProjectPaths::GetProjectRoot();
    if (projectRoot.empty() || !std::filesystem::exists(projectRoot) || !std::filesystem::is_directory(projectRoot)) {
        m_buildSizeStatusIsError = true;
        m_buildSizeStatusMessage = "Project root is invalid.";
        m_buildSizeNeedsRefresh = false;
        _recomputeBuildSizeSelectionTotals();
        return;
    }

    // Pull previous check state before scanning.
    _loadBuildSizeSelectionCache();

    // Walk entire project tree while skipping permission failures.
    std::error_code walkError;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator it(projectRoot, options, walkError), end;
         it != end;
         it.increment(walkError)) {
        if (walkError) {
            // Recover from per-entry traversal issues.
            walkError.clear();
            continue;
        }

        // Prune hidden folders and everything below them.
        std::error_code dirTypeError;
        if (it->is_directory(dirTypeError) && !dirTypeError && IsHiddenPath(it->path())) {
            it.disable_recursion_pending();
            continue;
        }

        // Keep files only.
        std::error_code typeError;
        if (!it->is_regular_file(typeError) || typeError) {
            continue;
        }

        // Skip hidden files.
        if (IsHiddenPath(it->path())) {
            continue;
        }

        // Convert to path key relative to project root.
        std::error_code relError;
        std::filesystem::path relativePath = std::filesystem::relative(it->path(), projectRoot, relError);
        if (relError) {
            continue;
        }

        // Query file byte size.
        std::error_code sizeError;
        const std::uintmax_t sizeBytes = it->file_size(sizeError);
        if (sizeError) {
            continue;
        }

        // Reuse persisted selection state when available.
        const std::string key = relativePath.generic_string();
        bool selected = true;
        const auto cachedIt = m_buildSizeSelectionCache.find(key);
        if (cachedIt != m_buildSizeSelectionCache.end()) {
            selected = cachedIt->second;
        }

        // Push row for tree rendering.
        m_buildSizeAssets.push_back({ key, sizeBytes, selected });
    }

    // Stable alphabetical ordering for deterministic UI.
    std::sort(m_buildSizeAssets.begin(), m_buildSizeAssets.end(),
        [](const BuildSizeAssetEntry& lhs, const BuildSizeAssetEntry& rhs) {
            return lhs.RelativePath < rhs.RelativePath;
        });

    // Finalize totals and status.
    _recomputeBuildSizeSelectionTotals();
    m_buildSizeNeedsRefresh = false;
    m_buildSizeStatusIsError = false;
    m_buildSizeStatusMessage = "Found " + std::to_string(m_buildSizeAssets.size()) + " assets.";
}

void EditorFileMenu::_renderBuildSizeAnalyzerWindow() {
    // Skip work when the export window is closed.
    if (!m_showBuildSizeAnalyzer) {
        return;
    }

    // Remember open state so we can persist when closing.
    const bool wasOpen = m_showBuildSizeAnalyzer;
    ImGui::SetNextWindowSize(ImVec2(900.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Export Project", &m_showBuildSizeAnalyzer)) {
        // Lazy-refresh when flagged.
        if (m_buildSizeNeedsRefresh) {
            _refreshBuildSizeAnalyzerAssets();
        }

        // Manual rescan action.
        if (ImGui::Button("Refresh")) {
            m_buildSizeNeedsRefresh = true;
            _refreshBuildSizeAnalyzerAssets();
        }
        ImGui::SameLine();
        // Bulk select helper.
        if (ImGui::Button("Select All")) {
            _setAllBuildSizeSelections(true);
        }
        ImGui::SameLine();
        // Bulk deselect helper.
        if (ImGui::Button("Deselect All")) {
            _setAllBuildSizeSelections(false);
        }

        // Status color reflects error/success state.
        if (!m_buildSizeStatusMessage.empty()) {
            const ImVec4 color = m_buildSizeStatusIsError
                ? ImVec4(0.90f, 0.30f, 0.30f, 1.0f)
                : ImVec4(0.30f, 0.85f, 0.40f, 1.0f);
            ImGui::TextColored(color, "%s", m_buildSizeStatusMessage.c_str());
        }

        ImGui::Separator();
        ImGui::Text("Assets: %zu", m_buildSizeAssets.size());
        ImGui::Text("Selected: %zu", m_buildSizeSelectedCount);
        ImGui::Text("Selected Size: %s", _formatBytes(m_buildSizeSelectedBytes).c_str());
        ImGui::Separator();

        // Reserve footer height below the tree panel.
        const float footerHeight = ImGui::GetFrameHeightWithSpacing() * 1.6f;
        // Track whether any checkbox changed this frame.
        bool selectionChanged = false;

        if (ImGui::BeginChild("ExportProjectTree", ImVec2(0.0f, -footerHeight), true)) {
            // Group file rows by directory path for tree rendering.
            std::unordered_map<std::string, std::vector<std::size_t>> filesByDirectory;
            // Track parent-to-children directory edges.
            std::unordered_map<std::string, std::vector<std::string>> childDirectories;
            // Deduplicate discovered tree edges.
            std::unordered_set<std::string> edgeSet;

            for (std::size_t i = 0; i < m_buildSizeAssets.size(); ++i) {
                // Parse each file path into parent directory chain.
                const std::filesystem::path relPath = std::filesystem::path(m_buildSizeAssets[i].RelativePath);
                const std::filesystem::path parentPath = relPath.parent_path();

                // Build parent->child directory links from path components.
                std::string currentDir;
                for (const auto& component : parentPath) {
                    const std::string part = component.string();
                    // Ignore empty and dot components.
                    if (part.empty() || part == ".") {
                        continue;
                    }

                    // Record unique edge then append child.
                    const std::string nextDir = currentDir.empty() ? part : (currentDir + "/" + part);
                    const std::string edgeKey = currentDir + "->" + nextDir;
                    if (edgeSet.insert(edgeKey).second) {
                        childDirectories[currentDir].push_back(nextDir);
                    }
                    currentDir = nextDir;
                }

                // Register file index under its terminal directory.
                filesByDirectory[currentDir].push_back(i);
            }

            // Sort children for deterministic tree order.
            for (auto& [parent, children] : childDirectories) {
                std::sort(children.begin(), children.end());
            }

            // Compute selection stats for a directory subtree.
            auto directorySelectionState = [&](const std::string& dirPath) {
                std::size_t selectedCount = 0;
                std::size_t totalCount = 0;
                const std::string prefix = dirPath.empty() ? std::string() : (dirPath + "/");

                // Walk all files and count those belonging to this subtree.
                for (const auto& asset : m_buildSizeAssets) {
                    const bool belongsToDirectory = dirPath.empty()
                        ? true
                        : (asset.RelativePath == dirPath || asset.RelativePath.rfind(prefix, 0) == 0);
                    if (!belongsToDirectory) {
                        continue;
                    }

                    ++totalCount;
                    if (asset.Selected) {
                        ++selectedCount;
                    }
                }

                // Return selected and total counts.
                return std::pair<std::size_t, std::size_t>(selectedCount, totalCount);
            };

            // Apply a selection state to all files in one subtree.
            auto setDirectorySelection = [&](const std::string& dirPath, const bool selected) {
                const std::string prefix = dirPath.empty() ? std::string() : (dirPath + "/");
                for (auto& asset : m_buildSizeAssets) {
                    const bool belongsToDirectory = dirPath.empty()
                        ? true
                        : (asset.RelativePath == dirPath || asset.RelativePath.rfind(prefix, 0) == 0);
                    if (belongsToDirectory) {
                        // Update the row state and mark dirty.
                        asset.Selected = selected;
                        selectionChanged = true;
                    }
                }
            };

            // Recursive renderer for directory nodes.
            std::function<void(const std::string&)> renderDirectoryNode;
            renderDirectoryNode = [&](const std::string& dirPath) {
                // Evaluate current subtree tri-state.
                const auto [selectedCount, totalCount] = directorySelectionState(dirPath);
                bool dirSelected = (totalCount > 0 && selectedCount == totalCount);
                const bool mixed = (selectedCount > 0 && selectedCount < totalCount);

                ImGui::PushID(dirPath.c_str());
                // Directory checkbox controls the subtree.
                if (ImGui::Checkbox("##dir_select", &dirSelected)) {
                    setDirectorySelection(dirPath, dirSelected);
                }
                // Lightweight partial-state marker.
                if (mixed) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("~");
                }

                ImGui::SameLine();
                // Show readable label for root vs child folder.
                const std::string folderLabel = dirPath.empty()
                    ? std::string("Project Root")
                    : std::filesystem::path(dirPath).filename().string();

                // Resolve file and child lists for this directory.
                const auto filesIt = filesByDirectory.find(dirPath);
                const auto childrenIt = childDirectories.find(dirPath);
                const bool hasFiles = (filesIt != filesByDirectory.end() && !filesIt->second.empty());
                const bool hasChildren = (childrenIt != childDirectories.end() && !childrenIt->second.empty());

                // Leaf nodes have no files and no child folders.
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
                if (!hasFiles && !hasChildren) {
                    flags |= ImGuiTreeNodeFlags_Leaf;
                }

                const std::string nodeId = "dir_node_" + (dirPath.empty() ? std::string("root") : dirPath);
                // Expand/collapse directory row.
                const bool open = ImGui::TreeNodeEx(nodeId.c_str(), flags, "%s", folderLabel.c_str());

                if (open) {
                    // Render files under this directory.
                    if (hasFiles) {
                        for (const std::size_t index : filesIt->second) {
                            BuildSizeAssetEntry& entry = m_buildSizeAssets[index];
                            ImGui::PushID(static_cast<int>(index));
                            bool selected = entry.Selected;
                            // File checkbox updates single row selection.
                            if (ImGui::Checkbox("##file_select", &selected)) {
                                entry.Selected = selected;
                                selectionChanged = true;
                            }
                            ImGui::SameLine();
                            ImGui::TextUnformatted(std::filesystem::path(entry.RelativePath).filename().string().c_str());
                            ImGui::SameLine();
                            ImGui::TextDisabled("(%s)", _formatBytes(entry.SizeBytes).c_str());
                            ImGui::PopID();
                        }
                    }

                    // Recurse through child directories.
                    if (hasChildren) {
                        for (const std::string& childPath : childrenIt->second) {
                            renderDirectoryNode(childPath);
                        }
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();
            };

            // Render the tree from synthetic root.
            renderDirectoryNode("");
        }
        ImGui::EndChild();

        // Persist checkbox changes and recalc totals.
        if (selectionChanged) {
            _recomputeBuildSizeSelectionTotals();
            _saveBuildSizeSelectionCache();
        }

        // Disable export button when no assets are selected.
        const bool canExport = (m_buildSizeSelectedCount > 0);
        if (!canExport) {
            ImGui::BeginDisabled();
        }
        // Start export with explicit selected-asset set.
        if (ImGui::Button("Export Selected Assets...", ImVec2(220.0f, 0.0f))) {
            std::string destination;
#ifdef _WIN32
            // Ask user for destination folder.
            destination = _pickExportFolder();
#endif
            if (destination.empty()) {
                // Cancel is non-error feedback.
                m_buildSizeStatusIsError = false;
                m_buildSizeStatusMessage = "Export cancelled.";
            }
            else {
                // Build a hash-set for O(1) selection checks in export filtering.
                std::unordered_set<std::string> selectedAssets;
                selectedAssets.reserve(m_buildSizeSelectedCount);
                for (const auto& asset : m_buildSizeAssets) {
                    if (asset.Selected) {
                        selectedAssets.insert(asset.RelativePath);
                    }
                }

                // Persist latest selection state before launch.
                _saveBuildSizeSelectionCache();
                // Run full build/export pipeline with selection filter.
                _exportProject(destination, selectedAssets);
                m_buildSizeStatusIsError = false;
                m_buildSizeStatusMessage = "Export started. See Export Summary for progress.";
            }
        }
        if (!canExport) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(120.0f, 0.0f))) {
            m_showBuildSizeAnalyzer = false;
        }
    }
    ImGui::End();

    // Persist when window transitions from open to closed.
    if (wasOpen && !m_showBuildSizeAnalyzer) {
        _saveBuildSizeSelectionCache();
    }
}

void EditorFileMenu::_exportProject(const std::string& destinationOverride,
    const std::unordered_set<std::string>& selectedAssets) {
    // Prevent concurrent exports from overlapping.
    if (m_exportInProgress.load()) {
        std::lock_guard<std::mutex> lock(m_exportMutex);
        m_exportResults.clear();
        m_exportResults.push_back({ "Export", false, "Export already in progress.", "" });
        m_openExportSummary = true;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_exportMutex);
        m_exportResults.clear();
        m_exportDestination.clear();
    }

    if (!Engine::ProjectPaths::IsInitialized()) {
        LOG_WARNING("ProjectPaths not initialized; cannot export project");
        std::lock_guard<std::mutex> lock(m_exportMutex);
        m_exportResults.push_back({ "Validate project paths", false, "ProjectPaths not initialized.", "" });
        m_openExportSummary = true;
        return;
    }

    const std::filesystem::path projectRoot = Engine::ProjectPaths::GetProjectRoot();
    if (projectRoot.empty()) {
        LOG_WARNING("Project root is empty; cannot export project");
        std::lock_guard<std::mutex> lock(m_exportMutex);
        m_exportResults.push_back({ "Validate project root", false, "Project root is empty.", "" });
        m_openExportSummary = true;
        return;
    }

    // Use provided destination when invoked from export window; otherwise open picker.
    const std::string destFolder = !destinationOverride.empty()
        ? destinationOverride
        :
#ifdef _WIN32
        _pickExportFolder();
#else
        "";
#endif

    if (destFolder.empty()) {
        return;
    }

    std::filesystem::path destinationRoot = std::filesystem::path(destFolder);
    std::error_code ec;
    const std::filesystem::path projectRootAbs = std::filesystem::absolute(projectRoot, ec);
    const std::filesystem::path destAbs = std::filesystem::absolute(destinationRoot, ec);

    if (!projectRootAbs.empty() && !destAbs.empty()) {
        const auto projStr = projectRootAbs.string();
        const auto destStr = destAbs.string();
        if (destStr.rfind(projStr, 0) == 0) {
            std::lock_guard<std::mutex> lock(m_exportMutex);
            m_exportResults.push_back({ "Validate destination", false, "Destination cannot be inside the project root.", "" });
            m_openExportSummary = true;
            return;
        }
    }

    const std::string projectName = projectRoot.filename().string().empty()
        ? "GrapeGame"
        : projectRoot.filename().string();

    auto findRepoRoot = [&](const std::filesystem::path& startPath) -> std::filesystem::path {
        std::filesystem::path current = startPath;
        while (!current.empty()) {
            if (std::filesystem::exists(current / "CMakeLists.txt") &&
                std::filesystem::exists(current / "engine")) {
                return current;
            }
            current = current.parent_path();
        }
        return {};
    };

    std::filesystem::path repoRoot;
    {
        std::error_code cwdEc;
        const std::filesystem::path cwd = std::filesystem::current_path(cwdEc);
        if (!cwdEc) {
            repoRoot = findRepoRoot(cwd);
        }
    }
    if (repoRoot.empty()) {
        repoRoot = findRepoRoot(projectRoot);
    }
    if (repoRoot.empty()) {
        std::lock_guard<std::mutex> lock(m_exportMutex);
        m_exportResults.push_back({ "Locate repo root", false, "Could not locate engine repo root.", "" });
        m_openExportSummary = true;
        return;
    }

    const std::filesystem::path buildRoot = repoRoot / "build_game";
    const std::filesystem::path exportRoot = buildRoot / "export" / projectName / "Release";
    // Export always lands in a project-named child folder.
    destinationRoot /= projectName;

    {
        std::lock_guard<std::mutex> lock(m_exportMutex);
        m_exportStepNames = {
            "Clean build folder",
            "Configure game build",
            "Build & export game",
            "Validate export output",
            "Compile scripts",
            "Copy project settings",
            "Prepare destination",
            "Copy export output"
        };
    }
    m_exportCurrentStep = 0;
    m_exportInProgress = true;
    m_exportDone = false;
    m_openExportSummary = true;

    // Launch long-running export work off the UI thread.
    m_exportThread = std::thread([this, projectRoot, repoRoot, projectName, buildRoot, exportRoot, destinationRoot, selectedAssets]() {
        auto pushResult = [&](const ExportStepResult& result) {
            // Record step result in a thread-safe way for summary UI.
            std::lock_guard<std::mutex> lock(m_exportMutex);
            m_exportResults.push_back(result);
        };

        try {
            // Helper to run external commands and capture output.
            auto runCommand = [&](const std::string& command, const std::string& stepName) {
                std::string output;
                // Start time for per-step duration reporting.
                const auto start = std::chrono::steady_clock::now();
                {
                    // Update active step index for progress UI.
                    std::lock_guard<std::mutex> lock(m_exportMutex);
                    const auto it = std::find(m_exportStepNames.begin(), m_exportStepNames.end(), stepName);
                    if (it != m_exportStepNames.end()) {
                        m_exportCurrentStep = static_cast<int>(std::distance(m_exportStepNames.begin(), it));
                    }
                }
                int result = 1;
                bool started = false;

#ifdef _WIN32
                const std::string fullCmd = "cmd.exe /C " + command;

                SECURITY_ATTRIBUTES sa = {};
                sa.nLength = sizeof(sa);
                sa.bInheritHandle = TRUE;

                HANDLE readPipe = nullptr;
                HANDLE writePipe = nullptr;
                if (CreatePipe(&readPipe, &writePipe, &sa, 0)) {
                    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

                    STARTUPINFOA si = {};
                    si.cb = sizeof(si);
                    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
                    si.wShowWindow = SW_HIDE;
                    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
                    si.hStdOutput = writePipe;
                    si.hStdError = writePipe;

                    PROCESS_INFORMATION pi = {};
                    std::vector<char> cmdLine(fullCmd.begin(), fullCmd.end());
                    cmdLine.push_back('\0');

                    started = CreateProcessA(
                        nullptr,
                        cmdLine.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        CREATE_NO_WINDOW,
                        nullptr,
                        nullptr,
                        &si,
                        &pi) == TRUE;

                    CloseHandle(writePipe);
                    writePipe = nullptr;

                    if (started) {
                        char buffer[512] = {};
                        DWORD bytesAvailable = 0;
                        DWORD bytesRead = 0;

                        for (;;) {
                            while (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0) {
                                if (!ReadFile(readPipe, buffer, static_cast<DWORD>(sizeof(buffer)), &bytesRead, nullptr) || bytesRead == 0) {
                                    break;
                                }
                                output.append(buffer, bytesRead);
                            }

                            const DWORD waitResult = WaitForSingleObject(pi.hProcess, 20);
                            if (waitResult == WAIT_OBJECT_0) {
                                break;
                            }
                        }

                        while (ReadFile(readPipe, buffer, static_cast<DWORD>(sizeof(buffer)), &bytesRead, nullptr) && bytesRead > 0) {
                            output.append(buffer, bytesRead);
                        }

                        DWORD exitCode = 1;
                        GetExitCodeProcess(pi.hProcess, &exitCode);
                        result = static_cast<int>(exitCode);

                        CloseHandle(pi.hThread);
                        CloseHandle(pi.hProcess);
                    }

                    CloseHandle(readPipe);
                }
#else
                const std::string fullCmd = "cmd /C " + command + " 2>&1";
                FILE* pipe = _popen(fullCmd.c_str(), "r");
                started = (pipe != nullptr);
                if (started) {
                    char buffer[512] = {};
                    while (fgets(buffer, static_cast<int>(sizeof(buffer)), pipe)) {
                        output.append(buffer);
                    }
                    result = _pclose(pipe);
                }
#endif
                // Compute elapsed wall time.
                const auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - start).count();

                if (!started) {
                    pushResult({ stepName, false, "Failed to start command.", "" });
                    return false;
                }
                if (result != 0) {
                    pushResult({ stepName, false, "Command failed after " + std::to_string(elapsedSeconds) + "s.", output });
                    return false;
                }
                pushResult({ stepName, true, "OK (" + std::to_string(elapsedSeconds) + "s)", "" });
                return true;
            };

            // Clean stale build output to avoid stale artifacts.
            std::error_code cleanEc;
            std::filesystem::remove_all(buildRoot, cleanEc);
            if (cleanEc) {
                pushResult({ "Clean build folder", false, "Failed to clear build_game directory.", "" });
                m_exportDone = true;
                m_exportInProgress = false;
                return;
            }
            pushResult({ "Clean build folder", true, "OK", "" });

            // Configure build folder for runtime export target.
            const std::string configureCmd =
                "cmake -S \"" + repoRoot.string() + "\" -B \"" + buildRoot.string() + "\" "
                "-G \"Visual Studio 17 2022\" -A x64 -DBUILD_EDITOR=OFF -DBUILD_GAME=ON "
                "-DEXPORT_PROJECT_NAME=\"" + projectName + "\" "
                "-DEXPORT_PROJECT_DIR=\"" + projectRoot.string() + "\" "
                "-DGAME_OUTPUT_NAME=\"" + projectName + "\"";

            if (!runCommand(configureCmd, "Configure game build")) {
                m_exportDone = true;
                m_exportInProgress = false;
                return;
            }

            // Build export target with machine concurrency.
            const unsigned int jobs = (std::max)(1u, std::thread::hardware_concurrency());
            const std::string buildCmd =
                "cmake --build \"" + buildRoot.string() + "\" --config Release --target ExportGame --parallel " + std::to_string(jobs);

            if (!runCommand(buildCmd, "Build & export game")) {
                m_exportDone = true;
                m_exportInProgress = false;
                return;
            }

            // Verify expected export output exists.
            if (!std::filesystem::exists(exportRoot)) {
                m_exportCurrentStep = 3;
                pushResult({ "Validate export output", false, "Export output folder not found after build.", "" });
                m_exportDone = true;
                m_exportInProgress = false;
                return;
            }

            // Track whether scripts must be compiled for this project.
            bool shouldCompileScripts = true;
            bool foundCsScript = false;
            std::error_code scriptScanEc;

            // Scan the project directory for any .cs scripts.
            // If we find any, we will attempt to compile them into a GameScripts.dll in the export output.
            for (std::filesystem::recursive_directory_iterator it(projectRoot, std::filesystem::directory_options::skip_permission_denied, scriptScanEc),
                 end; !scriptScanEc && it != end;
                 it.increment(scriptScanEc)) {
                if (it->is_regular_file(scriptScanEc) && it->path().extension() == ".cs") {
                    foundCsScript = true;
                    break;
                }
            }

            // If there was an error during the script scan, log a warning but continue with the export since scripts are optional 
            // and we don't want to fail the entire export just because of an issue scanning for scripts. If we found .cs scripts, 
            // we will attempt to compile them, but if we didn't find any then we can skip the compile step entirely.
            // Optional script compilation is skipped when no C# scripts are present.
            if (scriptScanEc) {
                LOG_WARNING("Failed to fully scan project scripts for export; attempting compile anyway. Error: " << scriptScanEc.message());
            } else if (!foundCsScript) {
                shouldCompileScripts = false;
                m_exportCurrentStep = 4;
                pushResult({ "Compile scripts", true, "Skipped: no .cs scripts found in project.", "" });
            }

            // If we found .cs scripts, attempt to compile them. 
            // If the compile fails, we will fail the export since scripts are likely a critical part of the game project, 
            // but if we didn't find any then we can skip this step entirely and still produce a valid export.
            // Build managed scripts into export output when required.
            if (shouldCompileScripts) {
                const std::filesystem::path scriptsOutput = exportRoot / "GameScripts.dll";
                const std::filesystem::path scriptProject = repoRoot / "managed" / "tools" / "ScriptCompiler" / "ScriptCompiler.csproj";
                const std::string scriptCmd =
                    "dotnet run --project \"" + scriptProject.string() + "\" --configuration Release -- "
                    "\"" + projectRoot.string() + "\" \"" + scriptsOutput.string() + "\"";

                if (!runCommand(scriptCmd, "Compile scripts")) {
                    m_exportDone = true;
                    m_exportInProgress = false;
                    return;
                }
            }

            m_exportCurrentStep = 5;
            const std::filesystem::path exportProjectDir = exportRoot / projectName;
            const std::filesystem::path localSettingsPath = projectRoot / "ProjectSettings.json";
            if (!std::filesystem::exists(localSettingsPath)) {
                pushResult({ "Copy project settings", false, "ProjectSettings.json not found in project root.", "" });
                m_exportDone = true;
                m_exportInProgress = false;
                return;
            }

            std::error_code createProjectDirEc;
            std::filesystem::create_directories(exportProjectDir, createProjectDirEc);
            if (createProjectDirEc) {
                pushResult({ "Copy project settings", false, "Failed to prepare export project folder.", "" });
                m_exportDone = true;
                m_exportInProgress = false;
                return;
            }

            // Copy the ProjectSettings.json to the export output
            const std::filesystem::path exportSettingsPath = exportProjectDir / "ProjectSettings.json";
            std::error_code settingsCopyEc;
            std::filesystem::copy_file(localSettingsPath, exportSettingsPath,
                std::filesystem::copy_options::overwrite_existing, settingsCopyEc);
            if (settingsCopyEc) {
                pushResult({ "Copy project settings", false, "Failed to copy ProjectSettings.json.", "" });
                m_exportDone = true;
                m_exportInProgress = false;
                return;
            }

            // After copying the ProjectSettings.json, we need to ensure that the StartupScene path is valid for the exported project
            try {
                std::ifstream settingsIn(exportSettingsPath);
                nlohmann::json settingsJson;
                settingsIn >> settingsJson;

                // If there is no StartupScene setting or it's empty, we can't validate it, so we will just warn the user but continue with the export since they may not be using a startup scene at all
                // But if there is a StartupScene setting, we will attempt to validate and fix it for the exported project, but if it's missing or empty then we can skip this step entirely
                std::string startupSceneSetting = settingsJson.value("StartupScene", std::string{});
                if (startupSceneSetting.empty()) {
                    pushResult({ "Copy project settings", false, "StartupScene is empty in ProjectSettings.json.", "" });
                    m_exportDone = true;
                    m_exportInProgress = false;
                    return;
                }

                // If the StartupScene path is absolute, we will check if it can be made relative to the project root
                // If it can, we will convert it to a relative path and update the settings for the exported project
                std::filesystem::path startupScenePath(startupSceneSetting);
                if (startupScenePath.is_absolute()) {
                    std::error_code relEc;
                    const std::filesystem::path rel = std::filesystem::relative(startupScenePath, projectRoot, relEc);
                    if (!relEc && !rel.empty() && rel.begin() != rel.end() && *rel.begin() != "..") {
                        startupScenePath = rel;
                        settingsJson["StartupScene"] = startupScenePath.generic_string();
                    } else {
                        pushResult({ "Copy project settings", false, "StartupScene must be inside the project folder for export.", "" });
                        m_exportDone = true;
                        m_exportInProgress = false;
                        return;
                    }
                }

                // Handle rooted-relative inputs like "/Scenes/Main.scene" and normalize "."/"..".
                if (!startupScenePath.is_absolute() && startupScenePath.has_root_directory()) {
                    startupScenePath = startupScenePath.relative_path();
                }
                startupScenePath = startupScenePath.lexically_normal();
                if (startupScenePath.empty() || startupScenePath == "." ||
                    (startupScenePath.begin() != startupScenePath.end() && *startupScenePath.begin() == "..")) {
                    pushResult({ "Copy project settings", false, "StartupScene must resolve inside the project folder.", "" });
                    m_exportDone = true;
                    m_exportInProgress = false;
                    return;
                }
                settingsJson["StartupScene"] = startupScenePath.generic_string();

                // Now we will validate that the StartupScene path (whether originally relative or converted to relative) actually exists in the exported project
                std::filesystem::path startupSceneResolved = startupScenePath;
                if (!startupSceneResolved.is_absolute()) {
                    startupSceneResolved = exportProjectDir / startupSceneResolved;
                }

                // If the resolved StartupScene path does not exist in the exported project, we will fail the export since the exported game would not be able to 
                // find its startup scene and would be effectively broken, but if it does exist then we can proceed with the export as normal
                if (!std::filesystem::exists(startupSceneResolved)) {
                    // Support legacy settings that include "<ProjectName>/" prefix.
                    std::filesystem::path trimmedPath = startupScenePath;
                    if (trimmedPath.begin() != trimmedPath.end() && trimmedPath.begin()->string() == projectName) {
                        trimmedPath = trimmedPath.lexically_relative(projectName);
                        startupSceneResolved = exportProjectDir / trimmedPath;
                    }

                    if (!std::filesystem::exists(startupSceneResolved)) {
                        pushResult({ "Copy project settings", false, "StartupScene path does not exist in exported project.", "" });
                        m_exportDone = true;
                        m_exportInProgress = false;
                        return;
                    }

                    startupScenePath = trimmedPath;
                    settingsJson["StartupScene"] = startupScenePath.generic_string();
                }

                std::ofstream settingsOut(exportSettingsPath, std::ios::trunc);
                settingsOut << settingsJson.dump(4);
            } catch (const std::exception& ex) {
                pushResult({ "Copy project settings", false, std::string("Invalid ProjectSettings.json: ") + ex.what(), "" });
                m_exportDone = true;
                m_exportInProgress = false;
                return;
            }

            pushResult({ "Copy project settings", true, "OK", "" });

            // Error code to remove existing destination if it exists, create destination directory, and copy export output to destination
            // Prepare destination root by deleting previous output if present.
            std::error_code removeDestEc;
            if (std::filesystem::exists(destinationRoot)) {
                m_exportCurrentStep = 6;
                std::filesystem::remove_all(destinationRoot, removeDestEc);
                if (removeDestEc) {
                    pushResult({ "Prepare destination", false, "Failed to clear destination.", "" });
                    m_exportDone = true;
                    m_exportInProgress = false;
                    return;
                }
            }

            // Create the destination directory if it doesn't exist (it should be removed by the previous step if it already exists, but we will create it here just in case it didn't exist before or there was an error removing it)
            // Ensure destination path exists for copy.
            std::error_code createEc;
            std::filesystem::create_directories(destinationRoot, createEc);
            if (createEc) {
                pushResult({ "Prepare destination", false, "Failed to create destination.", "" });
                m_exportDone = true;
                m_exportInProgress = false;
                return;
            }
            pushResult({ "Prepare destination", true, "OK", "" });

            // Copy full staged export output first.
            std::error_code copyEc;
            m_exportCurrentStep = 7;
            std::filesystem::copy(exportRoot, destinationRoot,
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, copyEc);
            if (copyEc) {
                pushResult({ "Copy export output", false, "Failed to copy export output.", "" });
                m_exportDone = true;
                m_exportInProgress = false;
                return;
            }

            // Apply selected-asset filter to exported project payload only.
            if (!selectedAssets.empty()) {
                const std::filesystem::path exportedProjectDir = destinationRoot / projectName;
                if (!std::filesystem::exists(exportedProjectDir) || !std::filesystem::is_directory(exportedProjectDir)) {
                    m_exportDone = true;
                    m_exportInProgress = false;
                    return;
                }

                // Remove unselected files while preserving required settings file.
                std::error_code walkEc;
                std::size_t removedCount = 0;
                std::size_t hiddenRemovedCount = 0;
                for (std::filesystem::recursive_directory_iterator it(exportedProjectDir,
                         std::filesystem::directory_options::skip_permission_denied, walkEc),
                     end; !walkEc && it != end; it.increment(walkEc)) {
                    if (it->is_directory(walkEc)) {
                        if (IsHiddenPath(it->path())) {
                            std::error_code removeHiddenDirEc;
                            std::filesystem::remove_all(it->path(), removeHiddenDirEc);
                            if (!removeHiddenDirEc) {
                                ++hiddenRemovedCount;
                            }
                            it.disable_recursion_pending();
                        }
                        continue;
                    }

                    if (walkEc || !it->is_regular_file(walkEc)) {
                        walkEc.clear();
                        continue;
                    }

                    if (IsHiddenPath(it->path())) {
                        std::error_code removeHiddenFileEc;
                        std::filesystem::remove(it->path(), removeHiddenFileEc);
                        if (!removeHiddenFileEc) {
                            ++hiddenRemovedCount;
                        }
                        continue;
                    }

                    // Map file to project-relative key for set lookup.
                    std::error_code relEc;
                    const std::filesystem::path rel = std::filesystem::relative(it->path(), exportedProjectDir, relEc);
                    if (relEc) {
                        continue;
                    }

                    // Always preserve project settings for runtime boot.
                    const std::string relKey = rel.generic_string();
                    const bool alwaysKeep = (relKey == "ProjectSettings.json");
                    if (!alwaysKeep && selectedAssets.find(relKey) == selectedAssets.end()) {
                        std::error_code removeEc;
                        std::filesystem::remove(it->path(), removeEc);
                        if (!removeEc) {
                            ++removedCount;
                        }
                    }
                }

                // Best-effort prune of now-empty directories.
                std::error_code pruneEc;
                for (std::filesystem::recursive_directory_iterator it(exportedProjectDir,
                         std::filesystem::directory_options::skip_permission_denied, pruneEc),
                     end; !pruneEc && it != end; it.increment(pruneEc)) {
                    if (!it->is_directory(pruneEc)) {
                        continue;
                    }

                    std::error_code emptyEc;
                    if (std::filesystem::is_empty(it->path(), emptyEc) && !emptyEc) {
                        std::error_code removeDirEc;
                        std::filesystem::remove(it->path(), removeDirEc);
                    }
                }

                // If the exported runtime embeds shaders, we can remove copied runtime shader files.
                const std::filesystem::path embeddedShadersMarker = destinationRoot / ".embedded_shaders";
                if (std::filesystem::exists(embeddedShadersMarker)) {
                    const std::filesystem::path runtimeShaderDir = destinationRoot / "assets" / "shaders";
                    if (std::filesystem::exists(runtimeShaderDir) && std::filesystem::is_directory(runtimeShaderDir)) {
                        std::error_code removeRuntimeShadersEc;
                        std::filesystem::remove_all(runtimeShaderDir, removeRuntimeShadersEc);
                    }
                }

                (void)removedCount;
                (void)hiddenRemovedCount;
            }
            {
                std::lock_guard<std::mutex> lock(m_exportMutex);
                m_exportDestination = destinationRoot.string();
            }
            pushResult({ "Copy export output", true, "OK", "" });
            LOG_INFO("Exported project to: " << destinationRoot.string());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to export project: " << e.what());
            pushResult({ "Export exception", false, e.what(), "" });
        }

        m_exportDone = true;
        m_exportCurrentStep = static_cast<int>(m_exportStepNames.size());
        m_exportInProgress = false;
    });
}

// Draws the "Edit" menu with Undo/Redo and Project settings
void EditorFileMenu::RenderEditMenu() {
    if (ImGui::BeginMenu("Edit")) {
        const bool canUndo = m_undoSystem && m_undoSystem->CanUndo();
        const bool canRedo = m_undoSystem && m_undoSystem->CanRedo();

        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
            m_undoSystem->Undo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
            m_undoSystem->Redo();
        }
        ImGui::Separator();
        
        if (ImGui::MenuItem("Project Settings...")) {
            m_showProjectSettings = true;
            m_projectSettingsDirty = false;
        }
        ImGui::EndMenu();
    }

    // Render project settings modal if open
    if (m_showProjectSettings) {
        _renderProjectSettingsModal();
    }
}

// Draws the "View" menu with UI scale controls
void EditorFileMenu::RenderViewMenu(float& uiScale) {
    // Clamp UI scale so users can't shrink or enlarge the UI too much
    uiScale = std::clamp(uiScale, 0.75f, 2.0f);

    // Apply the new scale globally so all ImGui text and widgets follow it
    ImGui::GetIO().FontGlobalScale = uiScale;
    EditorStyle::FontScale = uiScale;

    if (ImGui::BeginMenu("View")) {
        ImGui::Text("UI Scale: %.2f", uiScale);
        ImGui::Separator();
        if (ImGui::MenuItem("Zoom In", "Ctrl++")) { uiScale += 0.10f; }
        if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) { uiScale -= 0.10f; }
        if (ImGui::MenuItem("Reset Scale")) { uiScale = 1.0f; }
        ImGui::EndMenu();
    }
}

// -------------------------------------------------------------------------
// Project Settings
// -------------------------------------------------------------------------

void EditorFileMenu::_renderProjectSettingsModal() {
    if (!Engine::CORE) return;
    
    // Get a reference to the project settings for easy access
    ProjectSettings& settings = Engine::CORE->GetProjectSettings();
    static double lastSavedTime = -100.0;
    static bool showCloseConfirm = false;
    static bool baselineValid = false;
    static nlohmann::json baselineProject;
    static nlohmann::json baselineWindow;
    static nlohmann::json baselinePhysics;
    static nlohmann::json baselineAudio;
    static nlohmann::json baselinePerformance;

    static const nlohmann::json projectDefaults = {
        {"Title", "GrapeEngine Game Project"},
        {"Version", "1.0.0"},
        {"StartupScene", ""}
    };
    static const nlohmann::json windowDefaults = {
        {"Width", 1600},
        {"Height", 900},
        {"Mode", "Fullscreen"},
        {"VSync", true}
    };
    static const nlohmann::json physicsDefaults = {
        {"Gravity", -9.81f},
        {"TimeStep", 0.016f}
    };
    static const nlohmann::json audioDefaults = {
        {"MasterVolume", 1.0f},
        {"MusicVolume", 1.0f},
        {"SFXVolume", 1.0f},
        {"MuteWhenUnfocused", false}
    };
    static const nlohmann::json performanceDefaults = {
        {"MaxFPS", 120}
    };

    if (!baselineValid) {
        baselineProject = {
            {"Title", settings.Title},
            {"Version", settings.Version},
            {"StartupScene", settings.StartupScene}
        };
        baselineWindow = {
            {"Width", settings.WindowSettings.Width},
            {"Height", settings.WindowSettings.Height},
            {"Mode", settings.WindowSettings.Mode},
            {"VSync", settings.WindowSettings.VSync}
        };
        baselinePhysics = {
            {"Gravity", settings.Physics.Gravity},
            {"TimeStep", settings.Physics.TimeStep}
        };
        baselineAudio = {
            {"MasterVolume", settings.MasterVolume},
            {"MusicVolume", settings.MusicVolume},
            {"SFXVolume", settings.SFXVolume},
            {"MuteWhenUnfocused", settings.MuteWhenUnfocused}
        };
        baselinePerformance = {
            {"MaxFPS", settings.MaxFPS}
        };
        baselineValid = true;
    }

    // Helper to mark the settings as saved
    auto markSaved = []() {
        lastSavedTime = ImGui::GetTime();
    };

    // Helper to save project settings to disk
    auto saveSettings = [&]() -> bool {
        std::string projectRoot = Engine::ProjectPaths::GetProjectRoot();
        if (Engine::CORE->SaveProjectSettings(projectRoot)) {
            m_projectSettingsDirty = false;
            markSaved();
            return true;
        }
        return false;
    };

    // Lambda helper: Reloads project settings from disk, discarding any unsaved changes
    // This allows users to undo modifications and restore the last saved state
    auto revertSettings = [&]() {
        std::string projectRoot = Engine::ProjectPaths::GetProjectRoot();
        if (Engine::CORE->LoadProjectSettings(projectRoot)) {
            m_projectSettingsDirty = false;  // Mark as no unsaved changes since we reloaded from disk
        }
    };

    // Helpers to reset specific sections to defaults
    auto resetProjectDefaults = [&]() {
        settings.Title = projectDefaults.value("Title", settings.Title);
        settings.Version = projectDefaults.value("Version", settings.Version);
        settings.StartupScene = projectDefaults.value("StartupScene", settings.StartupScene);
        m_projectSettingsDirty = true;
    };

    // Reset window settings to defaults
    auto resetWindowDefaults = [&]() {
        settings.WindowSettings.Width = windowDefaults.value("Width", settings.WindowSettings.Width);
        settings.WindowSettings.Height = windowDefaults.value("Height", settings.WindowSettings.Height);
        settings.WindowSettings.Mode = windowDefaults.value("Mode", settings.WindowSettings.Mode);
        settings.WindowSettings.Fullscreen = (settings.WindowSettings.Mode == "Fullscreen");
        settings.WindowSettings.VSync = windowDefaults.value("VSync", settings.WindowSettings.VSync);
        m_projectSettingsDirty = true;
    };

    // Reset physics settings to defaults
    auto resetPhysicsDefaults = [&]() {
        settings.Physics.Gravity = physicsDefaults.value("Gravity", settings.Physics.Gravity);
        settings.Physics.TimeStep = physicsDefaults.value("TimeStep", settings.Physics.TimeStep);
        m_projectSettingsDirty = true;
    };

    auto resetAudioDefaults = [&]() {
        settings.MasterVolume = audioDefaults.value("MasterVolume", settings.MasterVolume);
        settings.MusicVolume = audioDefaults.value("MusicVolume", settings.MusicVolume);
        settings.SFXVolume = audioDefaults.value("SFXVolume", settings.SFXVolume);
        settings.MuteWhenUnfocused = audioDefaults.value("MuteWhenUnfocused", settings.MuteWhenUnfocused);
        m_projectSettingsDirty = true;
    };

    auto resetPerformanceDefaults = [&]() {
        settings.MaxFPS = performanceDefaults.value("MaxFPS", settings.MaxFPS);
        m_projectSettingsDirty = true;
    };

    // Render a reset button in the header of each section
    auto renderHeaderResetButton = [&](const char* id, float rightEdge, const char* tooltip, const std::function<void()>& onReset) {
        const float buttonSize = ImGui::GetFrameHeight();
        const float buttonX = rightEdge - buttonSize - ImGui::GetStyle().FramePadding.x;

        // Position the button on the same line as the header
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosX(buttonX);
        ImGui::PushID(id);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);

        // Render the reset icon button
        const char* resetIcon = EditorIcons::Reset;
        if (m_symbolsFont) ImGui::PushFont(m_symbolsFont);
        const bool clicked = ImGui::Button(resetIcon, ImVec2(buttonSize, buttonSize));
        if (m_symbolsFont) ImGui::PopFont();

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
        ImGui::PopID();

        if (clicked) {
            onReset();
        }
    };

    // Render a small reset button for individual rows
    auto renderRowResetButton = [&](const char* id, const char* tooltip) -> bool {
        const float buttonSize = ImGui::GetFrameHeight();
        const float rightEdge = ImGui::GetWindowContentRegionMax().x;
        const float buttonX = rightEdge - buttonSize - ImGui::GetStyle().FramePadding.x;

        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY());
        ImGui::SetCursorPosX(buttonX);
        ImGui::PushID(("Reset_" + std::string(id)).c_str());
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);

        const char* resetIcon = EditorIcons::Reset;
        if (m_symbolsFont) ImGui::PushFont(m_symbolsFont);
        const bool clicked = ImGui::SmallButton(resetIcon);
        if (m_symbolsFont) ImGui::PopFont();

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
        ImGui::PopID();
        return clicked;
    };

    // Lambda helper: Renders an action button on the right side of a property row
    // The 'slot' parameter determines horizontal position (0 = rightmost, increases leftward)
    // Returns true if the button was clicked
    auto renderRowActionButton = [&](const char* id, const char* label, const char* tooltip, int slot,
                                     const ImVec4& textColor) -> bool {
        const float buttonSize = ImGui::GetFrameHeight();
        const float rightEdge = ImGui::GetWindowContentRegionMax().x;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float buttonX = rightEdge - (buttonSize * (slot + 1))
            - (spacing * slot) - ImGui::GetStyle().FramePadding.x;

        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY());
        ImGui::SetCursorPosX(buttonX);
        ImGui::PushID(id);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);

        if (m_symbolsFont) ImGui::PushFont(m_symbolsFont);
        const bool clicked = ImGui::SmallButton(label);
        if (m_symbolsFont) ImGui::PopFont();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
        ImGui::PopID();
        return clicked;
    };

    // Render a small help icon button for individual rows
    auto renderHelpIcon = [&](const char* id, const char* tooltip) {
        const float iconSize = ImGui::GetFrameHeight();
        const float rightEdge = ImGui::GetWindowContentRegionMax().x;
        const float resetButtonSize = ImGui::GetFrameHeight();
        const float iconX = rightEdge - resetButtonSize - ImGui::GetStyle().ItemSpacing.x - iconSize;

        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY());
        ImGui::SetCursorPosX(iconX);
        ImGui::PushID(id);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Muted);

        ImGui::SmallButton("?");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
        ImGui::PopID();
    };

    // Helper to render a collapsible section with a reset button in the header
    auto renderSection = [&](const char* headerId, const char* headerLabel, const std::function<void()>& renderBody,
                             const std::function<void()>& onReset) {
        // Collapsing header with custom styling
        ImGui::SetNextItemAllowOverlap();
        const ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_Framed;
        const ImGuiID headerStorageId = ImGui::GetID(headerId);
        const bool wasOpen = ImGui::GetStateStorage()->GetBool(
            headerStorageId, (headerFlags & ImGuiTreeNodeFlags_DefaultOpen) != 0);
        const float headerRounding = wasOpen ? 0.0f : ImGui::GetStyle().FrameRounding;

        // Render the collapsing header
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, headerRounding);
        if (m_boldFont) ImGui::PushFont(m_boldFont);
        const bool nodeOpen = ImGui::CollapsingHeader(headerLabel, headerFlags);
        if (m_boldFont) ImGui::PopFont();
        ImGui::PopStyleVar();

        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        renderHeaderResetButton(headerId, headerMax.x, "Reset section to defaults", onReset);

        // Render the body within a styled box if the section is open
        if (nodeOpen) {
            // Draw a box around the section body
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float boxPaddingX = 10.0f;
            const float boxPaddingY = 6.0f;
            const float boxRounding = 6.0f;
            const float boxWidth = headerMax.x - headerMin.x;
            const float gap = ImGui::GetStyle().ItemSpacing.y;

            // Calculate box positions
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - gap * 2.f);
            ImVec2 boxMinScreen = ImVec2(headerMin.x, ImGui::GetCursorScreenPos().y);

            // Draw filled rect and border using ImGui channels to avoid interfering with other ImGui elements
            drawList->ChannelsSplit(2);
            drawList->ChannelsSetCurrent(1);

            // Render the section body within the box
            ImGui::BeginGroup();
            ImGui::Dummy(ImVec2(0.0f, boxPaddingY));
            ImGui::Indent(boxPaddingX);
            EditorUI::BeginPropertySection({});
            renderBody();
            EditorUI::EndPropertySection();
            ImGui::Unindent(boxPaddingX);
            ImGui::Dummy(ImVec2(0.0f, boxPaddingY));
            ImGui::EndGroup();

            // Calculate box max position
            ImVec2 boxMaxScreen = ImGui::GetItemRectMax();
            boxMaxScreen.x = boxMinScreen.x + boxWidth;

            // Draw the box
            drawList->ChannelsSetCurrent(0);
            drawList->AddRectFilled(
                boxMinScreen,
                boxMaxScreen,
                ImGui::GetColorU32(EditorStyle::Scale(EditorStyle::FrameBg, 0.85f)),
                boxRounding,
                ImDrawFlags_RoundCornersBottom
            );
            drawList->AddRect(
                boxMinScreen,
                boxMaxScreen,
                ImGui::GetColorU32(EditorStyle::Scale(EditorStyle::Border, 0.85f)),
                boxRounding,
                ImDrawFlags_RoundCornersBottom
            );
            drawList->ChannelsMerge();
            ImGui::SetCursorScreenPos(ImVec2(boxMinScreen.x, boxMaxScreen.y));
            ImGui::Spacing();
            ImGui::Spacing();
        }
    };

    // Validate each setting to ensure they have acceptable values
    // A valid title cannot be empty, width and height must be positive numbers
    // All three must be valid before allowing the user to save
    const bool titleValid = !settings.Title.empty();
    const bool widthValid = settings.WindowSettings.Width > 0;
    const bool heightValid = settings.WindowSettings.Height > 0;
    const bool settingsValid = titleValid && widthValid && heightValid;  // True only if all checks pass

    // Ensure Window Mode has a valid value (if somehow empty, use Fullscreen or Windowed based on flag)
    if (settings.WindowSettings.Mode.empty()) {
        settings.WindowSettings.Mode = settings.WindowSettings.Fullscreen ? "Fullscreen" : "Windowed";
        m_projectSettingsDirty = true;  // Mark as changed since we just auto-corrected
    }
    if (m_symbolsFont) {
        EditorUI::SetSymbolsFont(m_symbolsFont);
    }

    // Check if the specified startup scene file actually exists on disk
    // Shows a warning icon if the path points to a file that doesn't exist
    bool startupSceneMissing = false;
    if (!settings.StartupScene.empty()) {
        std::filesystem::path scenePath = Engine::ProjectPaths::GetProjectRoot();
        scenePath /= settings.StartupScene;  // Construct full path from project root + relative path
        startupSceneMissing = !std::filesystem::exists(scenePath);  // True if file not found
    }

    // Open the Project Settings modal window with custom styling and sizing
    ImGui::SetNextWindowSize(ImVec2(640, 520), ImGuiCond_FirstUseEver);  // 640x520 on first appearance
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));  // Add padding inside window
    if (ImGui::Begin("Project Settings", &m_showProjectSettings, ImGuiWindowFlags_NoCollapse)) {
        ImGui::PopStyleVar();

        // Calculate space needed for warning message (if there are validation errors)
        const float warningHeight = (!settingsValid || startupSceneMissing)
            ? (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y)
            : 0.0f;

        // Calculate total footer height = buttons + spacing + padding + optional warning
        const float footerHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y * 1.5f
            + ImGui::GetStyle().WindowPadding.y + warningHeight;
        ImGui::BeginChild("ProjectSettingsBody", ImVec2(0.0f, -footerHeight), false);
        nlohmann::json projectData = {
            {"Title", settings.Title},
            {"Version", settings.Version},
            {"StartupScene", settings.StartupScene}
        };
        const bool projectModified = (projectData != baselineProject);
        const std::string projectHeaderLabel = std::string("Project Information") +
            (projectModified ? " *" : "");

        // Render Project Information section
        renderSection("Project Information", projectHeaderLabel.c_str(), [&]() {
            EditorUI::RegisterDefaultDataScope(projectData, projectDefaults);
            if (!titleValid) {
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.2f, 0.2f, 0.5f));
            }
            EditorUI::RenderTextProperty("Title", projectData, "Title");
            if (!titleValid) {
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted("Title cannot be empty.");
                    ImGui::EndTooltip();
                }
            }
            EditorUI::RenderTextProperty("Version", projectData, "Version");
            std::string startupScene = projectData.value("StartupScene", std::string());
            char sceneBuf[512];
            strncpy_s(sceneBuf, startupScene.c_str(), sizeof(sceneBuf) - 1);

            ImGui::Text("Startup Scene");
            ImGui::SameLine();
            const float axisLabelWidth = ImGui::CalcTextSize("W").x;
            const float fieldStartX = EditorUI::GetContentStartX() + axisLabelWidth + 6.0f;
            ImGui::SetCursorPosX(fieldStartX);
            ImGui::SetNextItemWidth(280.0f);
            if (ImGui::InputText("##StartupScene", sceneBuf, sizeof(sceneBuf))) {
                projectData["StartupScene"] = std::string(sceneBuf);
            }

            const std::string defaultScene = projectDefaults.value("StartupScene", "");
            if (projectData.value("StartupScene", std::string()) != defaultScene) {
                if (renderRowResetButton("StartupScene", "Reset to default")) {
                    projectData["StartupScene"] = defaultScene;
                }
            }

            if (renderRowActionButton("StartupSceneCopy", EditorIcons::Copy, "Copy path", 1, EditorStyle::Text)) {
                ImGui::SetClipboardText(projectData.value("StartupScene", "").c_str());
            }

#ifdef _WIN32
            if (renderRowActionButton("StartupSceneBrowse", EditorIcons::Browse, "Browse...", 2, EditorStyle::Text)) {
                OPENFILENAMEA ofn = {};
                char szFile[260] = {};
                ofn.lStructSize = sizeof(ofn);
                auto* context = Engine::CORE->GetPlatformContext();
                auto* mainWindow = context ? context->GetMainWindow() : nullptr;
                if (mainWindow) {
                    ofn.hwndOwner = glfwGetWin32Window(reinterpret_cast<GLFWwindow*>(mainWindow->GetNativeHandle()));
                }
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = sizeof(szFile);
                ofn.lpstrFilter = "Scene Files (*.scn;*.scene)\0*.scn;*.scene\0All Files (*.*)\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.lpstrFileTitle = nullptr;
                ofn.nMaxFileTitle = 0;
                ofn.lpstrInitialDir = Engine::ProjectPaths::GetProjectRoot().c_str();
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

                if (GetOpenFileNameA(&ofn)) {
                    std::filesystem::path selected = std::filesystem::path(szFile);
                    std::filesystem::path root = std::filesystem::path(Engine::ProjectPaths::GetProjectRoot());
                    std::error_code ec;
                    std::filesystem::path rel = std::filesystem::relative(selected, root, ec);
                    if (!ec && !rel.empty() && rel.native().find(L"..") == std::wstring::npos) {
                        projectData["StartupScene"] = rel.generic_string();
                    } else {
                        projectData["StartupScene"] = selected.generic_string();
                    }
                }
            }
#endif

            bool startupSceneMissing = false;
            if (!projectData.value("StartupScene", std::string()).empty()) {
                std::filesystem::path scenePath = Engine::ProjectPaths::GetProjectRoot();
                scenePath /= projectData.value("StartupScene", std::string());
                startupSceneMissing = !std::filesystem::exists(scenePath);
            }
            if (startupSceneMissing) {
                renderRowActionButton("StartupSceneMissing", "!", "Startup scene not found", 3, EditorStyle::WarningText);
            }

            EditorUI::ClearDefaultDataScope();
        }, resetProjectDefaults);

        // Apply changes from Project Information section
        if (settings.Title != projectData.value("Title", settings.Title)) {
            settings.Title = projectData["Title"].get<std::string>();
            m_projectSettingsDirty = true;
        }
        if (settings.Version != projectData.value("Version", settings.Version)) {
            settings.Version = projectData["Version"].get<std::string>();
            m_projectSettingsDirty = true;
        }
        if (settings.StartupScene != projectData.value("StartupScene", settings.StartupScene)) {
            settings.StartupScene = projectData["StartupScene"].get<std::string>();
            m_projectSettingsDirty = true;
        }

        // Setup Window Settings section
        nlohmann::json windowData = {
            {"Width", settings.WindowSettings.Width},
            {"Height", settings.WindowSettings.Height},
            {"Mode", settings.WindowSettings.Mode},
            {"VSync", settings.WindowSettings.VSync}
        };
        const bool windowModified = (windowData != baselineWindow);
        const std::string windowHeaderLabel = std::string("Window Settings") +
            (windowModified ? " *" : "");

        // Render Window Settings section
        renderSection("Window Settings", windowHeaderLabel.c_str(), [&]() {
            EditorUI::RegisterDefaultDataScope(windowData, windowDefaults);

            if (!widthValid) {
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.2f, 0.2f, 0.5f));
            }
            EditorUI::RenderIntProperty("Width", windowData, "Width");
            if (!widthValid) {
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted("Width must be greater than 0.");
                    ImGui::EndTooltip();
                }
            }

            if (!heightValid) {
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.2f, 0.2f, 0.5f));
            }
            EditorUI::RenderIntProperty("Height", windowData, "Height");
            if (!heightValid) {
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted("Height must be greater than 0.");
                    ImGui::EndTooltip();
                }
            }

            // Render the Window Mode label and dropdown menu
            ImGui::Text("Mode");
            ImGui::SameLine();  // Put the next control on the same line (to the right)
            const float axisLabelWidth = ImGui::CalcTextSize("W").x;  // Get label width for alignment
            const float fieldStartX = EditorUI::GetContentStartX() + axisLabelWidth + 6.0f;  // Position input field
            ImGui::SetCursorPosX(fieldStartX);
            ImGui::SetNextItemWidth(180.0f);  // Set dropdown width

            // Window Mode options: Windowed (resizable), Fullscreen (exclusive), Borderless (covers screen)
            const char* modes[] = { "Windowed", "Fullscreen", "Borderless" };
            std::string modeValue = windowData.value("Mode", std::string("Windowed"));
            // Find which option is currently selected (convert string to index)
            int modeIndex = 0;  // Default to Windowed
            if (modeValue == "Fullscreen") {
                modeIndex = 1;
            } else if (modeValue == "Borderless") {
                modeIndex = 2;
            }

            // Render the combo box dropdown and update data if selection changed
            if (ImGui::Combo("##WindowMode", &modeIndex, modes, 3)) {
                // Convert selected index back to string and store it
                if (modeIndex == 0) {
                    windowData["Mode"] = "Windowed";
                } else if (modeIndex == 1) {
                    windowData["Mode"] = "Fullscreen";
                } else {
                    windowData["Mode"] = "Borderless";
                }
            }

            // Reset button for Window Mode
            const std::string defaultMode = windowDefaults.value("Mode", "Fullscreen");
            if (windowData.value("Mode", std::string()) != defaultMode) {
                if (renderRowResetButton("Mode", "Reset to default")) {
                    windowData["Mode"] = defaultMode;
                }
            }
            renderHelpIcon("ModeHelp", "Fullscreen is exclusive. Borderless covers the monitor.");

            EditorUI::RenderCheckboxProperty("VSync", windowData, "VSync");
            renderHelpIcon("VSyncHelp", "Disables tearing but can add latency.");
            EditorUI::ClearDefaultDataScope();
        }, resetWindowDefaults);

        // Apply changes from Window Settings section
        if (settings.WindowSettings.Width != windowData.value("Width", settings.WindowSettings.Width)) {
            settings.WindowSettings.Width = windowData["Width"].get<int>();
            m_projectSettingsDirty = true;
        }
        if (settings.WindowSettings.Height != windowData.value("Height", settings.WindowSettings.Height)) {
            settings.WindowSettings.Height = windowData["Height"].get<int>();
            m_projectSettingsDirty = true;
        }
        if (settings.WindowSettings.Mode != windowData.value("Mode", settings.WindowSettings.Mode)) {
            settings.WindowSettings.Mode = windowData["Mode"].get<std::string>();
            settings.WindowSettings.Fullscreen = (settings.WindowSettings.Mode == "Fullscreen");
            m_projectSettingsDirty = true;
        }
        if (settings.WindowSettings.VSync != windowData.value("VSync", settings.WindowSettings.VSync)) {
            settings.WindowSettings.VSync = windowData["VSync"].get<bool>();
            m_projectSettingsDirty = true;
        }

        nlohmann::json physicsData = {
            {"Gravity", settings.Physics.Gravity},
            {"TimeStep", settings.Physics.TimeStep}
        };
        const bool physicsModified = (physicsData != baselinePhysics);
        const std::string physicsHeaderLabel = std::string("Physics Settings") +
            (physicsModified ? " *" : "");

        renderSection("Physics Settings", physicsHeaderLabel.c_str(), [&]() {
            EditorUI::RegisterDefaultDataScope(physicsData, physicsDefaults);
            const float axisLabelWidth = ImGui::CalcTextSize("W").x;
            const float fieldStartX = EditorUI::GetContentStartX() + axisLabelWidth + 6.0f;

            ImGui::Text("Gravity");
            ImGui::SameLine();
            ImGui::SetCursorPosX(fieldStartX);
            ImGui::SetNextItemWidth(120.0f);
            {
                float value = physicsData.value("Gravity", -9.81f);
                if (ImGui::InputFloat("##Gravity", &value, 0.0f, 0.0f, "%.9f")) {
                    physicsData["Gravity"] = value;
                }
            }
            const float defaultGravity = physicsDefaults.value("Gravity", -9.81f);
            if (physicsData.value("Gravity", defaultGravity) != defaultGravity) {
                if (renderRowResetButton("Gravity", "Reset to default")) {
                    physicsData["Gravity"] = defaultGravity;
                }
            }

            ImGui::Text("Time Step");
            ImGui::SameLine();
            ImGui::SetCursorPosX(fieldStartX);
            ImGui::SetNextItemWidth(120.0f);
            {
                float value = physicsData.value("TimeStep", 0.016f);
                if (ImGui::InputFloat("##TimeStep", &value, 0.0f, 0.0f, "%.9f")) {
                    physicsData["TimeStep"] = value;
                }
            }
            const float defaultTimeStep = physicsDefaults.value("TimeStep", 0.016f);
            if (physicsData.value("TimeStep", defaultTimeStep) != defaultTimeStep) {
                if (renderRowResetButton("TimeStep", "Reset to default")) {
                    physicsData["TimeStep"] = defaultTimeStep;
                }
            }
            EditorUI::ClearDefaultDataScope();
        }, resetPhysicsDefaults);

        if (settings.Physics.Gravity != physicsData.value("Gravity", settings.Physics.Gravity)) {
            settings.Physics.Gravity = physicsData["Gravity"].get<float>();
            m_projectSettingsDirty = true;
        }
        if (settings.Physics.TimeStep != physicsData.value("TimeStep", settings.Physics.TimeStep)) {
            settings.Physics.TimeStep = physicsData["TimeStep"].get<float>();
            m_projectSettingsDirty = true;
        }

        nlohmann::json audioData = {
            {"MasterVolume", settings.MasterVolume},
            {"MusicVolume", settings.MusicVolume},
            {"SFXVolume", settings.SFXVolume},
            {"MuteWhenUnfocused", settings.MuteWhenUnfocused}
        };
        const bool audioModified = (audioData != baselineAudio);
        const std::string audioHeaderLabel = std::string("Audio Settings") +
            (audioModified ? " *" : "");

        renderSection("Audio Settings", audioHeaderLabel.c_str(), [&]() {
            EditorUI::RegisterDefaultDataScope(audioData, audioDefaults);

            float masterVolume = audioData.value("MasterVolume", settings.MasterVolume);
            if (ImGui::SliderFloat("Master Volume", &masterVolume, 0.0f, 1.0f, "%.2f")) {
                audioData["MasterVolume"] = masterVolume;
            }
            const float defaultMaster = audioDefaults.value("MasterVolume", 1.0f);
            if (audioData.value("MasterVolume", defaultMaster) != defaultMaster) {
                if (renderRowResetButton("MasterVolume", "Reset to default")) {
                    audioData["MasterVolume"] = defaultMaster;
                }
            }

            float musicVolume = audioData.value("MusicVolume", settings.MusicVolume);
            if (ImGui::SliderFloat("Music Volume", &musicVolume, 0.0f, 1.0f, "%.2f")) {
                audioData["MusicVolume"] = musicVolume;
            }
            const float defaultMusic = audioDefaults.value("MusicVolume", 1.0f);
            if (audioData.value("MusicVolume", defaultMusic) != defaultMusic) {
                if (renderRowResetButton("MusicVolume", "Reset to default")) {
                    audioData["MusicVolume"] = defaultMusic;
                }
            }

            float sfxVolume = audioData.value("SFXVolume", settings.SFXVolume);
            if (ImGui::SliderFloat("SFX Volume", &sfxVolume, 0.0f, 1.0f, "%.2f")) {
                audioData["SFXVolume"] = sfxVolume;
            }
            const float defaultSfx = audioDefaults.value("SFXVolume", 1.0f);
            if (audioData.value("SFXVolume", defaultSfx) != defaultSfx) {
                if (renderRowResetButton("SFXVolume", "Reset to default")) {
                    audioData["SFXVolume"] = defaultSfx;
                }
            }

            EditorUI::RenderCheckboxProperty("Mute when unfocused", audioData, "MuteWhenUnfocused");
            EditorUI::ClearDefaultDataScope();
        }, resetAudioDefaults);

        if (settings.MasterVolume != audioData.value("MasterVolume", settings.MasterVolume)) {
            settings.MasterVolume = audioData["MasterVolume"].get<float>();
            m_projectSettingsDirty = true;
        }
        if (settings.MusicVolume != audioData.value("MusicVolume", settings.MusicVolume)) {
            settings.MusicVolume = audioData["MusicVolume"].get<float>();
            m_projectSettingsDirty = true;
        }
        if (settings.SFXVolume != audioData.value("SFXVolume", settings.SFXVolume)) {
            settings.SFXVolume = audioData["SFXVolume"].get<float>();
            m_projectSettingsDirty = true;
        }
        if (settings.MuteWhenUnfocused != audioData.value("MuteWhenUnfocused", settings.MuteWhenUnfocused)) {
            settings.MuteWhenUnfocused = audioData["MuteWhenUnfocused"].get<bool>();
            m_projectSettingsDirty = true;
        }

        nlohmann::json performanceData = {
            {"MaxFPS", settings.MaxFPS}
        };
        const bool performanceModified = (performanceData != baselinePerformance);
        const std::string performanceHeaderLabel = std::string("Performance Settings") +
            (performanceModified ? " *" : "");

        renderSection("Performance Settings", performanceHeaderLabel.c_str(), [&]() {
            EditorUI::RegisterDefaultDataScope(performanceData, performanceDefaults);

            if (!settings.WindowSettings.VSync) {
                EditorUI::RenderIntProperty("Max FPS", performanceData, "MaxFPS");
                const int defaultMaxFps = performanceDefaults.value("MaxFPS", 120);
                if (performanceData.value("MaxFPS", defaultMaxFps) != defaultMaxFps) {
                    if (renderRowResetButton("MaxFPS", "Reset to default")) {
                        performanceData["MaxFPS"] = defaultMaxFps;
                    }
                }
            } else {
                int maxFpsValue = performanceData.value("MaxFPS", settings.MaxFPS);
                ImGui::BeginDisabled();
                ImGui::InputInt("Max FPS", &maxFpsValue);
                ImGui::EndDisabled();
            }

            EditorUI::ClearDefaultDataScope();
        }, resetPerformanceDefaults);

        const int maxFpsValue = std::max(1, performanceData.value("MaxFPS", settings.MaxFPS));
        if (settings.MaxFPS != maxFpsValue) {
            settings.MaxFPS = maxFpsValue;
            m_projectSettingsDirty = true;
        }
        
        ImGui::EndChild();

        ImGui::Separator();
        if (!settingsValid || startupSceneMissing) {
            const char* message = startupSceneMissing
                ? "Startup scene path is invalid."
                : "Fix invalid settings to save.";
            ImGui::TextColored(EditorStyle::WarningText, "%s", message);
        }
        
        // Save/Cancel buttons
        if (!settingsValid) {
            ImGui::BeginDisabled();
        }
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SuccessButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SuccessButtonHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SuccessButtonActive);
        if (ImGui::Button(m_projectSettingsDirty ? "Save*" : "Save", ImVec2(120, 0))) {
            if (saveSettings()) {
                m_showProjectSettings = false;
            }
        }
        ImGui::PopStyleColor(3);

        // Render Apply button
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::PrimaryButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::PrimaryButtonHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::PrimaryButtonActive);
        if (ImGui::Button("Apply", ImVec2(120, 0))) {
            saveSettings();
        }
        ImGui::PopStyleColor(3);
        if (!settingsValid) {
            ImGui::EndDisabled();
        }

        // Render Revert button
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::WarningButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::Scale(EditorStyle::WarningButton, 1.1f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::Scale(EditorStyle::WarningButton, 0.9f));
        if (ImGui::Button("Revert", ImVec2(120, 0))) {
            revertSettings();
        }
        ImGui::PopStyleColor(3);

        // Render Close button
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            if (m_projectSettingsDirty) {
                showCloseConfirm = true;
                ImGui::OpenPopup("Unsaved Project Settings");
            } else {
                m_showProjectSettings = false;
            }
        }
        ImGui::PopStyleColor(3);

        // Show "Saved" confirmation text for 2 seconds after saving
        if (ImGui::GetTime() - lastSavedTime < 2.0) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.3f, 1.0f), "Saved");
        }

        // Unsaved changes confirmation popup
        if (showCloseConfirm && ImGui::BeginPopupModal("Unsaved Project Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("You have unsaved changes. Save before closing?");
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SuccessButton);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SuccessButtonHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SuccessButtonActive);
            if (ImGui::Button("Save", ImVec2(120, 0))) {
                if (settingsValid) {
                    saveSettings();
                    m_showProjectSettings = false;
                    showCloseConfirm = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::DangerButton);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::DangerButtonHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::DangerButtonActive);
            if (ImGui::Button("Discard", ImVec2(120, 0))) {
                m_showProjectSettings = false;
                showCloseConfirm = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showCloseConfirm = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
            ImGui::EndPopup();
        }
    }
    else {
        ImGui::PopStyleVar();
    }
    ImGui::End();

    if (!m_showProjectSettings) {
        baselineValid = false;
    }
}

// Displays a modal window showing the progress and results of the project export operation
// Shows each export step status (working, completed, or failed) and any error messages
void EditorFileMenu::_renderExportSummaryPopup() {
    // Trigger popup to open if the export process just started or completed
    if (m_openExportSummary) {
        ImGui::OpenPopup("Export Summary");  // Show the modal window
        m_openExportSummary = false;  // Clear flag to prevent reopening next frame
    }

    // Set initial window size and min/max constraints for the export summary popup
    ImGui::SetNextWindowSize(ImVec2(520.0f, 360.0f), ImGuiCond_Appearing);  // Default size
    ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 240.0f), ImVec2(960.0f, 720.0f));  // Allow resizing within bounds
    if (ImGui::BeginPopupModal("Export Summary")) {
        // Load export state from thread-safe member variables using atomic loads and mutex locks
        // This prevents race conditions when the background export thread modifies these values
        const bool exportInProgress = m_exportInProgress.load();  // Atomic bool: is export still running?
        const int exportCurrentStep = m_exportCurrentStep.load();  // Atomic int: which step is active?
        std::string exportDestination;  // Where the export was saved to
        std::vector<ExportStepResult> exportResults;  // Results of each export step
        std::vector<std::string> exportSteps;  // Names of all export steps
        {
            // Lock the mutex to safely read shared state from the export thread
            std::lock_guard<std::mutex> lock(m_exportMutex);
            exportDestination = m_exportDestination;
            exportResults = m_exportResults;
            exportSteps = m_exportStepNames;
        }  // Mutex automatically unlocks when lock_guard goes out of scope

        if (!exportDestination.empty()) {
            ImGui::Text("Destination:");
            ImGui::TextWrapped("%s", exportDestination.c_str());
            ImGui::Separator();
        }
        if (exportInProgress) {
            ImGui::Text("Exporting... This may take a while.");
            ImGui::Separator();
        }

        // Calculate space reserved at bottom for the Close button
        const float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        // Create scrollable area for results that takes up remaining vertical space
        ImGui::BeginChild("ExportSummaryResults", ImVec2(0.0f, -footerHeight), true);
        // Create a map for quick lookup of step results by name (optimization for large export lists)
        std::unordered_map<std::string, ExportStepResult> resultsByName;
        resultsByName.reserve(exportResults.size());  // Pre-allocate space for efficiency
        // Populate the map: use step name as key for O(1) lookup
        for (const auto& result : exportResults) {
            resultsByName[result.Name] = result;
        }

        // Display each step in sequence, showing its status and any messages
        for (size_t i = 0; i < exportSteps.size(); ++i) {
            const auto& name = exportSteps[i];
            auto it = resultsByName.find(name);
            if (it != resultsByName.end()) {
                const auto& result = it->second;
                const ImVec4 color = result.Success ? ImVec4(0.2f, 0.8f, 0.3f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
                ImGui::TextColored(color, "%s", result.Name.c_str());
                if (!result.Message.empty()) {
                    ImGui::Indent();
                    ImGui::TextWrapped("%s", result.Message.c_str());
                    ImGui::Unindent();
                }
                if (!result.Success && !result.Output.empty()) {
                    ImGui::Indent();
                    ImGui::Text("Details:");
                    ImGui::PushID(result.Name.c_str());
                    ImGui::BeginChild("##ExportDetails", ImVec2(-1.0f, 140.0f), true);
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextUnformatted(result.Output.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndChild();
                    ImGui::PopID();
                    ImGui::Unindent();
                }
            } else if (exportInProgress) {
                if (static_cast<int>(i) == exportCurrentStep) {
                    ImGui::TextColored(EditorStyle::PrimaryButton, "%s", name.c_str());
                    ImGui::Indent();
                    ImGui::TextColored(EditorStyle::PrimaryButton, "Working");
                    ImGui::Unindent();
                } else if (static_cast<int>(i) > exportCurrentStep) {
                    ImGui::TextColored(EditorStyle::Text, "%s", name.c_str());
                    ImGui::Indent();
                    ImGui::TextColored(EditorStyle::Text, "Waiting");
                    ImGui::Unindent();
                }
            } else {
                ImGui::TextDisabled("%s", name.c_str());
                ImGui::Indent();
                ImGui::TextDisabled("Not run");
                ImGui::Unindent();
            }
            ImGui::Separator();
        }
        ImGui::EndChild();

        if (exportInProgress) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        if (exportInProgress) {
            ImGui::EndDisabled();
        }
        ImGui::EndPopup();
    }
}

void EditorFileMenu::_finalizeExportIfDone() {
    if (!m_exportDone.load()) {
        return;
    }
    if (m_exportThread.joinable()) {
        m_exportThread.join();
    }
    m_exportDone = false;
    m_openExportSummary = true;
}

#ifdef _WIN32
std::string EditorFileMenu::_pickExportFolder() {
    BROWSEINFOA bi = {};
    bi.lpszTitle = "Select Export Destination";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        char path[MAX_PATH] = {};
        if (SHGetPathFromIDListA(pidl, path)) {
            CoTaskMemFree(pidl);
            return std::string(path);
        }
        CoTaskMemFree(pidl);
    }
    return {};
}
#endif

// Creates a brand new scene and makes it the active one in the SceneManager
// Does not show any dialog, just starts from a fresh "New Scene"
void EditorFileMenu::CreateNewScene() {
    // If we have no SceneManager bound we cannot do anything
    if (!m_sceneManager) return;

    const size_t activeIdx = m_sceneManager->GetActiveIndex();
    if (activeIdx != static_cast<size_t>(-1)) {
        Scenes::Scene* activeScene = m_sceneManager->GetActive();
        if (activeScene) {
            activeScene->GetWorld().DestroyAll();
            activeScene->GetLayers().ResetToDefaults();
            activeScene->SetName("New Scene");
            activeScene->SetPath("");

            if (m_hierarchyPanel) {
                m_hierarchyPanel->ClearUIState();
                m_hierarchyPanel->RebuildEntityOrder();
            }

            m_currentScenePath.clear();
            m_hasUnsavedChanges = false;
            LOG_INFO("Reset active scene to new scene");
            return;
        }
    }

    // Allocate a new Scene on the heap using a unique_ptr for safety
    auto newScene = std::make_unique<Scenes::Scene>();

    // Give the scene a default name so it is not empty in any UI
    newScene->SetName("New Scene");

    // Hand ownership of the raw pointer to the SceneManager
    // AddScene returns an index we can use to refer to this scene later
    size_t idx = m_sceneManager->AddScene(newScene.release());

    // Switch the editor to use this new scene as the active one
    m_sceneManager->SetActive(idx);
    m_currentScenePath.clear();
    m_hasUnsavedChanges = false;
    LOG_INFO("Created new scene");
}

// Opens a Windows file browser dialog for the user to select a scene file to load
// Passes the selected file path to _openScene() for actual loading
void EditorFileMenu::OpenSceneDialog() {
#ifdef _WIN32
    // Safety check: if no SceneManager, we cannot load any scene
    if (!m_sceneManager) return;

    // OPENFILENAMEA struct: Configures the Windows file open dialog with options and filters
    OPENFILENAMEA ofn = {};  // Initialize all fields to zero
    char szFile[260] = {};  // Buffer to hold the file path selected by user (max 260 chars)

    // Configure the file dialog with required fields and options
    ofn.lStructSize = sizeof(ofn);  // Required: size of this struct
    // Get the main editor window handle so file dialog stays on top and is owned by editor
    auto* context = Engine::CORE->GetPlatformContext();
    auto* mainWindow = context ? context->GetMainWindow() : nullptr;
    if (!mainWindow)
        return;  // Cannot proceed without a window handle

    // Set the dialog owner to the editor window (makes it modal with respect to that window)
    ofn.hwndOwner = glfwGetWin32Window(reinterpret_cast<GLFWwindow*>(mainWindow->GetNativeHandle()));
    ofn.lpstrFile = szFile;  // Buffer for the selected file path
    ofn.nMaxFile = sizeof(szFile);  // Max size of szFile buffer
    // Filter string: shows scene file extensions, plus option for all files
    ofn.lpstrFilter = "Scene Files (*.scn;*.scene)\0*.scn;*.scene\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;  // Start with first filter (Scene Files)
    ofn.lpstrFileTitle = nullptr;  // Don't need just the filename, full path is fine
    ofn.nMaxFileTitle = 0;
    // Start browsing in the project root directory (for convenience)
    ofn.lpstrInitialDir = Engine::ProjectPaths::GetProjectRoot().c_str();

    // Dialog behavior flags:
    // OFN_PATHMUSTEXIST: Folder path must exist (greyed out non-existent paths)
    // OFN_FILEMUSTEXIST: File must exist (prevents creating new files)
    // OFN_NOCHANGEDIR: Don't change the current working directory
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    // Display the file open dialog and wait for user input
    // Returns non-zero if user selected a file and clicked OK, zero if cancelled
    if (GetOpenFileNameA(&ofn)) {
        // User selected a file - szFile now contains the full path to it
        _openScene(szFile);  // Load the selected scene file
    }
    // If cancelled, GetOpenFileNameA returns 0 and we do nothing
#endif
}

void EditorFileMenu::OpenSceneFromPath(const std::string& path) {
    if (path.empty()) {
        return;
    }
    _openScene(path);
}

// Opens a Windows file save dialog for the user to choose a filename and location for saving
// Then saves the active scene to that path
void EditorFileMenu::SaveSceneAsDialog() {
#ifdef _WIN32
    // Safety check: cannot save scene without SceneManager
    if (!m_sceneManager) return;

    // Configure file save dialog (similar to Open dialog but with save-specific options)
    OPENFILENAMEA ofn = {};  // Initialize struct
    char szFile[260] = {};  // Buffer to hold the filename user enters

    ofn.lStructSize = sizeof(ofn);  // Required struct size field
    // Get editor window handle for dialog ownership
    auto* context2 = Engine::CORE->GetPlatformContext();
    auto* mainWindow2 = context2 ? context2->GetMainWindow() : nullptr;
    if (!mainWindow2)
        return;  // Need valid window
        
    // Set dialog owner to editor window
    ofn.hwndOwner = glfwGetWin32Window(reinterpret_cast<GLFWwindow*>(mainWindow2->GetNativeHandle()));
    ofn.lpstrFile = szFile;  // Buffer for filename/path
    ofn.nMaxFile = sizeof(szFile);
    // File type filters: show scene files first, then all files
    ofn.lpstrFilter = "Scene Files (*.scn;*.scene)\0*.scn;*.scene\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;  // Start with Scene Files filter
    ofn.lpstrFileTitle = nullptr;  // Don't need just filename
    ofn.nMaxFileTitle = 0;
    // Start browsing in project root for convenience
    ofn.lpstrInitialDir = Engine::ProjectPaths::GetProjectRoot().c_str();
    // Default file extension if user doesn't type one
    ofn.lpstrDefExt = "scn";
    // Dialog behavior flags:
    // OFN_PATHMUSTEXIST: Folder must exist
    // OFN_OVERWRITEPROMPT: Warn if file already exists before overwriting
    // OFN_NOCHANGEDIR: Don't change current working directory
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    // Display the file save dialog and wait for user input
    // Returns non-zero if user entered filename and clicked Save, zero if cancelled
    if (GetSaveFileNameA(&ofn)) {
        // User confirmed a filename - copy the path from dialog buffer
        std::string savePath = szFile;  // szFile contains the full path user selected/typed

        // Ensure file has a valid scene extension (.scn or .scene)
        // If user didn't type an extension, add .scn as default
        if (savePath.find(".scn") == std::string::npos && savePath.find(".scene") == std::string::npos) {
            savePath += ".scn";  // Add default extension
        }

        // Write the scene to the chosen file path
        _saveSceneToFile(savePath);
        m_currentScenePath = savePath;  // Remember this path for future saves (Ctrl+S)
        m_hasUnsavedChanges = false;  // File is now up-to-date with editor state
    }
    // If cancelled, GetSaveFileNameA returns 0 and we do nothing
#endif
}

// Saves the active scene to its current file path (if known), or opens Save As dialog
void EditorFileMenu::SaveScene() {
#ifdef _WIN32
    // Safety check: need a SceneManager to save
    if (!m_sceneManager) return;
    // If we don't know where to save, ask user to choose a location first
    if (m_currentScenePath.empty()) { 
        SaveSceneAsDialog();  // Open Save As dialog
        return;  // Exit since SaveAsDialog handles the actual save
    }
    // We have a known path - save to that file
    _saveSceneToFile(m_currentScenePath);
    m_hasUnsavedChanges = false;  // File is now synchronized with editor state
#endif
}

// -------------------------------------------------------------------------
// Keyboard Shortcuts
// -------------------------------------------------------------------------

// Processes keyboard shortcuts for common editor operations (save, open, zoom, etc.)
// Called every frame to check if user pressed Ctrl+S, Ctrl+O, Ctrl+N, etc.
void EditorFileMenu::HandleShortcuts(float& uiScale) {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        uiScale = std::clamp(uiScale, 0.75f, 2.0f);
        EditorStyle::FontScale = uiScale;
        return;
    }

    // Check if Control key (either left or right) is currently held down
    bool ctrlDown = Input::IsKeyDown(KEY_LEFT_CONTROL) || Input::IsKeyDown(KEY_RIGHT_CONTROL);
    // Check if Shift key (either left or right) is currently held down
    bool shiftDown = Input::IsKeyDown(KEY_LEFT_SHIFT) || Input::IsKeyDown(KEY_RIGHT_SHIFT);

    // Process Ctrl+key shortcuts
    if (ctrlDown) {
        // Ctrl+Plus: Zoom in (increase UI scale by 10%)
        if (Input::IsKeyPressed(KEY_EQUAL)) uiScale += 0.10f;
        // Ctrl+Minus: Zoom out (decrease UI scale by 10%)
        if (Input::IsKeyPressed(KEY_MINUS)) uiScale -= 0.10f;
        // Ctrl+N: Create a new blank scene
        if (Input::IsKeyPressed(KEY_N)) CreateNewScene();
        // Ctrl+O: Open the file browser to load a scene
        if (Input::IsKeyPressed(KEY_O)) OpenSceneDialog();
        
        // Check conditions that must be true before allowing the save shortcuts to work:
        // 1. Must have an active scene loaded (not null)
        // 2. Must be in edit mode (not playing the game)
        // 3. Must have unsaved changes (for Ctrl+S, but not Ctrl+Shift+S)
        bool hasActiveScene = false;
        if (m_sceneManager) {
            size_t idx = m_sceneManager->GetActiveIndex();
            hasActiveScene = (idx != static_cast<size_t>(-1));  // -1 means no scene is active
        }

        // Check if editor is in Edit mode (true = editing, false = playing/simulating)
        bool isInEditMode = true;
        if (m_getEditorState) {
            isInEditMode = (m_getEditorState() == EditorState::Edit);
        }

        // Ctrl+S: Save scene (or Ctrl+Shift+S: Save As)
        if (Input::IsKeyPressed(KEY_S) && hasActiveScene && isInEditMode) {
            if (shiftDown) {
                SaveSceneAsDialog();  // Ctrl+Shift+S: Open "Save As" dialog
            }
            else if (m_hasUnsavedChanges) {
                SaveScene();  // Ctrl+S: Save to current file (if there are changes)
            }
        }
    }

    // Prevent UI scale from becoming too small (<0.75x) or too large (>2.0x)
    // This keeps the interface readable and prevents visual glitches
    uiScale = std::clamp(uiScale, 0.75f, 2.0f);
    // Apply the scale to global UI font size so all text scales together
    EditorStyle::FontScale = uiScale;
}

// -------------------------------------------------------------------------
// Internal Helpers
// -------------------------------------------------------------------------

// Loads a scene file from disk and makes it the active scene in the editor
// Handles reloading existing active scenes and creating new scene slots as needed
void EditorFileMenu::_openScene(const std::string& path) {
    // Safety check: if SceneManager is null, we cannot load anything
    if (!m_sceneManager) return;

    // Get the index of the currently active scene (or -1 if none is active)
    size_t activeIdx = m_sceneManager->GetActiveIndex();
    const bool hasActive = (activeIdx != static_cast<size_t>(-1));  // True if a scene is loaded

    // Special case: If reloading the SAME scene that's already open, reload it in place
    // This avoids creating duplicate scene entities and maintains the world state
    if (hasActive && (m_currentScenePath == path)) {
        std::vector<uint32_t> entityOrder;  // Order in which entities appear in the scene hierarchy
        // Reload (reload existing scene slot with new file data)
        if (m_sceneManager->RestartScene(activeIdx, path, &entityOrder)) {
            m_sceneManager->SetActiveImmediate(activeIdx);  // Make sure this scene is active right now
            // Update the editor hierarchy panel to reflect the reloaded entities
            if (m_hierarchyPanel) {
                m_hierarchyPanel->ClearUIState();  // Clear any selected entities/expanded tree nodes
                m_hierarchyPanel->SetEntityOrder(entityOrder);  // Restore original entity order
            }
            m_hasUnsavedChanges = false;  // File from disk is now the latest version
            // If we were in play mode and reloading, clear the playback snapshot
            if (m_getEditorState && m_getEditorState() != EditorState::Edit) {
                // This prevents restoring an old saved state when exiting play mode
                if (m_clearPlaybackSnapshot) {
                    m_clearPlaybackSnapshot();
                }
            }
            LOG_INFO("Reloaded active scene: " << path);
        }
        else {
            LOG_ERROR("Failed to reload scene: " << path);
        }
        return;
    }

    // Create a new empty scene object (using unique_ptr for automatic cleanup if needed)
    auto newScene = std::make_unique<Scenes::Scene>();

    // Register the scene with the SceneManager and transfer ownership
    // release() returns raw pointer and relinquishes ownership from unique_ptr
    // SceneManager now owns the Scene and will delete it when appropriate
    size_t idx = m_sceneManager->AddScene(newScene.release());

    // Load the scene file from disk into the newly created scene slot
    std::vector<uint32_t> entityOrder;  // Will be filled with entity IDs in their file order
    if (m_sceneManager->LoadScene(idx, path, &entityOrder)) {  // Load from disk
        m_sceneManager->SetActive(idx);  // Mark this scene as the active one
        m_sceneManager->Update();  // Process the activation immediately so UI sees the new scene
        // Update the editor's hierarchy panel to show the loaded entities
        if (m_hierarchyPanel) {
            m_hierarchyPanel->ClearUIState();  // Reset any previous selections/expansions
            m_hierarchyPanel->SetEntityOrder(entityOrder);  // Restore original entity order from file
        }
        m_currentScenePath = path;  // Remember which file this scene came from
        m_hasUnsavedChanges = false;  // File from disk is authoritative, no changes yet
        LOG_INFO("Opened scene: " << path);
    }
    else {
        LOG_ERROR("Failed to open scene: " << path);
        m_sceneManager->RemoveScene(idx);
    }
}

// Writes the currently active scene to a file on disk
// Handles entity serialization and calls pre-save callbacks for external cleanup
void EditorFileMenu::_saveSceneToFile(const std::string& path) {
    // Safety check: cannot save if there is no SceneManager
    if (!m_sceneManager) return;

    // Get the index of the currently active scene
    size_t activeIdx = m_sceneManager->GetActiveIndex();

    // Error check: if no scene is active, there's nothing to save
    if (activeIdx == static_cast<size_t>(-1)) {
        LOG_ERROR("No active scene to save");
        return;  // Stop and report the error
    }

    // Rebuild the entity order from the hierarchy panel to match the visual tree structure shown to user
    // This ensures entities save in the order the user sees them (important for scene organization)
    if (m_hierarchyPanel) {
        m_hierarchyPanel->RebuildEntityOrder();
    }

    // Get the entity order to save (preserves visual hierarchy structure in the file)
    const std::vector<uint32_t>* entityOrder = nullptr;  // Start with null (fallback: no specific order)
    if (m_hierarchyPanel) {
        entityOrder = &m_hierarchyPanel->GetEntityOrder();  // Use the hierarchy panel's entity list
    }

    // Call pre-save callback to let editor flush any external state (tilemaps, particles, etc.) before save
    if (m_preSaveCallback) {
        m_preSaveCallback(path);  // Opportunity to save associated data files
    }

    // Save the scene to disk with metadata (type="Scene", version="1.0")
    LOG_INFO("Saving scene: " << path);
    if (!m_sceneManager->SaveScene(activeIdx, path, "Scene", "1.0", entityOrder)) {
        // Save failed - log error and return without updating state
        LOG_ERROR("Failed to save scene: " << path);
        return;  // Leave m_hasUnsavedChanges as true so user knows save didn't happen
    }
    else {
        // Save successful - confirm in log and mark file as saved
        LOG_INFO("Successfully saved scene: " << path);
    }
}

