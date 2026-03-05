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

#include "Export.h"
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
    class GRAPEENGINE_API AudioSystem : public ISystem {
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

        /**
         * @brief Called when scene is about to unload (before transition)
         * Initiates fade-out for all active audio if fade is enabled
         * @param fadeDuration Duration of fade-out in seconds (0 for immediate stop)
         */
        void OnSceneWillUnload(float fadeDuration = 0.0f, bool allowCrossfade = false);

        /**
         * @brief Fade out all currently playing audio
         * @param duration Duration of fade-out in seconds
         */
        void FadeOutAllAudio(float duration);

        /**
         * @brief Check if any audio is currently fading out
         * @return true if at least one sound is fading out
         */
        bool HasActiveFadeOuts() const;

        /**
         * @brief Get the maximum remaining fade-out time
         * @return Maximum remaining fade time in seconds (0 if no fade-outs active)
         */
        float GetMaxFadeOutRemaining() const;

        /**
         * @brief Check if a specific entity's audio is currently fading
         * @param entity Entity to check
         * @return true if entity is currently fading
         */
        bool IsEntityFading(Entity entity) const;

    private:
        Services::AudioService& m_audioService;

        // Map entity -> playing audio handle
        std::unordered_map<Entity, Audio::PlaybackHandle, EntityHash> m_activeSounds;
        // Track PlayOnStart cues that have already fired (entity -> cue id).
        std::unordered_map<Entity, uint32_t, EntityHash> m_playOnStartPlayedCue;

        // Cached world pointer for fade-outs during scene transitions
        World* m_world = nullptr;

        // Track if scene has started (for PlayOnStart logic)
        bool m_hasStarted = false;

        // Guard against restarting audio while a scene unload transition is pending
        bool m_sceneUnloadInProgress = false;
        bool m_allowCrossfadeOnUnload = false;

        // Crossfade-in support for new scene audio
        float m_crossfadeInDuration = 0.0f;
        float m_crossfadeInRemaining = 0.0f;
        bool m_crossfadeFadeInActive = false;
        
        // Master low-pass DSP inserted into FMOD's master channel group.
        // Owned by AudioSystem and released in _shutdownMasterDsp().
        FMOD::DSP* m_masterLowPassDsp = nullptr;
        // Tracks whether the DSP has been inserted into the master chain.
        bool m_masterLowPassAttached = false;

        // Generic helpers
        void _stopSound(Entity entity, World& world, bool allowFade = true);
        bool _isGamePlaying() const;

        // DSP lifecycle helpers (called from OnCreate / OnDestroy)
        // Creates low-pass DSP and adds it to FMOD master group.
        bool _initializeMasterDsp();
        // Removes low-pass DSP from FMOD graph and releases it.
        void _shutdownMasterDsp();

        // Fade helper methods (delegated to AudioEngine)
        void _fadeInHandle(Audio::PlaybackHandle handle, float duration, float targetVolume);
        void _fadeOutHandle(Audio::PlaybackHandle handle, float duration);
    };
}

#endif
