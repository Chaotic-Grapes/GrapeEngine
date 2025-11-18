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

#include "../editor/EditorFileMenu.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// This define lets GLFW expose native Win32 window handles so we can
// pass the editor window as the owner of the file dialog
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "core/Logger.h"
#include "core/Application.h"
#include "scene/SceneManager.h"
#include "scene/Scene.h"
#include "services/WindowManager.h"
#include "services/Input.h"
#include <filesystem>
#include <algorithm>

// Default directory where scenes are stored on disk
// The Windows file dialogs will open here first
static constexpr const char* SCENE_DIR = "assets/scenes/";

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
void EditorFileMenu::RenderFileMenu(float& uiScale) {
    // Clamp UI scale so users can't shrink or enlarge the UI too much
    uiScale = std::clamp(uiScale, 0.75f, 2.0f);

    // Apply the new scale globally so all ImGui text and widgets follow it
    ImGui::GetIO().FontGlobalScale = uiScale;

    // File menu (New, Open, Save As, Exit)
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) { CreateNewScene(); }
        if (ImGui::MenuItem("Open Scene", "Ctrl+O")) { OpenSceneDialog(); }
        if (ImGui::MenuItem("Save Scene", "Ctrl+S")) { SaveScene(); }
        if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) { SaveSceneAsDialog(); }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) { if (Engine::CORE) { Engine::CORE->Close(); } }
        ImGui::EndMenu();
    }

    // View menu (UI scale display + zoom controls)
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
// Public Operations
// -------------------------------------------------------------------------

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
    ofn.lpstrInitialDir = SCENE_DIR;

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
    ofn.lpstrInitialDir = SCENE_DIR;
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
    }
#endif
}

void EditorFileMenu::SaveScene() {
#ifdef _WIN32
    if (!m_sceneManager) return;
    if (m_currentScenePath.empty()) { SaveSceneAsDialog(); return; }
    _saveSceneToFile(m_currentScenePath);
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
        if (Input::IsKeyPressed(KEY_S)) {
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

    // Allocate a new empty Scene that will receive the loaded data
    auto newScene = std::make_unique<Scenes::Scene>();

    // Register the scene with the SceneManager
    // Ownership is transferred via release so SceneManager now owns the Scene
    size_t idx = m_sceneManager->AddScene(newScene.release());

    // Ask the SceneManager to read the file and populate the scene
    if (m_sceneManager->LoadScene(idx, path)) {
        m_sceneManager->SetActive(idx);
        m_currentScenePath = path;
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

    // Query which scene is currently active
    size_t activeIdx = m_sceneManager->GetActiveIndex();

    // Some engines use -1 to mean "no active scene"
    // We compare against that sentinel after casting
    if (activeIdx == static_cast<size_t>(-1)) {
        LOG_ERROR("No active scene to save");
        return;
    }

    // Ask the SceneManager to serialize and write the scene to disk
    if (m_sceneManager->SaveScene(activeIdx, path)) {
        LOG_INFO("Saved scene: " << path);
    }
    else {
        LOG_ERROR("Failed to save scene: " << path);
    }
}
