/* Start Header *****************************************************************/
/*!
\file   EditorFileMenu.h
\author Foo Rui Qin    (100%)
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
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <imgui.h>
#include "serialization/ConfigurationSerializer.h"
#include "EditorState.h"

// Forward declarations
namespace Scenes { class SceneManager; }
namespace Editor { class UndoSystem; }
class HierarchyPanel;
#include <functional>


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
    
    // Set symbols font for icon-only buttons.
    void SetSymbolsFont(ImFont* symbolsFont) {
        m_symbolsFont = symbolsFont;
    }

    // Set hierarchy panel for entity order management during save/load
    void SetHierarchyPanel(HierarchyPanel* hierarchyPanel) {
        m_hierarchyPanel = hierarchyPanel;
    }

    // Set undo system for edit menu actions.
    void SetUndoSystem(Editor::UndoSystem* undoSystem) {
        m_undoSystem = undoSystem;
    }

    // Set a getter to query current editor state (decouples FileMenu from Playback)
    void SetEditorStateGetter(std::function<EditorState()> getter) {
        m_getEditorState = std::move(getter);
    }

    // Called when the active scene is reloaded during play to clear playback snapshot.
    void SetPlaybackSnapshotClearCallback(std::function<void()> callback) {
        m_clearPlaybackSnapshot = std::move(callback);
    }

    // Called before scene save to persist external assets (e.g., tilemaps).
    void SetPreSaveCallback(std::function<void(const std::string&)> callback) {
        m_preSaveCallback = std::move(callback);
    }

    // Request opening the project browser (project selection UI).
    void SetProjectBrowserRequestCallback(std::function<void()> callback) {
        m_requestProjectBrowser = std::move(callback);
    }

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    // Draws the "File" dropdown menu in the menu bar
    void RenderFileMenu();

    // Draws the "Edit" dropdown menu with undo/redo and project settings
    void RenderEditMenu();

    // Draws the "View" dropdown menu with UI scale controls
    void RenderViewMenu(float& uiScale);

    // -------------------------------------------------------------------------
    // Public Operations (callable from keyboard shortcuts)
    // -------------------------------------------------------------------------

    // Creates a blank scene and replaces the current active scene
    void CreateNewScene();

    // Shows a file dialog allowing the user to pick a .scene file to load
    void OpenSceneDialog();
    // Opens a scene directly from a provided path (e.g., asset browser double-click).
    void OpenSceneFromPath(const std::string& path);

    // Shows a Save As dialog and writes the current scene to disk
    void SaveSceneAsDialog();

    // Saves to current scene path if known; otherwise falls back to Save As
    void SaveScene();

    // Expose the current scene path for editor systems.
    const std::string& GetCurrentScenePath() const { return m_currentScenePath; }

    // Sync the current scene path when scenes are activated outside the file menu
    void SyncActiveScenePath(const std::string& path) {
        if (path.empty()) {
            return;
        }
        if (m_currentScenePath != path) {
            m_currentScenePath = path;
        }
    }

    // Mark the scene as having unsaved changes (only if it was loaded from a file)
    void MarkSceneDirty() {
        if (!m_currentScenePath.empty()) {
            m_hasUnsavedChanges = true;
        }
    }
    // Expose dirty state for play-mode guards.
    bool HasUnsavedChanges() const { return m_hasUnsavedChanges; }

    // -------------------------------------------------------------------------
    // Keyboard Shortcuts
    // -------------------------------------------------------------------------

    // Respond to keyboard shortcuts that trigger file menu actions like new open or save
    void HandleShortcuts(float& uiScale);

private:
    struct ExportStepResult {
        std::string Name;
        bool Success = false;
        std::string Message;
        std::string Output;
    };

    // -------------------------------------------------------------------------
    // Internal Helpers
    // -------------------------------------------------------------------------

    // Loads a scene from the provided filesystem path and sets it as active
    void _openScene(const std::string& path);

    // Serializes and writes the current scene to the given file path
    void _saveSceneToFile(const std::string& path);
    void _exportProject();
    void _renderExportSummaryPopup();
#ifdef _WIN32
    std::string _pickExportFolder();
#endif
    void _finalizeExportIfDone();

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    // Scene manager used to create, load, and save scenes
    Scenes::SceneManager* m_sceneManager = nullptr;
    // Hierarchy panel for entity order preservation
    HierarchyPanel* m_hierarchyPanel = nullptr;
    // Undo system for edit actions
    Editor::UndoSystem* m_undoSystem = nullptr;
    // Getter used to query current EditorState; optional (defaults to Edit)
    std::function<EditorState()> m_getEditorState = nullptr;
    // Optional callback to clear playback snapshot on in-place reload.
    std::function<void()> m_clearPlaybackSnapshot;
    // Optional callback to sync external assets before scene serialization.
    std::function<void(const std::string&)> m_preSaveCallback;
    // Optional callback to open the project browser.
    std::function<void()> m_requestProjectBrowser;
    // Tracks last opened/saved path for direct Save
    std::string m_currentScenePath;
    // Track whether current scene has unsaved changes
    bool m_hasUnsavedChanges = false;
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // Project settings editor state
    bool m_showProjectSettings = false;
    bool m_projectSettingsDirty = false;
    // Helper to render project settings modal
    void _renderProjectSettingsModal();

    // Optional callback to open the Game Configuration panel.
    std::function<void()> m_openGameConfigPanel;

public:
    void SetOpenGameConfigPanelCallback(std::function<void()> callback) { m_openGameConfigPanel = std::move(callback); }

    // Export state
    bool m_exportRequested = false;
    bool m_openExportSummary = false;
    std::string m_exportDestination;
    std::vector<ExportStepResult> m_exportResults;
    std::thread m_exportThread;
    std::mutex m_exportMutex;
    std::atomic<bool> m_exportInProgress{ false };
    std::atomic<bool> m_exportDone{ false };
    std::atomic<int> m_exportCurrentStep{ -1 };
    std::vector<std::string> m_exportStepNames;
    
};

#endif
