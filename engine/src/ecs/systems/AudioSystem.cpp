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

namespace {
    // Simple CueId -> Path cache
    std::unordered_map<uint32_t, std::string> g_cueCache;
    bool g_cacheBuilt = false;

    // Build the cache by scanning audio folder once
    void BuildCueCache(const std::string& audioRoot = "assets/Audio") {
        if (g_cacheBuilt) return;

        namespace fs = std::filesystem;

        if (!fs::exists(audioRoot) || !fs::is_directory(audioRoot)) {
            LOG_WARNING("Audio folder not found: " << audioRoot);
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

        LOG_INFO("AudioSystem: Cached " << g_cueCache.size() << " audio files");
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
}

namespace ECS {

AudioSystem::AudioSystem(Services::AudioService& audioService)
    : m_audioService(audioService)
    , m_hasStarted(true) 
{
}

void AudioSystem::OnCreate(World& world) {
    (void)world;
    // Build audio cache on first creation
    BuildCueCache();
    LOG_INFO("AudioSystem: Initialized");
}

void AudioSystem::OnUpdate(World& world, float /*dt*/)
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
                _stopSound(e);
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
                _stopSound(e);
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
                    _stopSound(e);
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

                LOG_DEBUG("AudioSystem: Starting playback for entity " << e.Index
                    << " cue: " << cueKey
                    << " (loop=" << src.Loop
                    << ", vol=" << src.Volume
                    << ", pitch=" << src.Pitch << ")");

                Audio::PlaybackHandle handle = m_audioService.Play(cueKey, settings);
                if (handle) {
                    m_activeSounds[e] = handle;
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
                _stopSound(e);
                return;
            }

            // ----------------------------------------------------------------
            // Update existing playback parameters
            // ----------------------------------------------------------------
            if (hasInstance) {
                Audio::PlaybackHandle handle = it->second;
                device->SetInstanceVolume(handle, src.Volume);
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
        _stopSound(entity);
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
    // Execution order: run after gameplay logic
    builder.SetExecutionOrder(50);
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

    LOG_DEBUG("AudioSystem: Scene stopped - all sounds stopped");
}

void AudioSystem::_stopSound(Entity entity)
{
    auto it = m_activeSounds.find(entity);
    if (it != m_activeSounds.end()) {
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

}
