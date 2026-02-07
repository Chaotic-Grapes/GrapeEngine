/* Start Header *****************************************************************/
/*!
\file   EditorFileMenu.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
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
#endif

#include "EditorFileMenu.h"
#include "HierarchyPanel.h"
#include "UndoSystem.h"
#include "EditorStyle.h"
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
#include "serialization/ConfigurationSerializer.h"
#include <filesystem>
#include <nlohmann/json.hpp>

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

    // File menu (New, Open, Save As, Exit)
    if (ImGui::BeginMenu("File")) {
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
}

// Draws the "Edit" menu with Undo/Redo and Project Settings
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

    // Helper to revert settings from disk
    auto revertSettings = [&]() {
        std::string projectRoot = Engine::ProjectPaths::GetProjectRoot();
        if (Engine::CORE->LoadProjectSettings(projectRoot)) {
            m_projectSettingsDirty = false;
        }
    };

    // Helpers to reset specific sections to defaults
    auto resetProjectDefaults = [&]() {
        settings.Title = "GrapeEngine Game Project";
        settings.Version = "1.0.0";
        settings.StartupScene.clear();
        m_projectSettingsDirty = true;
    };

    // Reset window settings to defaults
    auto resetWindowDefaults = [&]() {
        settings.WindowSettings.Width = 1600;
        settings.WindowSettings.Height = 900;
        settings.WindowSettings.Mode = "Fullscreen";
        settings.WindowSettings.Fullscreen = true;
        settings.WindowSettings.VSync = true;
        m_projectSettingsDirty = true;
    };

    // Reset physics settings to defaults
    auto resetPhysicsDefaults = [&]() {
        settings.Physics.Gravity = -9.81f;
        settings.Physics.TimeStep = 0.016f;
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
        const char* resetIcon = "\xEF\x91\xBF"; // Reset icon (material: restart_alt)
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

        const char* resetIcon = "\xEF\x91\xBF"; // Reset icon (material: restart_alt)
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
    auto renderSection = [&](const char* headerName, const std::function<void()>& renderBody,
                             const std::function<void()>& onReset) {
        // Collapsing header with custom styling
        ImGui::SetNextItemAllowOverlap();
        const ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_Framed;
        const ImGuiID headerId = ImGui::GetID(headerName);
        const bool wasOpen = ImGui::GetStateStorage()->GetBool(
            headerId, (headerFlags & ImGuiTreeNodeFlags_DefaultOpen) != 0);
        const float headerRounding = wasOpen ? 0.0f : ImGui::GetStyle().FrameRounding;

        // Render the collapsing header
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, headerRounding);
        if (m_boldFont) ImGui::PushFont(m_boldFont);
        const bool nodeOpen = ImGui::CollapsingHeader(headerName, headerFlags);
        if (m_boldFont) ImGui::PopFont();
        ImGui::PopStyleVar();

        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        renderHeaderResetButton(headerName, headerMax.x, "Reset section to defaults", onReset);

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

    // Validate settings
    const bool titleValid = !settings.Title.empty();
    const bool widthValid = settings.WindowSettings.Width > 0;
    const bool heightValid = settings.WindowSettings.Height > 0;
    const bool settingsValid = titleValid && widthValid && heightValid;

    // Ensure Window Mode is valid
    if (settings.WindowSettings.Mode.empty()) {
        settings.WindowSettings.Mode = settings.WindowSettings.Fullscreen ? "Fullscreen" : "Windowed";
        m_projectSettingsDirty = true;
    }
    if (m_symbolsFont) {
        EditorUI::SetSymbolsFont(m_symbolsFont);
    }

    // Render the modal window
    ImGui::SetNextWindowSize(ImVec2(640, 520), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
    if (ImGui::Begin("Project Settings", &m_showProjectSettings, ImGuiWindowFlags_NoCollapse)) {
        ImGui::PopStyleVar();
        const float footerHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y * 1.5f
            + ImGui::GetStyle().WindowPadding.y;
        ImGui::BeginChild("ProjectSettingsBody", ImVec2(0.0f, -footerHeight), false);
        nlohmann::json projectData = {
            {"Title", settings.Title},
            {"Version", settings.Version},
            {"StartupScene", settings.StartupScene}
        };
        const nlohmann::json projectDefaults = {
            {"Title", "Game Project"},
            {"Version", "1.0.0"},
            {"StartupScene", ""}
        };

        // Render Project Information section
        renderSection("Project Information", [&]() {
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
            EditorUI::RenderTextProperty("Startup Scene", projectData, "StartupScene");
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
        const nlohmann::json windowDefaults = {
            {"Width", 1600},
            {"Height", 900},
            {"Mode", "Fullscreen"},
            {"VSync", true}
        };

        // Render Window Settings section
        renderSection("Window Settings", [&]() {
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

            // Custom rendering for Window Mode combo box
            ImGui::Text("Mode");
            ImGui::SameLine();
            const float axisLabelWidth = ImGui::CalcTextSize("W").x;
            const float fieldStartX = EditorUI::GetContentStartX() + axisLabelWidth + 6.0f;
            ImGui::SetCursorPosX(fieldStartX);
            ImGui::SetNextItemWidth(180.0f);

            // Window Mode combo box
            const char* modes[] = { "Windowed", "Fullscreen", "Borderless" };
            std::string modeValue = windowData.value("Mode", std::string("Windowed"));
            int modeIndex = 0;
            if (modeValue == "Fullscreen") {
                modeIndex = 1;
            } else if (modeValue == "Borderless") {
                modeIndex = 2;
            }

            // Render the combo box and handle selection
            if (ImGui::Combo("##WindowMode", &modeIndex, modes, 3)) {
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
        const nlohmann::json physicsDefaults = {
            {"Gravity", -9.81f},
            {"TimeStep", 0.016f}
        };

        renderSection("Physics Settings", [&]() {
            EditorUI::RegisterDefaultDataScope(physicsData, physicsDefaults);
            const float axisLabelWidth = ImGui::CalcTextSize("W").x;
            const float fieldStartX = EditorUI::GetContentStartX() + axisLabelWidth + 6.0f;

            ImGui::Text("Gravity");
            ImGui::SameLine();
            ImGui::SetCursorPosX(fieldStartX);
            ImGui::SetNextItemWidth(120.0f);
            {
                float value = physicsData.value("Gravity", -9.81f);
                if (ImGui::DragFloat("##Gravity", &value, 0.001f, 0.0f, 0.0f, "%.6f")) {
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
                if (ImGui::DragFloat("##TimeStep", &value, 0.0001f, 0.0f, 0.0f, "%.6f")) {
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
        
        ImGui::EndChild();

        ImGui::Separator();
        
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
}

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

// Shows a Windows "Open File" dialog so the user can choose a scene file from disk
// If a file is selected we forward the path to _openScene which loads it
void EditorFileMenu::OpenSceneDialog() {
#ifdef _WIN32
    // No SceneManager means no scenes to load into
    if (!m_sceneManager) return;

    // OPENFILENAMEA is a struct used by the Win32 API to configure the dialog
    OPENFILENAMEA ofn = {};
    char szFile[260] = {};  // Buffer to store the resulting file path

    // Fill in the required fields for the dialog
    ofn.lStructSize = sizeof(ofn);
    auto* context = Engine::CORE->GetPlatformContext();
    auto* mainWindow = context ? context->GetMainWindow() : nullptr;
    if (!mainWindow)
        return;

    ofn.hwndOwner = glfwGetWin32Window(reinterpret_cast<GLFWwindow*>(mainWindow->GetNativeHandle()));
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Scene Files (*.scn;*.scene)\0*.scn;*.scene\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    // TODO: Remove when editor is separated - use project-relative paths
    ofn.lpstrInitialDir = Engine::ProjectPaths::GetProjectRoot().c_str();

    // OFN_PATHMUSTEXIST: ensures the folder exists
    // OFN_FILEMUSTEXIST: ensures the file exists before returning
    // OFN_NOCHANGEDIR: keeps the working directory unchanged
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    // Show the Open dialog
    // If the user selects a file and presses OK, GetOpenFileNameA returns non zero
    if (GetOpenFileNameA(&ofn)) {
        // szFile now contains the full path that the user chose
        _openScene(szFile);
    }
#endif
}

// Shows a Windows "Save File" dialog so the user can choose where to write the scene
// If the user confirms, the current active scene is saved to that path
void EditorFileMenu::SaveSceneAsDialog() {
#ifdef _WIN32
    // If there is no SceneManager then there is nothing to save
    if (!m_sceneManager) return;

    // Same as above
    OPENFILENAMEA ofn = {};
    char szFile[260] = {};

    ofn.lStructSize = sizeof(ofn);
    auto* context2 = Engine::CORE->GetPlatformContext();
    auto* mainWindow2 = context2 ? context2->GetMainWindow() : nullptr;
    if (!mainWindow2)
        return;
        
    ofn.hwndOwner = glfwGetWin32Window(reinterpret_cast<GLFWwindow*>(mainWindow2->GetNativeHandle()));
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Scene Files (*.scn;*.scene)\0*.scn;*.scene\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    // TODO: Remove when editor is separated - use project-relative paths
    ofn.lpstrInitialDir = Engine::ProjectPaths::GetProjectRoot().c_str();
    ofn.lpstrDefExt = "scn";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    // Show the Save dialog
    if (GetSaveFileNameA(&ofn)) {
        // Copy the path that the user entered or selected
        std::string savePath = szFile;

        // If the user did not type an extension we append ".scn" by default
        if (savePath.find(".scn") == std::string::npos && savePath.find(".scene") == std::string::npos) {
            savePath += ".scn";
        }

        // Actually write the active scene to this path
        _saveSceneToFile(savePath);
        m_currentScenePath = savePath;
        m_hasUnsavedChanges = false;
    }
#endif
}

void EditorFileMenu::SaveScene() {
#ifdef _WIN32
    if (!m_sceneManager) return;
    if (m_currentScenePath.empty()) { SaveSceneAsDialog(); return; }
    _saveSceneToFile(m_currentScenePath);
    m_hasUnsavedChanges = false;
#endif
}

// -------------------------------------------------------------------------
// Keyboard Shortcuts
// -------------------------------------------------------------------------

// Handle keyboard shortcuts for editor actions
void EditorFileMenu::HandleShortcuts(float& uiScale) {
    bool ctrlDown = Input::IsKeyDown(KEY_LEFT_CONTROL) || Input::IsKeyDown(KEY_RIGHT_CONTROL);
    bool shiftDown = Input::IsKeyDown(KEY_LEFT_SHIFT) || Input::IsKeyDown(KEY_RIGHT_SHIFT);

    if (ctrlDown) {
        if (Input::IsKeyPressed(KEY_EQUAL)) uiScale += 0.10f;
        if (Input::IsKeyPressed(KEY_MINUS)) uiScale -= 0.10f;
        if (Input::IsKeyPressed(KEY_N)) CreateNewScene();
        if (Input::IsKeyPressed(KEY_O)) OpenSceneDialog();
        
        // Only allow save shortcuts when:
        // - There is an active scene
        // - We are in edit mode (not playing)
        // - There are unsaved changes (for regular save, not save as)
        bool hasActiveScene = false;
        if (m_sceneManager) {
            size_t idx = m_sceneManager->GetActiveIndex();
            hasActiveScene = (idx != static_cast<size_t>(-1));
        }

        bool isInEditMode = true;
        if (m_getEditorState) {
            isInEditMode = (m_getEditorState() == EditorState::Edit);
        }

        if (Input::IsKeyPressed(KEY_S) && hasActiveScene && isInEditMode) {
            if (shiftDown) SaveSceneAsDialog();
            else if (m_hasUnsavedChanges) SaveScene();
        }
    }
}

// -------------------------------------------------------------------------
// Internal Helpers
// -------------------------------------------------------------------------

// Creates a new Scene slot in the SceneManager and loads scene data from disk
void EditorFileMenu::_openScene(const std::string& path) {
    // Again we protect against a missing SceneManager pointer
    if (!m_sceneManager) return;

    size_t activeIdx = m_sceneManager->GetActiveIndex();
    const bool hasActive = (activeIdx != static_cast<size_t>(-1));

    // If opening the SAME scene, reload into the current slot to avoid world rebinding issues
    if (hasActive && (m_currentScenePath == path)) {
        std::vector<uint32_t> entityOrder;
        if (m_sceneManager->RestartScene(activeIdx, path, &entityOrder)) {
            m_sceneManager->SetActiveImmediate(activeIdx);
            if (m_hierarchyPanel) {
                m_hierarchyPanel->ClearUIState();
                m_hierarchyPanel->SetEntityOrder(entityOrder);
            }
            m_hasUnsavedChanges = false;
            if (m_getEditorState && m_getEditorState() != EditorState::Edit) {
                // Reloading while playing should not restore an old snapshot later.
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

    auto newScene = std::make_unique<Scenes::Scene>();

    // Register the scene with the SceneManager
    // Ownership is transferred via release so SceneManager now owns the Scene
    size_t idx = m_sceneManager->AddScene(newScene.release());

    // Ask the SceneManager to read the file and populate the scene
    std::vector<uint32_t> entityOrder;
    if (m_sceneManager->LoadScene(idx, path, &entityOrder)) {
        m_sceneManager->SetActive(idx);
        m_sceneManager->Update(); // Apply pending activation so editor UI uses the loaded scene immediately.
        if (m_hierarchyPanel) {
            m_hierarchyPanel->ClearUIState();
            m_hierarchyPanel->SetEntityOrder(entityOrder);
        }
        m_currentScenePath = path;
        m_hasUnsavedChanges = false;
        LOG_INFO("Opened scene: " << path);
    }
    else {
        LOG_ERROR("Failed to open scene: " << path);
        m_sceneManager->RemoveScene(idx);
    }
}

// Saves the currently active scene to the given file path
void EditorFileMenu::_saveSceneToFile(const std::string& path) {
    if (!m_sceneManager) return;

    size_t activeIdx = m_sceneManager->GetActiveIndex();

    if (activeIdx == static_cast<size_t>(-1)) {
        LOG_ERROR("No active scene to save");
        return;
    }

    // Rebuild entity order from hierarchy panel to preserve visual order
    if (m_hierarchyPanel) {
        m_hierarchyPanel->RebuildEntityOrder();
    }

    // Get entity order for serialization
    const std::vector<uint32_t>* entityOrder = nullptr;
    if (m_hierarchyPanel) {
        entityOrder = &m_hierarchyPanel->GetEntityOrder();
    }

    if (m_preSaveCallback) {
        // Give the editor a chance to flush external assets (tilemaps, etc.) before scene save.
        m_preSaveCallback(path);
    }

    // Save scene (symlink automatically keeps source in sync)
    LOG_INFO("Saving scene: " << path);
    if (!m_sceneManager->SaveScene(activeIdx, path, "Scene", "1.0", entityOrder)) {
        LOG_ERROR("Failed to save scene: " << path);
        return;
    }
    else {
        LOG_INFO("Successfully saved scene: " << path);
    }
}
