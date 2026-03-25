/* Start Header *****************************************************************/
/*!
\file   EditorFileMenu.h
\author Foo Rui Qin (50%)
        Muhammad Nur Fadzly Bin Zulkifli (30%)
        Samantha Leong Sher Yen (20%)
\par    ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
        s.leong@digipen.edu
\date   11th March 2026

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
#include <cstdint>
#include <unordered_map>
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

    // Set symbols font for icon-only buttons
    void SetSymbolsFont(ImFont* symbolsFont) {
        m_symbolsFont = symbolsFont;
    }

    // Set hierarchy panel for entity order management during save/load
    void SetHierarchyPanel(HierarchyPanel* hierarchyPanel) {
        m_hierarchyPanel = hierarchyPanel;
    }

    // Set undo system for edit menu actions
    void SetUndoSystem(Editor::UndoSystem* undoSystem) {
        m_undoSystem = undoSystem;
    }

    // Set a getter to query current editor state (decouples FileMenu from Playback)
    void SetEditorStateGetter(std::function<EditorState()> getter) {
        m_getEditorState = std::move(getter);
    }

    // Called when the active scene is reloaded during play to clear playback snapshot
    void SetPlaybackSnapshotClearCallback(std::function<void()> callback) {
        m_clearPlaybackSnapshot = std::move(callback);
    }

    // Called before scene save to persist external assets (e.g. tilemaps)
    void SetPreSaveCallback(std::function<void(const std::string&)> callback) {
        m_preSaveCallback = std::move(callback);
    }

    // Request opening the project browser (project selection UI)
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

    // Opens a scene directly from a provided path (e.g., asset browser double-click)
    void OpenSceneFromPath(const std::string& path);

    // Shows a Save As dialog and writes the current scene to disk
    void SaveSceneAsDialog();

    // Saves to current scene path if known; otherwise falls back to Save As
    void SaveScene();

    // Expose the current scene path for editor systems
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

    // Expose dirty state for play-mode guards
    bool HasUnsavedChanges() const { return m_hasUnsavedChanges; }

    // -------------------------------------------------------------------------
    // Keyboard Shortcuts
    // -------------------------------------------------------------------------

    // Respond to keyboard shortcuts that trigger file menu actions like new open or save
    void HandleShortcuts(float& uiScale);

private:
    // Result of a single export step, including name, success flag and output message
    struct ExportStepResult {
        std::string Name;       // Display name of the export step
        bool Success = false;   // Whether the step completed successfully
        std::string Message;    // Human-readable result or error message
        std::string Output;     // Raw output captured from the step
    };

    // Single asset row shown in the Build Size Analyzer window
    struct BuildSizeAssetEntry {
        std::string RelativePath;   // Relative to project root so it is stable across machines
        std::uintmax_t SizeBytes = 0;
        bool Selected = true;
    };

    // -------------------------------------------------------------------------
    // Internal Helpers
    // -------------------------------------------------------------------------

    // Loads a scene from the provided filesystem path and sets it as active
    void _openScene(const std::string& path);

    // Serializes and writes the current scene to the given file path
    void _saveSceneToFile(const std::string& path);

    // Trigger a full project export, launching the export thread
    void _exportProject(const std::string& destinationOverride = {},
        const std::unordered_set<std::string>& selectedAssets = {});

    // Render the export summary popup showing per-step results
    void _renderExportSummaryPopup();

    // Render the Build Size Analyzer window and handle its controls
    void _renderBuildSizeAnalyzerWindow();

    // Rebuild the analyzer asset list from the active project
    void _refreshBuildSizeAnalyzerAssets();

    // Recalculate total selected size and selected-file count
    void _recomputeBuildSizeSelectionTotals();

    // Toggle all entries selected/unselected in one operation
    void _setAllBuildSizeSelections(bool selected);

    // Persist and restore analyzer selection state between window sessions
    void _loadBuildSizeSelectionCache();
    void _saveBuildSizeSelectionCache() const;

    // Human-friendly byte formatting used by the analyzer table/footer
    std::string _formatBytes(std::uintmax_t bytes) const;

    // Render the project settings modal for editing project-wide config
    void _renderProjectSettingsModal();

#ifdef _WIN32
    // Show a folder picker dialog and return the selected export destination path
    std::string _pickExportFolder();
#endif

    // Check if the export thread has finished and finalize export state
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

    // Optional callback to clear playback snapshot on in-place reload
    std::function<void()> m_clearPlaybackSnapshot;

    // Optional callback to sync external assets before scene serialization
    std::function<void(const std::string&)> m_preSaveCallback;

    // Optional callback to open the project browser
    std::function<void()> m_requestProjectBrowser;

    // Tracks last opened/saved path for direct Save
    std::string m_currentScenePath;

    // Whether the current scene has unsaved changes
    bool m_hasUnsavedChanges = false;

    ImFont* m_mainFont = nullptr;       // Main body font
    ImFont* m_boldFont = nullptr;       // Bold font for unsaved-changes asterisk
    ImFont* m_symbolsFont = nullptr;    // Symbols/icon font for icon-only buttons

    // Whether the project settings modal is currently open
    bool m_showProjectSettings = false;

    // Whether project settings have been modified but not yet saved
    bool m_projectSettingsDirty = false;

    // Build Size Analyzer visibility and data cache
    bool m_showBuildSizeAnalyzer = false;
    bool m_buildSizeNeedsRefresh = true;
    bool m_buildSizeSelectionLoaded = false;
    std::vector<BuildSizeAssetEntry> m_buildSizeAssets;
    std::unordered_map<std::string, bool> m_buildSizeSelectionCache;
    std::uintmax_t m_buildSizeSelectedBytes = 0;
    std::size_t m_buildSizeSelectedCount = 0;
    std::string m_buildSizeStatusMessage;
    bool m_buildSizeStatusIsError = false;

public:

    // -------------------------------------------------------------------------
    // Export State
    // -------------------------------------------------------------------------

    bool m_exportRequested = false;                     // True if an export has been requested this frame
    bool m_openExportSummary = false;                   // True if the export summary popup should open
    std::string m_exportDestination;                    // Target folder path for the export output
    std::vector<ExportStepResult> m_exportResults;      // Per-step results from the last export run
    std::thread m_exportThread;                         // Background thread running the export pipeline
    std::mutex m_exportMutex;                           // Guards shared export state across threads
    std::atomic<bool> m_exportInProgress{ false };      // True while the export thread is running
    std::atomic<bool> m_exportDone{ false };            // True once the export thread has completed
    std::atomic<int> m_exportCurrentStep{ -1 };         // Index of the currently executing export step
    std::vector<std::string> m_exportStepNames;         // Ordered display names for each export step
};

#endif