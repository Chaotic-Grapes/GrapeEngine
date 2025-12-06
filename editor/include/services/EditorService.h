/**
 * @file EditorService.h
 * @author Foo Rui Qin
 * @date 2025
 * @brief Editor service managing the ImGui-based LevelEditor interface
 * 
 * This file defines the EditorService class which serves as a system-level wrapper
 * for editor UI functionality. EditorService manages:
 * - LevelEditor creation/update/render gating per active scene
 * - Audio system integration for editor monitoring
 * - Conditional compilation support for ImGui features
 * - Window management integration for UI rendering
 * - World reference propagation to keep editor views in sync
 * 
 * The EditorService inherits from Engine::IService and integrates with the engine's
 * service architecture, providing a clean interface for the ImGui-based editor.
 */

#ifndef EDITOR_EDITORSERVICE_H
#define EDITOR_EDITORSERVICE_H

#include "core/IService.h"
#include "audio/FmodAudioDevice.h"
#include <memory>

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
        explicit EditorService(Scenes::SceneManager& sceneManager) : IService("Editor Service"), m_sceneManager(sceneManager) {
            m_editorInstance = this;
            SetEnabled(false);
        }

#ifdef USE_IMGUI
        ~EditorService() override;
#endif

        void Initialize() override;
        void Update() override;
        void Render() override;
        void Terminate() override;

        void EnableLevelEditorForScene(Scenes::Scene* scene);
        void DisableLevelEditor();

        void SetAudio(Audio::FmodAudioDevice* device) { m_audioDevice = device; }
        void SetWorld(ECS::World* world);

        bool IsGamePlaying() const;
        bool IsStepRequested() const;
        void ClearStepRequest() const;

        static inline EditorService* Get() { return m_editorInstance; }

    private:
        Audio::FmodAudioDevice* m_audioDevice = nullptr;
        Scenes::SceneManager& m_sceneManager;
        ECS::World* m_world = nullptr;

        static inline EditorService* m_editorInstance = nullptr;

#ifdef USE_IMGUI
        std::unique_ptr<LevelEditor> m_levelEditor;
        bool m_initialized = false;

        // Tracks whether ImGui platform/renderer backends have been initialized
        bool m_backendInitialized = false;

        bool m_showLevelEditor = false;
        Scenes::Scene* m_levelEditorForScene = nullptr;
        bool m_pendingLevelEditorRebuild = false;
#endif
    };
}

#endif // EDITOR_EDITORSERVICE_H
