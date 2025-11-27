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
#include "services/WindowManager.h"
#include "services/Input.h"
#include <filesystem>
#include <algorithm>
#include "serialization/ConfigurationSerializer.h"
#include <filesystem>

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

        // Show "Save Scene*" in BOLD when there are unsaved changes
        if (m_hasUnsavedChanges && m_boldFont) {
            ImGui::PushFont(m_boldFont);
            bool clicked = ImGui::MenuItem("Save Scene*", "Ctrl+S", false, hasActiveScene);
            ImGui::PopFont();
            if (clicked) SaveScene();
        }
        else {
            // Normal font when no unsaved changes
            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, hasActiveScene)) { SaveScene(); }
        }

        if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S", false, hasActiveScene)) { SaveSceneAsDialog(); }
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
        // TODO: Implement undo/redo functionality
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) {
            // Placeholder for undo
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) {
            // Placeholder for redo
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
    
    ProjectSettings& settings = Engine::CORE->GetProjectSettings();
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Project Settings", &m_showProjectSettings, ImGuiWindowFlags_NoCollapse)) {
        // Title
        ImGui::Text("Project Information");
        ImGui::Separator();
        
        char titleBuf[256];
        strncpy_s(titleBuf, settings.Title.c_str(), sizeof(titleBuf) - 1);
        if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
            settings.Title = titleBuf;
            m_projectSettingsDirty = true;
        }
        
        char versionBuf[64];
        strncpy_s(versionBuf, settings.Version.c_str(), sizeof(versionBuf) - 1);
        if (ImGui::InputText("Version", versionBuf, sizeof(versionBuf))) {
            settings.Version = versionBuf;
            m_projectSettingsDirty = true;
        }
        
        char sceneBuf[512];
        strncpy_s(sceneBuf, settings.StartupScene.c_str(), sizeof(sceneBuf) - 1);
        if (ImGui::InputText("Startup Scene", sceneBuf, sizeof(sceneBuf))) {
            settings.StartupScene = sceneBuf;
            m_projectSettingsDirty = true;
        }
        
        ImGui::Spacing();
        ImGui::Text("Window Settings");
        ImGui::Separator();
        
        if (ImGui::InputInt("Width", &settings.WindowSettings.Width)) {
            m_projectSettingsDirty = true;
        }
        if (ImGui::InputInt("Height", &settings.WindowSettings.Height)) {
            m_projectSettingsDirty = true;
        }
        if (ImGui::Checkbox("Fullscreen", &settings.WindowSettings.Fullscreen)) {
            m_projectSettingsDirty = true;
        }
        if (ImGui::Checkbox("VSync", &settings.WindowSettings.VSync)) {
            m_projectSettingsDirty = true;
        }
        
        ImGui::Spacing();
        ImGui::Text("Physics Settings");
        ImGui::Separator();
        
        if (ImGui::InputFloat("Gravity", &settings.Physics.Gravity)) {
            m_projectSettingsDirty = true;
        }
        if (ImGui::InputFloat("Time Step", &settings.Physics.TimeStep, 0.001f, 0.01f, "%.4f")) {
            m_projectSettingsDirty = true;
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Save/Cancel buttons
        const bool pushedSaveStyle = m_projectSettingsDirty;
        if (pushedSaveStyle) {
            ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SuccessButton);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SuccessButtonHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SuccessButtonActive);
        }

        if (ImGui::Button(m_projectSettingsDirty ? "Save*" : "Save", ImVec2(120, 0))) {
            std::string projectRoot = Engine::ProjectPaths::GetProjectRoot();
            if (Engine::CORE->SaveProjectSettings(projectRoot)) {
                m_projectSettingsDirty = false;
            }
        }

        if (pushedSaveStyle) {
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            if (m_projectSettingsDirty) {
                // TODO: Add unsaved changes warning
            }
            m_showProjectSettings = false;
        }
    }
    ImGui::End();
}

// Creates a brand new scene and makes it the active one in the SceneManager
// Does not show any dialog, just starts from a fresh "New Scene"
void EditorFileMenu::CreateNewScene() {
    // If we have no SceneManager bound we cannot do anything
    if (!m_sceneManager) return;

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
    ofn.hwndOwner = glfwGetWin32Window(WindowManager::GetMainWindow()->Handle());
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
    ofn.hwndOwner = glfwGetWin32Window(WindowManager::GetMainWindow()->Handle());
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
        // Only allow save shortcuts when there is an active scene
        bool hasActiveScene = false;
        if (m_sceneManager) {
            size_t idx = m_sceneManager->GetActiveIndex();
            hasActiveScene = (idx != static_cast<size_t>(-1));
        }

        if (Input::IsKeyPressed(KEY_S) && hasActiveScene) {
            if (shiftDown) SaveSceneAsDialog();
            else SaveScene();
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
        if (m_sceneManager->LoadScene(activeIdx, path, &entityOrder)) {
            m_sceneManager->SetActiveImmediate(activeIdx);
            if (m_hierarchyPanel) {
                m_hierarchyPanel->ClearUIState();
                m_hierarchyPanel->SetEntityOrder(entityOrder);
            }
            m_hasUnsavedChanges = false;
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
