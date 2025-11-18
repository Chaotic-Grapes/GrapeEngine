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

// Handles File menu UI and scene file operations
class EditorFileMenu {
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    // Connects the File menu to the SceneManager so it can create, load and save scenes
    void Initialize(Scenes::SceneManager* sceneManager);

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

    // -------------------------------------------------------------------------
    // Keyboard Shortcuts
    // -------------------------------------------------------------------------

    // Respond to keyboard shortcuts that trigger file menu actions like new open or save
    void HandleShortcuts(float& uiScale);

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

};

#endif