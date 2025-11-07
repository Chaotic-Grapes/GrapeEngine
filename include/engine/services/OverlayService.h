/* Start Header *****************************************************************/
/*!
\file   OverlayService.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Defines the Overlay class which serves as a system-level wrapper for managing
debug UI and level editor functionality.
*/
/* End Header *******************************************************************/

#ifndef OVERLAY_SERVICE_H
#define OVERLAY_SERVICE_H
#include <memory>
#include <string>
#include "core/IService.h"
#include "audio/FmodAudioDevice.h"
#include "services/DebugUI.h"
#include "services/UICommon.h"
#include "services/WindowManager.h"
#include "services/Input.h"
#include "scene/SceneManager.h"
#include "ecs/World.h"
#include "../editor/LevelEditor.h"

namespace Services {

    class OverlayService final : public Engine::IService {
    public:
        static inline OverlayService* m_overlayInstance = nullptr;

        explicit OverlayService(Scenes::SceneManager& sceneManager) : IService("Overlay Service"),
            m_sceneManager(sceneManager) {
            m_overlayInstance = this;
        }

        ~OverlayService() { m_overlayInstance = nullptr; }

        void Initialize() override;
        void Update() override;
        void Render() override;
        void Terminate() override;

        void SetAudio(Audio::FmodAudioDevice* device) { m_audioDevice = device; }
        void SetWorld(ECS::World* world) {
            if (m_world == world) return;
            m_world = world;
#ifdef USE_IMGUI
            // Propagate world update directly to avoid stale references
            if (m_levelEditor) {
                m_levelEditor->SetWorld(m_world);
            }
            if (m_debugUI) {
                m_debugUI->SetWorld(m_world);
            }
#endif
        }

        void EnableLevelEditorForScene(Scenes::Scene* scene);
        void DisableLevelEditor();

        bool IsGamePlaying() const { return m_levelEditor && m_levelEditor->IsPlaying(); }
        bool IsStepRequested() const { return m_levelEditor && m_levelEditor->IsStepRequested(); }
        void ClearStepRequest() const { if (m_levelEditor) m_levelEditor->ClearStepRequest(); }

        static inline OverlayService* Get() { return m_overlayInstance; }

    private:
        Scenes::SceneManager& m_sceneManager;
        Audio::FmodAudioDevice* m_audioDevice = nullptr;
        ECS::World* m_world = nullptr;

#ifdef USE_IMGUI
        std::unique_ptr<DebugUI> m_debugUI;
        std::unique_ptr<LevelEditor> m_levelEditor;
        bool m_initialized = false;
        bool m_showLevelEditor = false;
        Scenes::Scene* m_levelEditorForScene = nullptr; // nullptr means scene-less mode
        bool m_pendingLevelEditorRebuild = false;
#endif
    };

}

#endif