/* Start Header *****************************************************************/
/*!
\file   EditorFileMenu.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   16th November 2025

\brief
Declares the EditorFileMenu class which manages all File-menu actions in the
editor. This includes creating new scenes, opening existing scene files and
saving scenes through standard file dialogs.

The class provides the UI for the File menu tab and exposes operations that can
also be triggered by keyboard shortcuts. It forwards all load/save requests to
the SceneManager and ensures the editor state stays in sync with the currently
active scene.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_FILE_MENU_H
#define EDITOR_FILE_MENU_H

#include <string>
#include <imgui.h>

// Forward declaration so we don't need the full SceneManager here
namespace Scenes { class SceneManager; }
class HierarchyPanel;

// Handles File menu UI and scene file operations
class EditorFileMenu {
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    // Connects the File menu to the SceneManager so it can create, load and save scenes
    void Initialize(Scenes::SceneManager* sceneManager);
    
    // Set fonts for rendering bold asterisk
    void SetFonts(ImFont* mainFont, ImFont* boldFont) {
        m_mainFont = mainFont;
        m_boldFont = boldFont;
    }

    // Set hierarchy panel for entity order management during save/load
    void SetHierarchyPanel(HierarchyPanel* hierarchyPanel) {
        m_hierarchyPanel = hierarchyPanel;
    }

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    // Draws the "File" dropdown menu in the menu bar
    void RenderFileMenu(float& uiScale);

    // -------------------------------------------------------------------------
    // Public Operations (callable from keyboard shortcuts)
    // -------------------------------------------------------------------------

    // Creates a blank scene and replaces the current active scene
    void CreateNewScene();

    // Shows a file dialog allowing the user to pick a .scene file to load
    void OpenSceneDialog();

    // Shows a Save As dialog and writes the current scene to disk
    void SaveSceneAsDialog();

    // Saves to current scene path if known; otherwise falls back to Save As
    void SaveScene();

    // Mark the scene as having unsaved changes (only if it was loaded from a file)
    void MarkSceneDirty() {
        if (!m_currentScenePath.empty()) {
            m_hasUnsavedChanges = true;
        }
    }

    // -------------------------------------------------------------------------
    // Keyboard Shortcuts
    // -------------------------------------------------------------------------

    // Respond to keyboard shortcuts that trigger file menu actions like new open or save
    void HandleShortcuts(float& uiScale);

    // Build the current game project using the repository build script
    void BuildGame();

    // Build with explicit configuration and optionally run after build
    void BuildGameWithConfig(const std::string& config);

private:
    // Start the build process and capture output into the editor console
    void StartBuildProcess(const std::string& scriptPath, const std::string& config);

private:
    // -------------------------------------------------------------------------
    // Internal Helpers
    // -------------------------------------------------------------------------

    // Loads a scene from the provided filesystem path and sets it as active
    void _openScene(const std::string& path);

    // Serializes and writes the current scene to the given file path
    void _saveSceneToFile(const std::string& path);

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    // Scene manager used to create, load, and save scenes
    Scenes::SceneManager* m_sceneManager = nullptr;
    // Hierarchy panel for entity order preservation
    HierarchyPanel* m_hierarchyPanel = nullptr;
    // Tracks last opened/saved path for direct Save
    std::string m_currentScenePath;
    // Track whether current scene has unsaved changes
    bool m_hasUnsavedChanges = false;
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;

};

#endif