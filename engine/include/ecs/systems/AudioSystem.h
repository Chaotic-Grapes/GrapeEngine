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

        /**
         * @brief Initialize the master low-pass DSP and prepare audio playback state.
         * @param world ECS world used to locate AudioSource components.
         */
        void OnCreate(World& world) override;

        /**
         * @brief Process AudioSource components and manage playback lifecycle for this frame.
         * @param world ECS world containing entities with AudioSource components.
         */
        void OnUpdate(World& world) override;

        /**
         * @brief Stop all sounds and release the master DSP and internal state.
         * @param world ECS world passed by the scheduler.
         */
        void OnDestroy(World& world) override;
        
        /**
         * @brief Return system metadata for scheduler registration.
         * @return SystemMetadata describing component access and execution order.
         */
        SystemMetadata GetMetadata() const override;

        /** @brief Run in the PostUpdate group after game logic completes. */
        SystemGroup GetSystemGroup() const override { return SystemGroup::PostUpdate; }

        /** @brief Only run during play mode; audio is silent in edit mode. */
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

        /**
         * @brief Stop the sound associated with an entity.
         * @param entity Entity whose audio playback to stop.
         * @param world ECS world containing the entity's AudioSource component.
         * @param allowFade True to use the configured fade-out duration; false for immediate stop.
         */
        void _stopSound(Entity entity, World& world, bool allowFade = true);

        /**
         * @brief Check whether the engine is currently in play mode.
         * @return True if the game simulation is active.
         */
        bool _isGamePlaying() const;

        /**
         * @brief Create the master low-pass DSP and insert it into the FMOD master channel group.
         * @return True if the DSP was created and attached successfully.
         */
        bool _initializeMasterDsp();

        /**
         * @brief Remove the master low-pass DSP from the FMOD graph and release its resources.
         */
        void _shutdownMasterDsp();

        /**
         * @brief Begin a fade-in on a playback handle toward a target volume.
         * @param handle Playback handle to fade in.
         * @param duration Fade duration in seconds.
         * @param targetVolume Destination linear gain value.
         */
        void _fadeInHandle(Audio::PlaybackHandle handle, float duration, float targetVolume);

        /**
         * @brief Begin a fade-out on a playback handle toward silence.
         * @param handle Playback handle to fade out.
         * @param duration Fade duration in seconds.
         */
        void _fadeOutHandle(Audio::PlaybackHandle handle, float duration);
    };
}

#endif
