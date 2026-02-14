/* Start Header *****************************************************************/
/*!
\file   ProjectStartupUI.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
This file defines the ProjectStartupUI class which manages the project and
scene selection UI during the editor's startup process.

The ProjectStartupUI class renders different UI screens based on the current
EditorStartupStage, allowing the user to select a project, view a booting screen,
and pick a scene to open. It communicates user selections back to the editor
through callback functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ProjectStartupUI.h"

#include "core/Application.h"
#include "core/Logger.h"
#include "core/ProjectPaths.h"
#include "EditorStyle.h"
#include "serialization/ConfigurationSerializer.h"

#include <filesystem>
#include <algorithm>
#include <vector>
#include <fstream>
#include <cstring>
#include <unordered_set>
#include <cctype>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#ifdef _WIN32
#include <ShlObj.h>
#include <Shellapi.h>
#ifdef ERROR
#undef ERROR
#endif
#ifdef WARNING
#undef WARNING
#endif
#ifdef INFO
#undef INFO
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#endif

namespace Editor {
#ifdef USE_IMGUI
    namespace {
        // Helper function: Removes leading and trailing whitespace from a string
        // Whitespace includes spaces, tabs, newlines, and carriage returns
        std::string TrimCopy(std::string value) {

            // Erase whitespace from the beginning
            value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                [](unsigned char c) { return c != ' ' && c != '\t' && c != '\n' && c != '\r'; }));

            // Erase whitespace from the end
            value.erase(std::find_if(value.rbegin(), value.rend(),
                [](unsigned char c) { return c != ' ' && c != '\t' && c != '\n' && c != '\r'; }).base(), value.end());

            return value;  // Return the trimmed copy (original unchanged)
        }

        // Helper function: Converts all backslashes to forward slashes in a path
        // This normalizes Windows paths to a consistent format
        std::string NormalizePath(std::string value) {
            std::replace(value.begin(), value.end(), '\\', '/');
            return value;
        }

        // Helper function: Cleans a project path by trimming, normalizing, and removing trailing slashes
        // Returns empty string if input is empty or only whitespace
        std::string NormalizeProjectPath(std::string value) {
            value = TrimCopy(std::move(value));  // Remove leading/trailing whitespace

            if (value.empty()) {
                return value;  // Return early if nothing left to process
            }

            value = NormalizePath(std::move(value));  // Convert backslashes to forward slashes

            // Remove trailing slashes or backslashes at the end
            while (value.size() > 1 && (value.back() == '/' || value.back() == '\\')) {
                value.pop_back();
            }

            return value;
        }

        // Helper function: Creates a normalized key for path comparison
        // On Windows, converts path to lowercase for case-insensitive comparison
        // This allows reliable matching of paths typed differently by the user
        std::string NormalizePathKey(std::string value) {
            value = NormalizeProjectPath(std::move(value));  // Standardize the path first
#ifdef _WIN32
            // On Windows: convert to lowercase for case-insensitive comparison
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
            return value;  // On non-Windows, returns normalized path as-is
        }

    }

    // Main render function: Displays appropriate startup UI screen based on current editor stage
    // The stage can be: SelectProject, Booting, or SelectScene
    void ProjectStartupUI::Render() {
        // Safety check: need a method to get the current editor stage
        if (!m_stageGetter) {
            return;  // Cannot proceed without stage information
        }

        // Determine which UI screen to show based on the editor's current state
        const EditorStartupStage stage = m_stageGetter();
        const bool showProjectBrowser = (stage == EditorStartupStage::SelectProject) || m_forceProjectBrowser;
        const bool showBooting = (stage == EditorStartupStage::Booting);
        const bool showScenePicker = (stage == EditorStartupStage::SelectScene);

        // Draw the appropriate UI screen(s) based on current stage
        if (showProjectBrowser) {
            _renderProjectBrowser();  // Show project selection UI
        }
        if (showBooting) {
            _renderBootingScreen();  // Show loading/booting message
        }
        if (showScenePicker) {
            _renderScenePicker();  // Show scene selection UI
        }
    }

    // Displays the project browser UI where users can view recent projects, create new ones, or add existing ones
    void ProjectStartupUI::_renderProjectBrowser() {
        // Get the current ImGui viewport dimensions to fill the entire window
        ImGuiIO& io = ImGui::GetIO();

        // Position window at top-left corner (0,0)
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);

        // Stretch window to fill entire display
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

        // Window flags: Make window borderless and non-resizable to fill screen
        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("Project Browser", nullptr, windowFlags)) {
            // Reset lock state when window first appears (allows user interaction)
            if (ImGui::IsWindowAppearing()) {
                m_projectSelectionLocked = false;  // Allow user to click buttons
            }

            // Clean up the recent projects list: remove any empty paths
            if (m_settings) {
                auto& recents = m_settings->RecentProjects;
                recents.erase(std::remove_if(recents.begin(), recents.end(),
                    [](const std::string& path) { return TrimCopy(path).empty(); }), recents.end());
            }

            // If no project selected yet but we have a recent project, pre-select it
            if (m_selectedProjectPath.empty() && m_settings && !TrimCopy(m_settings->LastProject).empty()) {
                m_selectedProjectPath = NormalizeProjectPath(m_settings->LastProject);  // Use last opened project
            }

            // Draw spacing and "Projects" header title
            ImGui::Dummy(ImVec2(0.0f, 6.0f));  // Add vertical spacing
            const float headerStartX = ImGui::GetCursorPosX();
            const float headerRightEdge = headerStartX + ImGui::GetContentRegionAvail().x;  // Right edge of window
            ImGui::SetWindowFontScale(1.4f);
            ImGui::Text("Projects");  // Main title
            ImGui::SetWindowFontScale(1.0f);  // Reset font size

            // Calculate button widths and positions to align them to the right of the header
            const ImGuiStyle& style = ImGui::GetStyle();
            const float addButtonWidth = ImGui::CalcTextSize("Add from Disk").x + style.FramePadding.x * 2.0f;
            const float newButtonWidth = ImGui::CalcTextSize("New Project").x + style.FramePadding.x * 2.0f;
            const float buttonsWidth = addButtonWidth + newButtonWidth + style.ItemSpacing.x;

            // Position the buttons to the right of the "Projects" title
            ImGui::SameLine();  // Put buttons on same line as title
            ImGui::SetCursorPosX(headerRightEdge - buttonsWidth);  // Align to right edge
            // "Add from Disk" button - styled with secondary color scheme
            ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);

            if (ImGui::Button("Add from Disk") && !m_projectSelectionLocked) {

                // Open folder picker dialog
                const std::string picked = _pickFolder();
                
                if (!picked.empty() && m_callbacks.OnProjectSelected) {
                    m_projectSelectionLocked = true;  // Prevent user from clicking again while loading
                    m_callbacks.OnProjectSelected(picked);  // Trigger callback with selected path
                    m_forceProjectBrowser = false;  // Close the project browser
                }
            }
            ImGui::PopStyleColor(3);  // Restore button colors

            // "New Project" button - styled with primary color scheme
            ImGui::SameLine();  // Put on same line as previous button
            ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::PrimaryButton);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::PrimaryButtonHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::PrimaryButtonActive);
            if (ImGui::Button("New Project") && !m_projectSelectionLocked) {
                m_openNewProjectModal = true;  // Flag to show the "New Project" dialog
            }
            ImGui::PopStyleColor(3);  // Restore button colors

            // Calculate layout for the two main areas: project list and project details
            ImGui::Dummy(ImVec2(0.0f, 12.0f));  // Add vertical spacing

            const float contentHeight = ImGui::GetContentRegionAvail().y;  // Available vertical space
            const float listHeight = (std::max)(120.0f, contentHeight * 0.8f);  // 80% for list
            const float detailsHeight = (std::max)(80.0f, contentHeight - listHeight - style.ItemSpacing.y);  // 20% for details

            // Build list of unique project paths for display
            std::vector<std::string> projectPaths;  // Will hold deduplicated project paths
            if (m_settings) {
                std::vector<std::string> ordered;  // Ordered list of unique paths
                std::unordered_set<std::string> seen;  // Set to track which paths we've already added

                // Lambda: Add a path to the list (only if not already seen)
                auto addPath = [&](const std::string& rawPath) {
                    const std::string cleaned = NormalizeProjectPath(rawPath);  // Clean path

                    if (cleaned.empty()) {
                        return;  // Skip empty paths
                    }

                    const std::string key = NormalizePathKey(cleaned);  // Create lookup key

                    // insert() returns pair<iterator, bool> - bool is true if we inserted (not already present)
                    if (seen.insert(key).second) {
                        ordered.push_back(cleaned);  // Add to list only if new
                    }
                };

                // Add last opened project first (if exists)
                addPath(m_settings->LastProject);

                // Add remaining recent projects in their stored order
                for (const auto& path : m_settings->RecentProjects) {
                    addPath(path);
                }

                projectPaths = std::move(ordered);  // Move ordered list into return variable
            }

            // Render the scrollable list of available projects
            const std::string selectedKey = NormalizePathKey(m_selectedProjectPath);  // Normalize selection for comparison
            ImGui::BeginChild("RecentProjectsList", ImVec2(0.0f, listHeight), true);  // Scrollable list box
            if (!projectPaths.empty()) {

                // Disable list interaction if a project is currently being loaded
                const bool disableList = m_projectSelectionLocked;
                if (disableList) {
                    ImGui::BeginDisabled();  // Make all controls grayed out
                }

                // Render each project as a selectable item
                for (size_t i = 0; i < projectPaths.size(); ++i) {
                    const std::string& path = projectPaths[i];

                    // Extract just the folder name from the full path for display
                    std::string projectName = std::filesystem::path(path).filename().string();
                    if (projectName.empty()) {
                        projectName = path;  // Fall back to full path if no folder name
                    }

                    // Check if this item is currently selected
                    const bool isSelected = (selectedKey == NormalizePathKey(path));
                    ImGui::PushID(static_cast<int>(i));  // Unique identifier for this item

                    // Selectable item that allows double-click
                    if (ImGui::Selectable(projectName.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        m_selectedProjectPath = path;  // Update selected project

                        // Check for double-click to open project immediately
                        if (ImGui::IsMouseDoubleClicked(0) && m_callbacks.OnProjectSelected) {
                            m_projectSelectionLocked = true;  // Lock UI while loading
                            m_callbacks.OnProjectSelected(path);  // Trigger project load
                            m_forceProjectBrowser = false;  // Close browser
                        }
                    }

                    // Show tooltip with full path when hovering
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", path.c_str());
                    }
                    ImGui::PopID();  // Pop unique identifier
                }
                if (disableList) {
                    ImGui::EndDisabled();  // Re-enable controls
                }
            } else {
                ImGui::TextDisabled("No recent projects.");  // Message when no projects found
            }
            ImGui::EndChild();  // End scrollable list

            // Render project details panel on the right
            ImGui::BeginChild("ProjectDetails", ImVec2(0.0f, detailsHeight), true);
            if (!m_selectedProjectPath.empty()) {

                // Get just the folder name from the path
                std::string selectedName = std::filesystem::path(m_selectedProjectPath).filename().string();
                if (selectedName.empty()) {
                    selectedName = m_selectedProjectPath;  // Use full path as fallback
                }

                // Display project name and path
                ImGui::Text("Project");
                ImGui::SameLine();
                ImGui::Text("%s", selectedName.c_str());
                ImGui::Spacing();
                ImGui::Text("Path");
                ImGui::SameLine();
                ImGui::TextWrapped("%s", m_selectedProjectPath.c_str());  // Wrap long paths

                ImGui::Spacing();
                ImGui::Separator();  // Visual divider
                ImGui::Spacing();

                // "Open Folder" button - opens in file explorer
                ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
                if (ImGui::Button("Open Folder") && !m_projectSelectionLocked) {
                    _openFolder(m_selectedProjectPath);  // Open in system file browser
                }
                ImGui::PopStyleColor(3);

                // "Open Project" button - main action to load project
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::PrimaryButton);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::PrimaryButtonHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::PrimaryButtonActive);
                if (ImGui::Button("Open Project") && !m_projectSelectionLocked) {
                    if (m_callbacks.OnProjectSelected) {
                        m_projectSelectionLocked = true;  // Lock to prevent double-opening
                        m_callbacks.OnProjectSelected(m_selectedProjectPath);  // Load project
                    }
                    m_forceProjectBrowser = false;  // Close browser
                }
                ImGui::PopStyleColor(3);

                // "Remove" button - danger colored (red)
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::DangerButton);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::DangerButtonHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::DangerButtonActive);
                if (ImGui::Button("Remove")) {
                    m_deleteTargetPath = m_selectedProjectPath;  // Mark for deletion
                    m_openDeleteConfirm = true;  // Trigger confirmation dialog
                    ImGui::OpenPopup("Remove Project?");  // Open the modal
                }
                ImGui::PopStyleColor(3);

                // "Close" button only visible when project browser is forced open (e.g., via menu)
                if (m_forceProjectBrowser) {
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
                    if (ImGui::Button("Close")) {
                        m_forceProjectBrowser = false;  // Hide browser
                        m_projectSelectionLocked = false;  // Allow interaction again
                    }
                    ImGui::PopStyleColor(3);
                }
            } else {
                ImGui::TextDisabled("Select a project to view details.");  // Placeholder when nothing selected
            }
            ImGui::EndChild();  // End details panel

            // Modal popup for confirming project deletion from the list
              if (m_openDeleteConfirm) {
                  ImGui::OpenPopup("Remove Project?");
                  if (ImGui::BeginPopupModal("Remove Project?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextWrapped("Remove this project from the list? This will not delete the folder.");
                    ImGui::Separator();
                    // Show which project will be removed
                    if (!m_deleteTargetPath.empty()) {
                        ImGui::TextWrapped("%s", m_deleteTargetPath.c_str());
                    }

                    // Calculate button positioning to align them to the right
                    const float buttonWidth = 120.0f;  // Width of each button
                    const float spacing = ImGui::GetStyle().ItemSpacing.x;  // Space between buttons
                    const float totalButtonsWidth = buttonWidth * 3.0f + spacing * 2.0f;  // Cancel + Remove + Delete
                    const float rightEdge = ImGui::GetWindowContentRegionMax().x;  // Right edge of window
                    ImGui::SetCursorPosX((std::max)(0.0f, rightEdge - totalButtonsWidth));  // Align right

                    // "Cancel" button - dismiss without doing anything
                    ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
                    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
                        m_deleteTargetPath.clear();  // Clear target
                        m_openDeleteConfirm = false;  // Close dialog
                        ImGui::CloseCurrentPopup();  // Close modal
                    }
                    ImGui::PopStyleColor(3);

                    // "Remove" button - remove from list only (keeps folder)
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::PrimaryButton);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::PrimaryButtonHover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::PrimaryButtonActive);
                      if (ImGui::Button("Remove", ImVec2(buttonWidth, 0))) {
                          // Remove from recent projects list (normalize for reliable match)
                          if (m_settings) {
                              const std::string targetKey = NormalizePathKey(m_deleteTargetPath);
                              auto& recents = m_settings->RecentProjects;
                              recents.erase(std::remove_if(recents.begin(), recents.end(),
                                  [&](const std::string& path) {
                                      return NormalizePathKey(path) == targetKey;
                                  }), recents.end());

                              if (NormalizePathKey(m_settings->LastProject) == targetKey) {
                                  m_settings->LastProject.clear();
                              }
                          }

                        // Deselect if this was the selected project
                        if (m_selectedProjectPath == m_deleteTargetPath) {
                            m_selectedProjectPath.clear();
                        }

                        m_deleteTargetPath.clear();  // Clear target
                        m_openDeleteConfirm = false;  // Close dialog
                        ImGui::CloseCurrentPopup();  // Close modal
                    }

                    // Show tooltip explaining that folder is not deleted
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Does not delete folder.");
                    }
                    ImGui::PopStyleColor(3);

                    // "Delete" button - danger colored, removes from list AND deletes folder
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::DangerButton);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::DangerButtonHover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::DangerButtonActive);
                      if (ImGui::Button("Delete", ImVec2(buttonWidth, 0))) {

                        // Actually delete the folder from disk
                        if (!m_deleteTargetPath.empty() && std::filesystem::exists(m_deleteTargetPath)
                            && std::filesystem::is_directory(m_deleteTargetPath)) {
                            std::error_code ec;  // Capture any errors
                            std::filesystem::remove_all(m_deleteTargetPath, ec);  // Recursively delete

                            if (ec) {
                                LOG_WARNING("Failed to delete project: " << m_deleteTargetPath << " (" << ec.message() << ")");
                            }
                        }

                        // Also remove from settings lists
                          if (m_settings) {
                              const std::string targetKey = NormalizePathKey(m_deleteTargetPath);
                              auto& recents = m_settings->RecentProjects;
                              recents.erase(std::remove_if(recents.begin(), recents.end(),
                                  [&](const std::string& path) {
                                      return NormalizePathKey(path) == targetKey;
                                  }), recents.end());

                              if (NormalizePathKey(m_settings->LastProject) == targetKey) {
                                  m_settings->LastProject.clear();
                              }
                          }

                        // Deselect if this was the selected project
                        if (m_selectedProjectPath == m_deleteTargetPath) {
                            m_selectedProjectPath.clear();
                        }

                        m_deleteTargetPath.clear();  // Clear target
                        m_openDeleteConfirm = false;  // Close dialog
                        ImGui::CloseCurrentPopup();  // Close modal
                    }
                    // Show tooltip explaining that we delete both list entry and folder
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Deletes the folder too.");
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::EndPopup();  // End modal window
                }
            }

            // Modal popup for creating a new project
            if (m_openNewProjectModal) {
                m_openNewProjectModal = false;  // Only open once per flag toggle
                // Initialize default location on first open
                if (!m_projectLocationInitialized) {
                    const std::string defaultRoot = Engine::ProjectPaths::GetEditorDocumentsRoot();  // Get user's Documents folder
                    strncpy_s(m_projectLocationBuffer, defaultRoot.c_str(), sizeof(m_projectLocationBuffer) - 1);
                    m_projectLocationInitialized = true;  // Don't reinitialize next time
                }
                ImGui::OpenPopup("New Project");  // Show the modal
            }

            // "New Project" modal window
            if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                // Input field for project name
                ImGui::Text("Project Name");
                ImGui::InputText("##ProjectName", m_projectNameBuffer, sizeof(m_projectNameBuffer));

                // Input field for project location with browse button
                ImGui::Text("Location");
                const float rightEdge = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;  // Right edge
                const float iconSize = ImGui::GetFrameHeight();  // Button size
                const float browseWidth = ImGui::CalcTextSize("...").x + style.FramePadding.x * 2.0f;  // Width of browse button
                // Set width of the path input field to leave room for browse button
                ImGui::SetNextItemWidth(rightEdge - ImGui::GetCursorPosX() - browseWidth - style.ItemSpacing.x);
                ImGui::InputText("##ProjectLocation", m_projectLocationBuffer, sizeof(m_projectLocationBuffer));
                
                // "Browse" button to pick a folder
                ImGui::SameLine();
                ImGui::SetCursorPosX(rightEdge - browseWidth);
                ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
                bool browseClicked = ImGui::Button("...", ImVec2(browseWidth, iconSize));
                ImGui::PopStyleColor(3);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Browse...");
                }

                // Handle browse button click
                if (browseClicked) {
                    const std::string picked = _pickFolder();  // Open folder picker
                    
                    if (!picked.empty()) {
                        strncpy_s(m_projectLocationBuffer, picked.c_str(), sizeof(m_projectLocationBuffer) - 1);
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Check if project name is valid (not empty)
                const bool canCreate = !TrimCopy(m_projectNameBuffer).empty();
                float createWidth = ImGui::CalcTextSize("Create").x + style.FramePadding.x * 2.0f;
                float cancelWidth = ImGui::CalcTextSize("Cancel").x + style.FramePadding.x * 2.0f;
                const float buttonsRightEdge = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
                // Align buttons to the right
                ImGui::SetCursorPosX(buttonsRightEdge - (createWidth + cancelWidth + style.ItemSpacing.x));
                
                // "Cancel" button - close without creating
                ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();  // Close modal
                    m_projectSelectionLocked = false;  // Re-enable UI
                }
                ImGui::PopStyleColor(3);

                // "Create" button - create the project
                ImGui::SameLine();
                if (!canCreate) ImGui::BeginDisabled();  // Disable if name is empty
                ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::PrimaryButton);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::PrimaryButtonHover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::PrimaryButtonActive);
                if (ImGui::Button("Create")) {
                    m_projectSelectionLocked = true;  // Lock UI during creation
                    _createProject(m_projectNameBuffer, m_projectLocationBuffer);  // Create project
                    m_forceProjectBrowser = false;  // Close browser
                    ImGui::CloseCurrentPopup();  // Close modal
                }
                ImGui::PopStyleColor(3);
                if (!canCreate) ImGui::EndDisabled();

                ImGui::EndPopup();  // End modal
            }
        }
        ImGui::End();
    }

    // Displays a loading/booting screen while the editor initializes the project
    void ProjectStartupUI::_renderBootingScreen() {
        // Get the main viewport (the game window)
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        if (vp) {
            // Position window at the center of the screen
            ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        }

        // Open the modal (shows it if not already visible)
        ImGui::OpenPopup("Loading Project");
        // Display the loading modal with message
        if (ImGui::BeginPopupModal("Loading Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::TextDisabled("Initializing editor...");  // Grayed-out status message
            ImGui::EndPopup();
        }
    }

    // Displays a window for the user to select or create a scene to open
    void ProjectStartupUI::_renderScenePicker() {
        // Set window size and center it on screen
        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 windowSize(620.0f, 0.0f);  // 620 pixels wide, auto height
        const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);  // Center of screen
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));  // Center window on center position
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);  // Always apply size

        // Get the project root directory
        std::filesystem::path projectRoot = Engine::ProjectPaths::GetProjectRoot();
        if (projectRoot.empty()) {
            return;  // Cannot proceed without a project
        }

        // Scan project for all scene files (.scn or .scene extensions)
        std::filesystem::path scenesRoot = Engine::ProjectPaths::GetProjectRoot();
        std::vector<std::pair<std::string, std::string>> scenes;  // pairs of (relative_path, absolute_path)
        if (std::filesystem::exists(scenesRoot)) {
            // Recursively iterate through all files in project directory
            for (const auto& entry : std::filesystem::recursive_directory_iterator(scenesRoot)) {
                if (!entry.is_regular_file()) {
                    continue;  // Skip directories
                }

                // Check if file has scene extension
                const std::string ext = entry.path().extension().string();
                if (ext == ".scn" || ext == ".scene") {
                    // Get both absolute path and relative path (from project root)
                    std::string absolute = entry.path().string();
                    std::string relative = Engine::ProjectPaths::ToRelativePath(absolute);
                    if (relative.empty()) {
                        relative = entry.path().filename().string();  // Fallback to just filename
                    }
                    scenes.emplace_back(relative, absolute);  // Add to list
                }
            }
        }

        // Sort scenes alphabetically by relative path for consistent display
        std::sort(scenes.begin(), scenes.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        // Handle project changes: update scene list and pre-select startup scene
        if (m_cachedProjectRoot != projectRoot.string()) {
            m_cachedProjectRoot = projectRoot.string();  // Remember this project root
            m_selectedSceneIndex = -1;  // Reset selection

            // Try to auto-select the startup scene from project settings
            if (Engine::CORE && Engine::CORE->HasProjectSettings()) {
                const std::string startup = NormalizePath(Engine::CORE->GetProjectSettings().StartupScene);
                if (!startup.empty()) {
                    // Search for matching scene in the list
                    for (size_t i = 0; i < scenes.size(); ++i) {
                        if (NormalizePath(scenes[i].first) == startup) {
                            m_selectedSceneIndex = static_cast<int>(i);  // Found it!
                            break;
                        }
                    }
                }
            }
        }

        // Clamp selection index to valid range
        if (m_selectedSceneIndex >= static_cast<int>(scenes.size())) {
            m_selectedSceneIndex = -1;  // Invalid, clear selection
        }

        // Show the "Select Scene" modal
        ImGui::OpenPopup("Select Scene");  // Show modal (idempotent, safe to call each frame)
        if (ImGui::BeginPopupModal("Select Scene", nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
            // Scrollable list of available scenes
            ImGui::BeginChild("SceneList", ImVec2(0.0f, 200.0f), true);
            if (!scenes.empty()) {
                // Render each scene as a selectable item
                for (size_t i = 0; i < scenes.size(); ++i) {
                    const bool selected = (m_selectedSceneIndex == static_cast<int>(i));
                    // Selectable scene that allows double-click to open
                    if (ImGui::Selectable(scenes[i].first.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        m_selectedSceneIndex = static_cast<int>(i);  // Update selection
                        // Check for double-click to immediately open scene
                        if (ImGui::IsMouseDoubleClicked(0) && m_callbacks.OnSceneSelected) {
                            m_callbacks.OnSceneSelected(scenes[i].second);  // Trigger open with absolute path
                        }
                    }
                }
            } else {
                ImGui::TextDisabled("No scenes found in project.");  // Message when no scenes exist
            }
            ImGui::EndChild();  // End scrollable list

            ImGui::Spacing();
            // Input field to create a new scene with a name
            const ImGuiStyle& style = ImGui::GetStyle();
            const float createWidth = ImGui::CalcTextSize("Create Scene").x + style.FramePadding.x * 2.0f;
            const float rightEdge = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;  // Right edge
            ImGui::SetNextItemWidth(rightEdge - ImGui::GetCursorPosX() - createWidth - style.ItemSpacing.x);
            ImGui::InputTextWithHint("##NewScene", "Scene name", m_sceneNameBuffer, sizeof(m_sceneNameBuffer));
            ImGui::SameLine();
            ImGui::SetCursorPosX(rightEdge - createWidth);
            // Only enable button if user typed a name
            const bool canCreate = !TrimCopy(m_sceneNameBuffer).empty();
            if (!canCreate) ImGui::BeginDisabled();
            if (ImGui::Button("Create Scene")) {
                _createScene(m_sceneNameBuffer);  // Create new scene file
            }
            if (!canCreate) ImGui::EndDisabled();

            ImGui::Separator();
            // "Continue Without Scene" button for opening editor with no scene
            const float continueWidth = ImGui::CalcTextSize("Continue Without Scene").x + style.FramePadding.x * 2.0f;
            const float continueRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(continueRight - continueWidth);
            ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
            if (ImGui::Button("Continue Without Scene")) {
                if (m_callbacks.OnContinueWithoutScene) {
                    m_callbacks.OnContinueWithoutScene();  // Proceed without opening a scene
                }
            }
            ImGui::PopStyleColor(3);
        }
        ImGui::EndPopup();  // End modal window
    }

    // Opens a Windows folder browser dialog using the Shell API
    // Returns the selected folder path, or empty string if user cancels
    std::string ProjectStartupUI::_pickFolder() {
#ifdef _WIN32
        // Configure the folder picker dialog
        BROWSEINFOA bi = {};  // Initialize all fields to zero
        bi.lpszTitle = "Select Project Folder";  // Dialog title
        // Flags: only show folders, use newer dialog style
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

        // Show the dialog and get the result (pidl = pointer to item ID list)
        LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
        if (pidl) {
            // If user selected something, convert the pidl to a path string
            char path[MAX_PATH] = {};  // Buffer for folder path
            if (SHGetPathFromIDListA(pidl, path)) {
                CoTaskMemFree(pidl);  // Free the pidl memory allocated by Shell
                return std::string(path);  // Return the path user selected
            }
            CoTaskMemFree(pidl);  // Free even if conversion failed
        }
#endif
        return {};  // Return empty string if cancelled or error occurred
    }

    // Opens a folder in the native file explorer/finder
    void ProjectStartupUI::_openFolder(const std::string& path) {
#ifdef _WIN32
        // Safety check: path must not be empty
        if (path.empty()) {
            return;
        }
        // Use Windows ShellExecute to open the folder in Explorer
        // "open" = default action, nullptr = no parameters, SW_SHOWNORMAL = normal window
        ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
        // Non-Windows platforms: do nothing (not implemented)
        (void)path;  // Suppress unused parameter warning
#endif
    }

    // Creates a new project folder structure with default directories and settings file
    void ProjectStartupUI::_createProject(const std::string& projectName, const std::string& parentFolder) {
        // Validate project name (cannot be empty or only whitespace)
        std::string trimmed = TrimCopy(projectName);
        if (trimmed.empty()) {
            LOG_WARNING("Project name is empty");
            return;  // Exit if validation fails
        }

        // Validate parent folder (use default if not provided)
        std::string trimmedParent = TrimCopy(parentFolder);
        if (trimmedParent.empty()) {
            trimmedParent = Engine::ProjectPaths::GetEditorDocumentsRoot();  // Use Documents folder
        }

        // Construct the full project path (parentFolder/projectName/)
        std::filesystem::path root = std::filesystem::path(trimmedParent) / trimmed;
        if (std::filesystem::exists(root)) {
            // Check if path exists but is not a folder
            if (!std::filesystem::is_directory(root)) {
                LOG_ERROR("Project path exists but is not a folder: " << root.string());
                return;  // Cannot create project here
            }
            // Check if folder is not empty
            std::error_code ec;
            if (!std::filesystem::is_empty(root, ec) || ec) {
                LOG_WARNING("Project folder is not empty: " << root.string());
                return;  // Cannot create in non-empty folder
            }
        }
        
        // Create the default project subdirectories
        std::filesystem::path assetsDir = root / "Assets";  // For game assets
        std::filesystem::path scenesDir = root / "Scenes";  // For scene files
        std::filesystem::path scriptsDir = root / "Scripts";  // For game scripts

        try {
            // Recursively create all directories (also creates parent root if needed)
            std::filesystem::create_directories(assetsDir);
            std::filesystem::create_directories(scenesDir);
            std::filesystem::create_directories(scriptsDir);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create project directories: " << e.what());
            return;  // Exit on creation error
        }

        // Create a default ProjectSettings.json file
        ProjectSettings settings;  // Create settings object
        settings.Title = trimmed;  // Set project name

        std::filesystem::path settingsPath = root / "ProjectSettings.json";  // Where to save settings
        // Save the settings to the JSON file
        Serialization::ConfigurationSerializer::SaveProjectSettings(settingsPath.string(), settings);

        // Trigger the project selected callback so editor loads the new project
        if (m_callbacks.OnProjectSelected) {
            m_callbacks.OnProjectSelected(root.string());  // Pass full project path
        }
    }

    // Creates a new empty scene file in the project's Scenes directory
    void ProjectStartupUI::_createScene(const std::string& sceneName) {
        // Validate scene name (cannot be empty or only whitespace)
        std::string trimmed = TrimCopy(sceneName);
        if (trimmed.empty()) {
            LOG_WARNING("Scene name is empty");
            return;  // Exit if validation fails
        }

        // Get the project root path
        std::filesystem::path projectRoot = Engine::ProjectPaths::GetProjectRoot();
        if (projectRoot.empty()) {
            LOG_WARNING("Project root not set; cannot create scene");
            return;  // Cannot create scene without knowing project location
        }

        // Get the Scenes directory within the project
        std::filesystem::path scenesDir = Engine::ProjectPaths::GetScenesPath();
        try {
            // Make sure Scenes directory exists
            std::filesystem::create_directories(scenesDir);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create Scenes directory: " << e.what());
            return;  // Exit on directory creation error
        }

        // Build full path to new scene file (Scenes/sceneName.scn)
        std::filesystem::path scenePath = scenesDir / (trimmed + ".scn");
        if (std::filesystem::exists(scenePath)) {
            LOG_WARNING("Scene already exists: " << scenePath.string());
            return;  // Don't overwrite existing scene
        }

        // Create JSON content for the new scene with standard structure
        std::string sceneContent =
            "{\n"
            "  \"Version\": \"1.0\",\n"
            "  \"Name\": \"" + trimmed + "\",\n"  // Scene name from user input
            "  \"Entities\": []\n"  // Empty entity list (no objects yet)
            "}\n";

        try {
            // Open file for writing
            std::ofstream file(scenePath);
            if (!file.is_open()) {
                LOG_ERROR("Failed to create scene file: " << scenePath.string());
                return;  // Cannot open file for writing
            }
            // Write JSON content to file
            file << sceneContent;
            // File automatically closes when ofstream goes out of scope
        } catch (const std::exception& e) {
            LOG_ERROR("Error creating scene: " << e.what());
        }
    }
#else
    void ProjectStartupUI::Render() {}
#endif
}
