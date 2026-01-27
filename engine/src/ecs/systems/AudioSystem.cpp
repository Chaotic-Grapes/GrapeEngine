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
#include "audio/FmodAudioDevice.h"
#include <iostream>
#include "core/Logger.h"
#include <set>
#include "core/Application.h"
#include "services/TimeSystem.h"

namespace {
    // Simple CueId -> Path cache
    std::unordered_map<uint32_t, std::string> g_cueCache;
    bool g_cacheBuilt = false;

	// TODO: Do not hardcode audio folder path. Use ProjectPaths and scan for all audio files
    // Build the cache by scanning audio folder once
    void BuildCueCache(const std::string& audioRoot = "assets/Audio") {
        if (g_cacheBuilt) return;

        namespace fs = std::filesystem;

        if (!fs::exists(audioRoot) || !fs::is_directory(audioRoot)) {
            g_cacheBuilt = true;
            return;
        }

        // Scan all audio files recursively
        for (auto& entry : fs::recursive_directory_iterator(audioRoot)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext != ".wav" && ext != ".ogg" && ext != ".mp3" && ext != ".flac")
                continue;

            // Normalize path (\ to /)
            std::string path = entry.path().string();
            std::replace(path.begin(), path.end(), '\\', '/');

            // Generate CueId from path hash
            uint32_t cueId = static_cast<uint32_t>(std::hash<std::string>{}(path));

            g_cueCache[cueId] = path;
        }

        g_cacheBuilt = true;
    }

    // Resolve CueId to path
    const std::string* ResolveCueId(uint32_t cueId) {
        // Build cache on first call
        if (!g_cacheBuilt) {
            BuildCueCache();
        }

        auto it = g_cueCache.find(cueId);
        if (it != g_cueCache.end()) {
            return &it->second;
        }

        return nullptr;
    }
} // anonymous namespace

namespace ECS {

    AudioSystem::AudioSystem(Services::AudioService& audioService)
        : m_audioService{audioService}
        , m_hasStarted{true} 
    { }

    void AudioSystem::OnCreate(World& world) {
        (void)world;
        // Build audio cache on first creation
        BuildCueCache();
    }

