/* Start Header *****************************************************************/
/*!
\file   AudioSystem.h
\author Dalton Koh (100%)
\par    d.koh@digipen.edu
\brief
Declares the AudioSystem which manages audio playback in the ECS framework.

Responsibilities:
- Process entities with AudioSource component
- Resolve CueId to audio file paths
- Manage audio playback lifecycle (start/stop/update)
- Handle 3D spatial audio positioning
- Support PlayOnStart functionality

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include "ecs/ISystem.h"
#include "ecs/ComponentAccessAttribute.h"
#include "ecs/World.h"
#include "services/AudioService.h"
#include "audio/FmodAudioDevice.h"
#include <unordered_map>

namespace ECS {
    /**
     * @brief System for managing audio playback
     * Executes in PostUpdate phase with executionOrder=50
     */
    class AudioSystem : public ISystem {
    public:
        /**
         * @brief Construct audio system
         * @param audioService Reference to audio service (must outlive this system)
         */
        AudioSystem(Services::AudioService& audioService);
        ~AudioSystem() override = default;

        // ISystem interface
        void OnCreate(World& world) override;
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override;
        
        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::PostUpdate; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::PlayOnly; }

        /**
         * @brief Called when scene starts playing (editor play mode)
         * Enables PlayOnStart audio to begin
         */
        void OnSceneStart();

        /**
         * @brief Called when scene stops (editor stop mode)
         * Stops all active audio
         */
        void OnSceneStop();

    private:
        Services::AudioService& m_audioService;

        // Fade State
        struct FadeState {
            float CurrentVolume = 0.0f;
            float TargetVolume = 1.0f;
            float FadeDuration = 1.0f;
            float ElapsedTime = 0.0f;
            bool IsFadingIn = true;
        };

        // Map entity -> playing audio handle
        std::unordered_map<Entity, Audio::PlaybackHandle, EntityHash> m_activeSounds;
        // Map entity -> fade states stored
        std::unordered_map<Entity, FadeState, EntityHash> m_activeFades;

        // Track if scene has started (for PlayOnStart logic)
        bool m_hasStarted = false;

        // Helper methods
        void _stopSound(Entity entity, World& world, bool allowFade = true);
        bool _isGamePlaying() const;

        // Fade helper methods
		void _startFadeIn(Entity entity, Audio::PlaybackHandle handle, float duration, float targetVolume);
		void _startFadeOut(Entity entity, float duration);
        void _startFadeOutByHandle(Audio::PlaybackHandle handle, float duration);
		void _updateFades(World& world, float deltaTime);
        bool _hasActiveFadeOuts() const;
        float _getMaxFadeOutRemaining() const;
    };
}

#endif
