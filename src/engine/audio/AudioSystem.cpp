#include "../engine/audio/AudioSystem.h"
#include "../editor/AudioAssetLibrary.h"
#include "../engine/services/AudioService.h"
#include "audio/FmodAudioDevice.h"
#include <iostream>
#include "core/Logger.h"
#include <set>
#include "../engine/core/Application.h"
#include "../engine/services/OverlayService.h"

/*
    Update(dt)
    ----------
    Runtime audio update:

    For every entity with AudioSource + WorldTransform:
      - Resolve CueId -> clip (path) via AudioAssetLibrary
      - Ensure the cue is loaded into the audio device
      - If this entity has no active PlaybackHandle yet -> Play
      - If it does have one -> update volume/pitch every frame
      - If CueId becomes 0 or clip disappears -> Stop and clear handle
*/

AudioSystem::AudioSystem(ECS::World& world, Services::AudioService& audioService)
    : m_world(world)
    , m_audioService(audioService)
    , m_hasStarted(true) 
{
}

void AudioSystem::Update(float /*dt*/)
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

    // Get audio clip registry
    auto& lib = AudioAssetLibrary::Get();

    // Check if game is playing (for editor mode)
    bool isPlaying = _isGamePlaying();

    // Track which entities we've processed this frame
    std::unordered_set<ECS::Entity, ECS::EntityHash> processedEntities;

    // Process all entities with AudioSource
    m_world.Each<ECS::Components::AudioSource>(
        [&](ECS::Entity e, ECS::Components::AudioSource& src)
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
            const auto* clip = lib.FindById(src.CueId);
            if (!clip) {
                static std::set<uint32_t> s_warnedCues;
                if (s_warnedCues.find(src.CueId) == s_warnedCues.end()) {
                    LOG_WARNING("AudioSystem: Entity " << e.Index
                        << " has invalid CueId " << src.CueId);
                    s_warnedCues.insert(src.CueId);
                }
                _stopSound(e);
                return;
            }

            const std::string cueKey = clip->path;

            // ----------------------------------------------------------------
            // Load the sound if not already loaded
            // ----------------------------------------------------------------
            Audio::SoundParams params{};
            params.stream = false;
            params.is3D = src.Spatial3D;
            params.defaultVolume = src.Volume;

            if (!m_audioService.LoadCue(cueKey, clip->path, params)) {
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
                settings.volume = src.Volume;
                settings.pitch = src.Pitch;
                settings.loop = src.Loop;

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
            // Update existing playback (volume/pitch changes)
            // ----------------------------------------------------------------
            if (hasInstance) {
                Audio::PlaybackHandle handle = it->second;
                device->SetInstanceVolume(handle, src.Volume);
                device->SetInstancePitch(handle, src.Pitch);

                //// Update 3D position if needed
                //if (src.Spatial3D && m_world.Has<ECS::Components::WorldTransform>(e)) {
                //    auto& transform = m_world.Get<ECS::Components::WorldTransform>(e);
                //    Audio::Vec3 pos{ transform.Position.x, transform.Position.y, transform.Position.z };
                //    Audio::Vec3 vel{ 0, 0, 0 };
                //    device->SetInstancePosition(handle, pos, vel);
                //}
            }
        });

    // ----------------------------------------------------------------
    // Clean up sounds for entities that no longer have AudioSource
    // ----------------------------------------------------------------
    std::vector<ECS::Entity> toRemove;
    for (auto& [entity, handle] : m_activeSounds) {
        if (processedEntities.find(entity) == processedEntities.end()) {
            toRemove.push_back(entity);
        }
    }

    for (auto entity : toRemove) {
        _stopSound(entity);
    }
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

void AudioSystem::_stopSound(ECS::Entity entity)
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
    auto* app = Engine::CORE;
    if (!app) return true;

    auto* overlay = app->GetOverlayService();  // You'll need to expose this
    if (!overlay) return true;

    return overlay->IsGamePlaying();  // Uses your PlaybackControls!
}