    void AudioSystem::OnUpdate(World& world)
    {
        // Get device
        Audio::FmodAudioDevice* device = m_audioService.Device();
        if (!device) {
            static bool s_warningLogged = false;
            if (!s_warningLogged) {
                LOG_WARNING("AudioSystem::Update: No audio device available");
                s_warningLogged = true;
            }
            return;
        }

        _updateFades(world, static_cast<float>(TimeSystem::Instance().GetDeltaTime()));

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
                const std::string* pathPtr = ResolveCueId(src.CueId);
                if (!pathPtr) {
                    static std::set<uint32_t> s_warnedCues;
                    if (s_warnedCues.find(src.CueId) == s_warnedCues.end()) {
                        LOG_WARNING("AudioSystem: Entity " << e.Index
                            << " has invalid CueId " << src.CueId
                            << " (audio file not found in assets/Audio)");
                        s_warnedCues.insert(src.CueId);
                    }
                    _stopSound(e, world);
                    return;
                }

                const std::string& cueKey = *pathPtr;

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
                const bool hasInstance = (it != m_activeSounds.end());

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
                    settings.Volume = src.Volume;
                    settings.Pitch = src.Pitch;
                    settings.Loop = src.Loop;

                    Audio::PlaybackHandle handle = m_audioService.Play(cueKey, settings);
                    if (handle) {
                        m_activeSounds[e] = handle;

                        // Check if fadein is enabled
                        if (src.EnableFadeIn && src.FadeInDuration > 0.0f) {
							_startFadeIn(e, handle, src.FadeInDuration, src.Volume);
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
                    // Fading entities have their volume managed by _updateFades()
                    if (!IsEntityFading(e)) {
                        device->SetInstanceVolume(handle, src.Volume);
                    }

                    device->SetInstancePitch(handle, src.Pitch);

                    // Update 3D position if spatial audio is enabled
                    if (src.Spatial3D && world.Has<Components::WorldTransform>(e)) {
                        auto& transform = world.Get<Components::WorldTransform>(e);
                        // Extract translation from Matrix4x4: translation is stored in the
                        // last column (m03, m13, m23) per Matrix4x4::Translation implementation.
                        const auto& m = transform.Matrix;
                        Audio::Vec3 pos{ m.m03, m.m13, m.m23 };
                        Audio::Vec3 vel{ 0, 0, 0 };
                        device->SetInstancePosition(handle, pos, vel);
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
        LOG_DEBUG("AudioSystem: Scene started - PlayOnStart sounds will now play");
    }

    void AudioSystem::OnSceneStop()
    {
        m_hasStarted = false;

        // Stop all currently playing sounds
        for (auto& [entity, handle] : m_activeSounds) {
            m_audioService.Stop(handle, Audio::StopMode::Immediate);
        }
        m_activeSounds.clear();
        m_activeFades.clear();

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
                _startFadeOut(entity, src.FadeOutDuration);
                LOG_DEBUG("AudioSystem: Fade-out started for entity " << entity.Index
                         << " (duration=" << src.FadeOutDuration << "s)");
            }
        }
		// No fade-out; stop immediately
        else {
            m_audioService.Stop(it->second, Audio::StopMode::Immediate);
            m_activeSounds.erase(it);
            LOG_DEBUG("AudioSystem: Stopped sound for entity " << entity.Index);
        }
    }

    bool AudioSystem::_isGamePlaying() const
    {
        // Engine always considers game as "playing"
        // Editor can override this behavior by managing AudioSystem directly
        return true;
    }

    void AudioSystem::_startFadeIn(Entity entity, Audio::PlaybackHandle handle, float duration, float targetVolume) {
        if (!handle || duration <= 0.0f) return;

        Audio::FmodAudioDevice* device = m_audioService.Device();
        if (!device) return;

        // Initialize fade state
        FadeState fade;
        fade.CurrentVolume = 0.0f;
        fade.TargetVolume = targetVolume;
        fade.FadeDuration = duration;
        fade.ElapsedTime = 0.0f;
        fade.IsFadingIn = true;

        // Set initial volume to 0
        device->SetInstanceVolume(handle, 0.0f);

        // Store fade state
        m_activeFades[entity] = fade;

        LOG_DEBUG("AudioSystem: Started fade-in for entity " << entity.Index
            << " (duration=" << duration << "s, target=" << targetVolume << ")");
    }

    void AudioSystem::_startFadeOut(Entity entity, float duration) {
        auto it = m_activeSounds.find(entity);
        if (it == m_activeSounds.end()) return;

        Audio::FmodAudioDevice* device = m_audioService.Device();
        if (!device) return;

        Audio::PlaybackHandle handle = it->second;

        // Get current volume
        float currentVol = 1.0f;  // Default if we can't get actual volume
        // Note: FMOD doesn't have a GetInstanceVolume, so we track it ourselves
        // or just use the component's volume

        // Initialize fade state
        FadeState fade;
        fade.CurrentVolume = currentVol;
        fade.TargetVolume = 0.0f;
        fade.FadeDuration = duration;
        fade.ElapsedTime = 0.0f;
        fade.IsFadingIn = false;

        // Store fade state
        m_activeFades[entity] = fade;

        LOG_DEBUG("AudioSystem: Started fade-out for entity " << entity.Index
            << " (duration=" << duration << "s)");
    }

    void AudioSystem::_startFadeOutByHandle(Audio::PlaybackHandle handle, float duration) {
        if (!handle || duration <= 0.0f) return;

        Audio::FmodAudioDevice* device = m_audioService.Device();
        if (!device) return;

        // Create synthetic entity ID from handle (won't conflict with real entities)
        Entity syntheticEntity;
        syntheticEntity.Index = static_cast<uint32_t>(handle.Id);
        syntheticEntity.Generation = 0xFFFF; // Mark as synthetic

        // Initialize fade state
        FadeState fade;
        fade.CurrentVolume = 1.0f; // Assume full volume
        fade.TargetVolume = 0.0f;
        fade.FadeDuration = duration;
        fade.ElapsedTime = 0.0f;
        fade.IsFadingIn = false;

        // Store fade state and handle using synthetic entity
        m_activeFades[syntheticEntity] = fade;
        m_activeSounds[syntheticEntity] = handle;
    }

    void AudioSystem::_updateFades(World& world, float deltaTime) {
        Audio::FmodAudioDevice* device = m_audioService.Device();
        if (!device) return;

        std::vector<Entity> toRemove;

        for (auto& [entity, fade] : m_activeFades) {
            // Get the playback handle
            auto soundIt = m_activeSounds.find(entity);
            if (soundIt == m_activeSounds.end()) {
                toRemove.push_back(entity);
                continue;
            }

            Audio::PlaybackHandle handle = soundIt->second;

            // Update fade progress
            fade.ElapsedTime += deltaTime;

            if (fade.ElapsedTime >= fade.FadeDuration) {
                // Fade complete
                device->SetInstanceVolume(handle, fade.TargetVolume);

                // If fading out, stop the sound
                if (!fade.IsFadingIn) {
                    _stopSound(entity, world);
                }

                toRemove.push_back(entity);
                LOG_DEBUG("AudioSystem: Fade complete for entity " << entity.Index);
            }
            else {
                // Calculate interpolated volume (linear fade)
                float t = fade.ElapsedTime / fade.FadeDuration;
                float newVolume = fade.CurrentVolume + (fade.TargetVolume - fade.CurrentVolume) * t;
                device->SetInstanceVolume(handle, newVolume);
            }
        }

        // Clean up completed fades
        for (Entity entity : toRemove) {
            m_activeFades.erase(entity);
        }
    }

    bool AudioSystem::_hasActiveFadeOuts() const {
        for (const auto& [entity, fade] : m_activeFades) {
            if (!fade.IsFadingIn) {
                return true;
            }
        }
        return false;
    }

    float AudioSystem::_getMaxFadeOutRemaining() const {
        float maxRemaining = 0.0f;

        for (const auto& [entity, fade] : m_activeFades) {
            if (!fade.IsFadingIn) {
                float remaining = fade.FadeDuration - fade.ElapsedTime;
                if (remaining > maxRemaining) {
                    maxRemaining = remaining;
                }
            }
        }

        return maxRemaining;
    }

    void AudioSystem::OnSceneWillUnload(float fadeDuration) {
        if (fadeDuration > 0.0f) {
            FadeOutAllAudio(fadeDuration);
            LOG_DEBUG("AudioSystem: Scene will unload - fading out all audio (duration="
                     << fadeDuration << "s)");
        }
        else {
            // Immediate stop without fade
            OnSceneStop();
        }
    }

    void AudioSystem::FadeOutAllAudio(float duration) {
        if (duration <= 0.0f) {
            LOG_WARNING("AudioSystem::FadeOutAllAudio called with duration <= 0, using immediate stop");
            OnSceneStop();
            return;
        }

        // Start fade-out for all currently playing sounds
        for (auto& [entity, handle] : m_activeSounds) {
            // Check if already fading
            auto fadeIt = m_activeFades.find(entity);
            if (fadeIt != m_activeFades.end()) {
                // Already fading - update to fade-out if it was fading in
                if (fadeIt->second.IsFadingIn) {
                    fadeIt->second.IsFadingIn = false;
                    fadeIt->second.TargetVolume = 0.0f;
                    fadeIt->second.FadeDuration = duration;
                    fadeIt->second.ElapsedTime = 0.0f;
                }
            }
            else {
                // Not fading - start new fade-out
                _startFadeOut(entity, duration);
            }
        }

        LOG_DEBUG("AudioSystem: Fading out " << m_activeSounds.size()
                 << " sounds (duration=" << duration << "s)");
    }

    bool AudioSystem::HasActiveFadeOuts() const {
        return _hasActiveFadeOuts();
    }

    float AudioSystem::GetMaxFadeOutRemaining() const {
        return _getMaxFadeOutRemaining();
    }

    bool AudioSystem::IsEntityFading(Entity entity) const {
        return m_activeFades.find(entity) != m_activeFades.end();
    }
} // namespace ECS
