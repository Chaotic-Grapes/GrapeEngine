/* Start Header *****************************************************************/
/*!
\file   AudioSystem.cpp
\author Dalton Koh (100%)
\par    d.koh@digipen.edu
\brief
Implements the AudioSystem which manages audio playback in the ECS framework.

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

#include "ecs/systems/AudioSystem.h"
#include "services/AudioService.h"
#include "core/ProjectPaths.h"
#include "core/Logger.h"
#include <set>
#include <unordered_set>
#include "core/Application.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "ecs/StringTable.h"
#include <filesystem>

namespace {
    Audio::Bus ToBus(uint8_t raw) {
        const auto count = static_cast<uint8_t>(Audio::Bus::Count);
        if (raw >= count) {
            return Audio::Bus::SFX;
        }
        return static_cast<Audio::Bus>(raw);
    }
}

namespace ECS {

    AudioSystem::AudioSystem(Services::AudioService& audioService)
        : m_audioService{audioService}
        , m_hasStarted{true} 
    { }

    void AudioSystem::OnCreate(World& world) {
        (void)world;
        m_world = &world;
        // Ensure cue registry is populated for CueId resolution
        m_audioService.CueRegistry().Refresh(Engine::ProjectPaths::GetProjectRoot());

        // Subscribe to scene change events to clean up audio state
        Messaging::MessageSystem::Subscribe<Messaging::SceneChanged>([this](const Messaging::SceneChanged& msg) {
            // When scene changes, clear all active sounds and stop any lingering fades in the engine
            // This ensures the AudioEngine doesn't carry over fade state from the previous scene
            m_sceneUnloadInProgress = false;
            const bool allowCrossfade = m_allowCrossfadeOnUnload;
            m_allowCrossfadeOnUnload = false;
            m_activeSounds.clear();

            // Stop all fading sounds in the engine to prevent carryover
            if (!allowCrossfade) {
                if (auto* engine = m_audioService.Engine()) {
                    engine->StopAllFades();
                }
            }

            LOG_DEBUG("AudioSystem: Cleaned up audio state after scene change (from '"
                     << msg.OldScene << "' to '" << msg.NewScene << "')");
        });

        // Subscribe to scene changing events to prepare for audio transitions
        Messaging::MessageSystem::Subscribe<Messaging::SceneChanging>([this](const Messaging::SceneChanging&) {
            if (!Engine::CORE) {
                return;
            }

            auto& sceneMgr = Engine::CORE->GetSceneManager();
            float fadeDuration = 0.0f;
            bool allowCrossfade = false;
            if (sceneMgr.ConsumeNextAudioTransition(fadeDuration, allowCrossfade)) {
                OnSceneWillUnload(fadeDuration, allowCrossfade);
            }
        });

    }

    void AudioSystem::OnUpdate(World& world)
    {
        m_world = &world;
        // Get engine
        Audio::AudioEngine* engine = m_audioService.Engine();
        if (!engine) {
            static bool s_warningLogged = false;
            if (!s_warningLogged) {
                LOG_WARNING("AudioSystem::Update: No audio engine available");
                s_warningLogged = true;
            }
            return;
        }

        // Check if game is playing (for editor mode)
        bool isPlaying = _isGamePlaying();

        // Track which entities we've processed this frame
        std::unordered_set<Entity, EntityHash> processedEntities;

        // Process all entities with AudioSource
        world.Each<Components::AudioSource>(
            [&](Entity e, Components::AudioSource& src)
            {
                processedEntities.insert(e);

                // ----------------------------------------------------------------
                // Handle CueId = 0 (no audio assigned)
                // ----------------------------------------------------------------
                if (src.CueId == 0) {
                    _stopSound(e, world);
                    return;
                }

                // ----------------------------------------------------------------
                // Resolve cueId -> clip info
                // ----------------------------------------------------------------
                const auto* cueInfo = m_audioService.CueRegistry().FindById(src.CueId);
                if (!cueInfo && src.CuePathId) {
                    const std::string cuePathRaw = ECS::StringTable::Resolve(src.CuePathId);
                    const std::string cuePath = Audio::AudioCueRegistry::NormalizePath(cuePathRaw);
                    if (!cuePath.empty()) {
                        cueInfo = m_audioService.CueRegistry().FindByPath(cuePath);
                        if (!cueInfo) {
                            cueInfo = &m_audioService.CueRegistry().Register(cuePath);
                            LOG_INFO("AudioSystem: Registered cue from path '" << cuePath << "' (id=" << cueInfo->Id << ")");
                        }
                        src.CueId = cueInfo->Id;
                    }
                }
                if (!cueInfo) {
                    static std::set<uint32_t> s_warnedCues;
                    if (s_warnedCues.find(src.CueId) == s_warnedCues.end()) {
                        const std::string cuePath = src.CuePathId ? ECS::StringTable::Resolve(src.CuePathId) : std::string();
                        LOG_WARNING("AudioSystem: Entity " << e.Index
                            << " has invalid CueId " << src.CueId
                            << " (CuePath='" << cuePath << "')"
                            << " (audio file not found in assets/Audio)");
                        s_warnedCues.insert(src.CueId);
                    }
                    _stopSound(e, world);
                    return;
                }

                const std::string& cueKey = cueInfo->Path;

                // ----------------------------------------------------------------
                // Load the sound if not already loaded
                // ----------------------------------------------------------------
                Audio::SoundParams params{};
                params.Stream = false;
                params.Is3D = src.Spatial3D;
                params.DefaultVolume = src.Volume;

                if (!m_audioService.LoadCue(cueKey, cueKey, params)) {
                    static std::set<std::string> s_failedCues;
                    if (s_failedCues.find(cueKey) == s_failedCues.end()) {
                        LOG_ERROR("AudioSystem: Failed to load cue: " << cueKey);
                        s_failedCues.insert(cueKey);
                    }
                    return;
                }

                // ----------------------------------------------------------------
                // Check if we have an active instance
                // ----------------------------------------------------------------
                auto it = m_activeSounds.find(e);
                bool hasInstance = (it != m_activeSounds.end());
                if (hasInstance && !engine->IsHandleActive(it->second)) {
                    m_activeSounds.erase(it);
                    hasInstance = false;
                }
                if (m_sceneUnloadInProgress && !hasInstance) {
                    return;
                }

                // ----------------------------------------------------------------
                // Determine if this sound should be playing
                // ----------------------------------------------------------------
                bool shouldPlay = false;

                if (src.PlayOnStart) {
                    // PlayOnStart sounds only play when:
                    // 1. Game is in play mode
                    // 2. System has started (prevents playing during entity creation in editor)
                    shouldPlay = isPlaying && m_hasStarted;
                }
                else {
                    // Non-PlayOnStart sounds can be controlled manually
                    // For now, we don't auto-play these at all
                    // (You can add manual Play() API later if needed)
                    shouldPlay = false;
                }

                // ----------------------------------------------------------------
                // Handle starting/stopping based on game state
                // ----------------------------------------------------------------
                if (!isPlaying) {
                    // Game is paused/stopped - stop all sounds
                    if (hasInstance) {
						// false -> no fade when pausing/stopping
                        _stopSound(e, world);
                    }
                    return;
                }

                // ----------------------------------------------------------------
                // Start playback if needed
                // ----------------------------------------------------------------
                if (shouldPlay && !hasInstance) {
                    Audio::PlaySettings settings{};
                    const bool doFadeIn = src.EnableFadeIn && src.FadeInDuration > 0.0f;
                    settings.Volume = doFadeIn ? 0.0f : src.Volume;
                    settings.Pitch = src.Pitch;
                    settings.Loop = src.Loop;
                    settings.Spatial3D = src.Spatial3D;
                    settings.Pan = src.Pan;

                    Audio::PlaybackHandle handle = m_audioService.Play(cueKey, settings, ToBus(src.Bus));
                    if (handle) {
                        m_activeSounds[e] = handle;

                        // Check if fadein is enabled
                        if (doFadeIn) {
							_fadeInHandle(handle, src.FadeInDuration, src.Volume);
                            LOG_DEBUG("AudioSystem: Fade-in started (duration=" << src.FadeInDuration << "s)");
                        }
                        LOG_DEBUG("AudioSystem: Playback started (handle ID=" << handle.Id << ")");
                    }
                    else {
                        LOG_ERROR("AudioSystem: Failed to start playback for " << cueKey);
                    }
                    return;
                }

                // ----------------------------------------------------------------
                // Stop playback if it shouldn't be playing
                // ----------------------------------------------------------------
                if (!shouldPlay && hasInstance) {
                    _stopSound(e, world);
                    return;
                }

                // ----------------------------------------------------------------
                // Update existing playback parameters
                // ----------------------------------------------------------------
                if (hasInstance) {
                    Audio::PlaybackHandle handle = it->second;

                    // Only set volume directly if NOT currently fading
                    // Fading entities have their volume managed by the AudioEngine
                    if (!IsEntityFading(e)) {
                        engine->SetInstanceVolume(handle, src.Volume);
                    }

                    engine->SetInstancePitch(handle, src.Pitch);
                    if (!src.Spatial3D) {
                        engine->SetInstancePan(handle, src.Pan);
                    }

                    // Update 3D position if spatial audio is enabled
                    if (src.Spatial3D && world.Has<Components::WorldTransform>(e)) {
                        auto& transform = world.Get<Components::WorldTransform>(e);
                        // Extract translation from Matrix4x4: translation is stored in the
                        // last column (m03, m13, m23) per Matrix4x4::Translation implementation.
                        const auto& m = transform.Matrix;
                        Audio::Vec3 pos{ m.m03, m.m13, m.m23 };
                        Audio::Vec3 vel{ 0, 0, 0 };
                        engine->SetInstancePosition(handle, pos, vel);
                    }
                }
            });

        // ----------------------------------------------------------------
        // Clean up sounds for entities that no longer have AudioSource
        // ----------------------------------------------------------------
        std::vector<Entity> toRemove;
        for (auto& [entity, handle] : m_activeSounds) {
            if (processedEntities.find(entity) == processedEntities.end()) {
                toRemove.push_back(entity);
            }
        }

        for (auto entity : toRemove) {
            _stopSound(entity, world);
        }
    }

    void AudioSystem::OnDestroy(World& world) {
        (void)world;
        m_world = nullptr;
        // Stop all active sounds
        OnSceneStop();
        LOG_INFO("AudioSystem: Destroyed");
    }

    SystemMetadata AudioSystem::GetMetadata() const {
        ComponentAccessBuilder builder("Audio");
        // Note: AudioSystem reads AudioSource and WorldTransform components
        // but uses them through custom service lookups, not direct ECS iteration.
        // Declaring minimal access for dependency tracking.
        // Execution parameters
        builder.SetExecutionOrder(50);
        builder.SetGroup(SystemGroup::Update);
        builder.SetRunMode(SystemRunMode::PlayOnly);
        return builder.Build();
    }

    void AudioSystem::OnSceneStart()
    {
        m_hasStarted = true;
        m_sceneUnloadInProgress = false;
        m_allowCrossfadeOnUnload = false;
        LOG_DEBUG("AudioSystem: Scene started - PlayOnStart sounds will now play");
    }

    void AudioSystem::OnSceneStop()
    {
        m_hasStarted = false;
        m_sceneUnloadInProgress = false;
        m_allowCrossfadeOnUnload = false;

        // Stop all currently playing sounds
        for (auto& [entity, handle] : m_activeSounds) {
            m_audioService.Stop(handle, Audio::StopMode::Immediate);
        }
        m_activeSounds.clear();

        LOG_DEBUG("AudioSystem: Scene stopped - all sounds stopped");
    }

    void AudioSystem::_stopSound(Entity entity, World& world, bool allowFade)
    {
        auto it = m_activeSounds.find(entity);
        if (it == m_activeSounds.end()) return;

        // Check if entity has AudioSource with fade-out enabled
		if (allowFade && world.Has<Components::AudioSource>(entity)) {
            auto& src = world.Get<Components::AudioSource>(entity);
            if (src.EnableFadeOut && src.FadeOutDuration > 0.0f) {
                _fadeOutHandle(it->second, src.FadeOutDuration);
                m_activeSounds.erase(it);
                LOG_DEBUG("AudioSystem: Fade-out started for entity " << entity.Index
                         << " (duration=" << src.FadeOutDuration << "s)");
                return;
            }
        }

		// No fade-out; stop immediately
        m_audioService.Stop(it->second, Audio::StopMode::Immediate);
        m_activeSounds.erase(it);
        LOG_DEBUG("AudioSystem: Stopped sound for entity " << entity.Index);
    }

    bool AudioSystem::_isGamePlaying() const
    {
        // Engine always considers game as "playing"
        // Editor can override this behavior by managing AudioSystem directly
        return true;
    }

    void AudioSystem::_fadeInHandle(Audio::PlaybackHandle handle, float duration, float targetVolume) {
        if (!handle || duration <= 0.0f) {
            return;
        }
        if (auto* engine = m_audioService.Engine()) {
            engine->SetInstanceVolume(handle, 0.0f);
            engine->FadeInstance(handle, targetVolume, duration, false);
        }
    }

    void AudioSystem::_fadeOutHandle(Audio::PlaybackHandle handle, float duration) {
        if (!handle || duration <= 0.0f) {
            return;
        }
        if (auto* engine = m_audioService.Engine()) {
            engine->FadeInstance(handle, 0.0f, duration, true);
        }
    }

    void AudioSystem::OnSceneWillUnload(float fadeDuration, bool allowCrossfade) {
        m_sceneUnloadInProgress = true;
        m_allowCrossfadeOnUnload = allowCrossfade;

        if (m_activeSounds.empty()) {
            if (fadeDuration > 0.0f) {
                FadeOutAllAudio(fadeDuration);
            }
            return;
        }

        auto* engine = m_audioService.Engine();
        if (!engine) {
            OnSceneStop();
            return;
        }

        bool anyFadeStarted = false;
        for (auto& [entity, handle] : m_activeSounds) {
            float duration = fadeDuration;
            bool shouldFade = fadeDuration > 0.0f;

            if (m_world && m_world->Has<Components::AudioSource>(entity)) {
                auto& src = m_world->Get<Components::AudioSource>(entity);
                if (src.EnableFadeOut && src.FadeOutDuration > 0.0f) {
                    duration = src.FadeOutDuration;
                    shouldFade = true;
                }
            }

            if (shouldFade) {
                _fadeOutHandle(handle, duration);
                anyFadeStarted = true;
            }
            else {
                m_audioService.Stop(handle, Audio::StopMode::Immediate);
            }
        }

        if (allowCrossfade) {
            m_activeSounds.clear();
        }

        if (anyFadeStarted) {
            LOG_DEBUG("AudioSystem: Scene will unload - fading out audio");
        }
        else {
            LOG_DEBUG("AudioSystem: Scene will unload - stopping audio immediately");
        }
    }

    void AudioSystem::FadeOutAllAudio(float duration) {
        if (duration <= 0.0f) {
            LOG_WARNING("AudioSystem::FadeOutAllAudio called with duration <= 0, using immediate stop");
            for (auto& [entity, handle] : m_activeSounds) {
                m_audioService.Stop(handle, Audio::StopMode::Immediate);
            }
            m_activeSounds.clear();
            return;
        }

        if (auto* engine = m_audioService.Engine()) {
            engine->FadeOutAll(duration);
        }
        m_activeSounds.clear();

        LOG_DEBUG("AudioSystem: Fading out " << m_activeSounds.size()
                 << " sounds (duration=" << duration << "s)");
    }

    bool AudioSystem::HasActiveFadeOuts() const {
        return m_audioService.Engine() ? m_audioService.Engine()->HasActiveFadeOuts() : false;
    }

    float AudioSystem::GetMaxFadeOutRemaining() const {
        return m_audioService.Engine() ? m_audioService.Engine()->GetMaxFadeOutRemaining() : 0.0f;
    }

    bool AudioSystem::IsEntityFading(Entity entity) const {
        auto it = m_activeSounds.find(entity);
        if (it == m_activeSounds.end()) {
            return false;
        }
        return m_audioService.Engine() ? m_audioService.Engine()->IsHandleFading(it->second) : false;
    }
} // namespace ECS
