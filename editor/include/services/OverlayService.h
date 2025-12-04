/**
 * @file OverlayService.h
 * @author Foo Rui Qin
 * @date 2025
 * @brief Overlay system bridging DebugUI and the ImGui-based LevelEditor
 * 
 * This file defines the OverlayService class which serves as a system-level wrapper
 * for editor and debug UI functionality. OverlayService manages:
 * - DebugUI lifecycle and ImGui backend integration
 * - LevelEditor creation/update/render gating per active scene
 * - Audio system integration for debug monitoring
 * - Conditional compilation support for ImGui features
 * - Window management integration for UI rendering
 * - World reference propagation to keep editor/debug views in sync
 * 
 * The OverlayService system inherits from Engine::ISystem and follows the ECS pattern,
 * providing a clean interface between the engine's system architecture and
 * the ImGui-based debug interface.
 */

#ifndef EDITOR_OVERLAYSERVICE_H
#define EDITOR_OVERLAYSERVICE_H

#include "core/IService.h"
#include <memory>
#include "audio/FmodAudioDevice.h"
#include <memory>

// Forward declarations
namespace Scenes { class SceneManager; class Scene; }
namespace ECS { class World; }

#ifdef USE_IMGUI
#include "services/DebugUI.h"
#include "LevelEditor.h"
#else
// Forward declarations for non-ImGui builds to avoid pulling editor headers
class DebugUI;
class LevelEditor;
#endif

namespace Services {
    class OverlayService final : public Engine::IService {
    public:
        explicit OverlayService(Scenes::SceneManager& sceneManager) : IService("Overlay Service"), m_sceneManager(sceneManager) {
            m_overlayInstance = this;
            SetEnabled(false);
        }

#ifdef USE_IMGUI
        ~OverlayService() override;
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

        static inline OverlayService* Get() { return m_overlay_instance; }

    private:
        Audio::FmodAudioDevice* m_audioDevice = nullptr;
        Scenes::SceneManager& m_sceneManager;
        ECS::World* m_world = nullptr;

        static inline OverlayService* m_overlay_instance = nullptr;

#ifdef USE_IMGUI
        std::unique_ptr<DebugUI> m_debugUI;
        std::unique_ptr<LevelEditor> m_levelEditor;
        bool m_initialized = false;
        bool m_showLevelEditor = false;
        Scenes::Scene* m_levelEditorForScene = nullptr;
        bool m_pendingLevelEditorRebuild = false;
#endif
    };
}

#endif // EDITOR_OVERLAYSERVICE_H
