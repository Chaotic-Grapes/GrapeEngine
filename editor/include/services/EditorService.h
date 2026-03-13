/* Start Header *****************************************************************/
/*!
\file   EditorService.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   11th March 2026
\brief
Editor service managing the ImGui-based LevelEditor interface.

This file defines the EditorService class which serves as a system-level wrapper
for editor UI functionality. EditorService manages:
- LevelEditor creation/update/render gating per active scene
- Audio system integration for editor monitoring
- Conditional compilation support for ImGui features
- Window management integration for UI rendering
- World reference propagation to keep editor views in sync

The EditorService inherits from Engine::IService and integrates with the engine's
service architecture, providing a clean interface for the ImGui-based editor.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_EDITORSERVICE_H
#define EDITOR_EDITORSERVICE_H

#include "core/IService.h"
#include "audio/FmodAudioDevice.h"
#include "EditorState.h"
#include "EditorStartupStage.h"
#include "EditorConfiguration.h"
#include "ProjectStartupUI.h"
#include <memory>
#include <functional>

// Forward declarations
namespace Scenes { class SceneManager; class Scene; }
namespace ECS { class World; }

#ifdef USE_IMGUI
#include "LevelEditor.h"
#else
// Forward declarations for non-ImGui builds to avoid pulling editor headers
class LevelEditor;
#endif

namespace Services {
    class EditorService final : public Engine::IService {
    public:
        // -------------------------------------------------------------------------
        // Lifecycle
        // -------------------------------------------------------------------------

        // Construct the editor service, registering it as the global instance
        explicit EditorService(Scenes::SceneManager& sceneManager) : IService("Editor Service"), m_sceneManager(sceneManager) {
            m_editorInstance = this;
            SetEnabled(false);
        }

#ifdef USE_IMGUI
        // Destroy the editor service and clean up ImGui resources
        ~EditorService() override;
#endif

        // Initialize ImGui backends and prepare the editor for use
        void Initialize() override;

        // Begin a new ImGui frame
        void BeginFrame();

        // Process per-frame editor logic including playback and shortcut handling
        void Update() override;

        // Render the editor UI and all registered panels
        void Render() override;

        // Submit ImGui draw data and finalize the frame
        void EndFrame();

        // Shut down the editor and release all ImGui resources
        void Terminate() override;

        // -------------------------------------------------------------------------
        // Scene Integration
        // -------------------------------------------------------------------------

        // Create and initialize the level editor for the given scene
        void EnableLevelEditorForScene(Scenes::Scene* scene);

        // Destroy the active level editor instance
        void DisableLevelEditor();

        // -------------------------------------------------------------------------
        // System Wiring
        // -------------------------------------------------------------------------

        // Set the audio device for editor audio monitoring
        void SetAudio(Audio::FmodAudioDevice* device) { m_audioDevice = device; }

        // Update the active ECS world reference across the editor
        void SetWorld(ECS::World* world);

        // Set a getter to query the current editor startup stage
        void SetStartupStageGetter(std::function<EditorStartupStage()> getter);

        // Set callbacks used by the project startup UI
        void SetProjectStartupCallbacks(const Editor::ProjectStartupCallbacks& callbacks);

        // Apply editor settings to the level editor and all panels
        void SetEditorSettings(EditorSettings* settings);

        // -------------------------------------------------------------------------
        // State Query
        // -------------------------------------------------------------------------

        // Return true if the editor is currently in Play or Step mode
        bool IsGamePlaying() const;

        // Return true if a single-step advance has been requested
        bool IsStepRequested() const;

        // Clear the pending step request after it has been consumed
        void ClearStepRequest() const;

        // Return the current playback state (Edit, Play, Pause, Step)
        EditorState GetPlaybackState() const;

        // -------------------------------------------------------------------------
        // Global Access
        // -------------------------------------------------------------------------

        // Return the global EditorService instance
        static inline EditorService* Get() { return m_editorInstance; }

        // Request that the project browser be opened
        void RequestProjectBrowser();

        // Request a full rebuild of the level editor on the next frame
        void RequestLevelEditorRebuild();

    private:
        // -------------------------------------------------------------------------
        // State
        // -------------------------------------------------------------------------

        Audio::FmodAudioDevice* m_audioDevice = nullptr;        // Audio device for editor monitoring
        Scenes::SceneManager& m_sceneManager;                   // Scene manager for scene load/save operations
        ECS::World* m_world = nullptr;                          // Active ECS world reference
        static inline EditorService* m_editorInstance = nullptr; // Global singleton instance

#ifdef USE_IMGUI
        std::unique_ptr<LevelEditor> m_levelEditor;        // Active level editor instance
        EditorSettings* m_editorSettings = nullptr;        // Editor settings for panels and config
        bool m_initialized = false;                        // Whether the editor has been fully initialized
        bool m_backendInitialized = false;                 // Whether ImGui platform/renderer backends are initialized
        bool m_showLevelEditor = false;                    // Whether the level editor should be rendered this frame
        Scenes::Scene* m_levelEditorForScene = nullptr;    // Scene the level editor was created for
        bool m_pendingLevelEditorRebuild = false;          // Whether a level editor rebuild is queued for next frame
        Editor::ProjectStartupUI m_projectStartupUI;       // Project browser and startup UI
#endif
    };
}

#endif // EDITOR_EDITORSERVICE_H