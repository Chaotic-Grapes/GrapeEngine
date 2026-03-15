/* Start Header *****************************************************************/
/*!
\file   AudioSystem.cpp
\author Dalton Koh (100%)
\par    d.koh@digipen.edu
\brief
Implements the AudioSystem that manages ECS-driven audio playback.

Responsibilities:
- Process entities with AudioSource components
- Resolve CueId/CuePathId to audio cue paths
- Manage playback lifecycle (start/stop/update/cleanup)
- Apply per-instance runtime parameters (volume, pitch, pan, low-pass)
- Handle 3D spatial audio positioning updates
- support low pass filter effects for damage muffle
- Support PlayOnStart gating and one-shot tracking
- Support PlayOnStart functionality
- Coordinate fade-in/fade-out and scene transition behavior
- Route playback through audio buses

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
#include <unordered_map>
#include "core/Application.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "services/TimeSystem.h"
#include <algorithm>
#include "ecs/StringTable.h"
#include <filesystem>
#include <exception>
#include <fmod_dsp_effects.h>

namespace ECS {
    class AudioSystem;
}

namespace {
    // convert raw bus value to a valid enum
    Audio::Bus ToBus(uint8_t raw) {
        const auto count = static_cast<uint8_t>(Audio::Bus::Count);
        if (raw >= count) {
            return Audio::Bus::SFX;
        }
        return static_cast<Audio::Bus>(raw);
    }

    // Audio backend teardown can invalidate handles mid-frame; guard stop paths.
    void SafeStopAudio(Services::AudioService& audioService,
                       Audio::PlaybackHandle handle,
                       uint32_t entityIndex,
                       const char* context) {
        if (!handle) {
            return;
        }

        try {
            audioService.Stop(handle, Audio::StopMode::Immediate);
        }
        catch (const std::exception& ex) {
            LOG_ERROR("AudioSystem: Stop failed in " << context
                      << " for entity " << entityIndex
                      << " (handle=" << handle.Id << "): " << ex.what());
        }
        catch (...) {
            LOG_ERROR("AudioSystem: Stop failed in " << context
                      << " for entity " << entityIndex
                      << " (handle=" << handle.Id << "): unknown exception");
        }
    }

    struct AudioSystemSubscriptions {
        Messaging::SubscriptionHandle sceneChanged;
        Messaging::SubscriptionHandle sceneChanging;
    };

    std::unordered_map<const ECS::AudioSystem*, AudioSystemSubscriptions> g_audioSystemSubscriptions;
}

namespace ECS {

    // build audio system with required audio service
    AudioSystem::AudioSystem(Services::AudioService& audioService)
        : m_audioService{audioService}
        , m_hasStarted{true} 
    { }

    // initialize state and register scene event hooks
    void AudioSystem::OnCreate(World& world) {
        (void)world;
        m_world = &world;
        auto& subscriptions = g_audioSystemSubscriptions[this];

        if (subscriptions.sceneChanged.IsValid()) {
            Messaging::MessageSystem::Unsubscribe<Messaging::SceneChanged>(subscriptions.sceneChanged);
        }
        if (subscriptions.sceneChanging.IsValid()) {
            Messaging::MessageSystem::Unsubscribe<Messaging::SceneChanging>(subscriptions.sceneChanging);
        }

        // Ensure cue registry is populated before the first entity update.
        m_audioService.CueRegistry().Refresh(Engine::ProjectPaths::GetProjectRoot());

        // Build the master DSP node once per AudioSystem lifetime.
        _initializeMasterDsp();

        // Subscribe to scene change events to clean up audio state
        subscriptions.sceneChanged = Messaging::MessageSystem::Subscribe<Messaging::SceneChanged>([this](const Messaging::SceneChanged& msg) {
            // When scene changes, clear all active sounds and stop any lingering fades in the engine
            // This ensures the AudioEngine doesn't carry over fade state from the previous scene
            m_sceneUnloadInProgress = false;
            const bool allowCrossfade = m_allowCrossfadeOnUnload;
            m_allowCrossfadeOnUnload = false;
            if (allowCrossfade && m_crossfadeInDuration > 0.0f) {
                m_crossfadeInRemaining = m_crossfadeInDuration;
                m_crossfadeFadeInActive = true;
            } else {
                m_crossfadeInRemaining = 0.0f;
                m_crossfadeFadeInActive = false;
                m_crossfadeInDuration = 0.0f;
            }
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

        // subscribe to scene changing events to prepare for audio transitions
        subscriptions.sceneChanging = Messaging::MessageSystem::Subscribe<Messaging::SceneChanging>([this](const Messaging::SceneChanging&) {
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

    // process all audio source entities for this frame
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

        // update pending crossfade in timer
        if (m_crossfadeFadeInActive) {
            const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
            if (dt > 0.0f) {
                m_crossfadeInRemaining = std::max(0.0f, m_crossfadeInRemaining - dt);
                if (m_crossfadeInRemaining <= 0.0f) {
                    m_crossfadeFadeInActive = false;
                    m_crossfadeInDuration = 0.0f;
                }
            }
        }

        // Track which entities we've processed this frame
        std::unordered_set<Entity, EntityHash> processedEntities;

        // Process all entities with AudioSource
        world.Each<Components::AudioSource>(
            [&](Entity e, Components::AudioSource& src)
            {
                // mark this entity as processed this frame
                processedEntities.insert(e);

                // stop if no cue is assigned
                if (src.CueId == 0) {
                    m_playOnStartPlayedCue.erase(e);
                    _stopSound(e, world);
                    return;
                }

                // resolve cue id and optional cue path
                const auto* cueInfo = m_audioService.CueRegistry().FindById(src.CueId);
                if (!cueInfo && src.CuePathId) {
                    const std::string cuePathRaw = ECS::StringTable::Resolve(src.CuePathId);
                    std::string cuePath = cuePathRaw;

                    // If the path is not empty and not absolute, resolve it relative to project root
                    if (!cuePath.empty() && Engine::ProjectPaths::IsInitialized()) {
                        std::filesystem::path fsPath(cuePath);

                        // If the path is not absolute, resolve it relative to project root
                        if (!fsPath.is_absolute()) {
                            cuePath = Engine::ProjectPaths::ToAbsolutePath(cuePath);
                        }
                    }
                    cuePath = Audio::AudioCueRegistry::NormalizePath(cuePath);
                    if (!cuePath.empty()) {
                        cueInfo = m_audioService.CueRegistry().FindByPath(cuePath);

                        // If still not found, register it (this allows dynamic cues that aren't pre-registered)
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
                    m_playOnStartPlayedCue.erase(e);
                    _stopSound(e, world);
                    return;
                }

                const std::string& cueKey = cueInfo->Path;

                // build loading params from audio source settings
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

                // check for an existing active playback handle
                auto it = m_activeSounds.find(e);
                bool hasInstance = (it != m_activeSounds.end());
                if (hasInstance && !engine->IsHandleActive(it->second)) {
                    m_activeSounds.erase(it);
                    hasInstance = false;
                    if (src.PlayOnStart && !src.Loop) {
                        src.PlayOnStart = false;
                    }
                }
                if (m_sceneUnloadInProgress && !hasInstance) {
                    return;
                }

                // compute if this source should be playing now
                bool shouldPlay = false;

                if (!src.PlayOnStart || src.Loop) {
                    m_playOnStartPlayedCue.erase(e);
                }

                if (src.PlayOnStart) {
                    // allow one shot play on start once per cue
                    bool playOnStartEligible = true;
                    auto playedIt = m_playOnStartPlayedCue.find(e);
                    if (playedIt != m_playOnStartPlayedCue.end() && playedIt->second == src.CueId) {
                        playOnStartEligible = false;
                    }
                    shouldPlay = isPlaying && m_hasStarted && (src.Loop || playOnStartEligible || hasInstance);
                }
                else {
                    // Non-PlayOnStart sounds can be controlled manually
                    // For now, we don't auto-play these at all
                    // (can add manual Play() API later if needed)
                    shouldPlay = false;
                }

                // stop active sounds when game is not running
                if (!isPlaying) {
                    // Game is paused/stopped - stop all sounds
                    if (hasInstance) {
						// Stop current instance when gameplay is not running.
                        _stopSound(e, world);
                    }
                    return;
                }

                // start playback when needed and no instance exists
                if (shouldPlay && !hasInstance) {
                    Audio::PlaySettings settings{};
                    const bool crossfadeFadeIn = m_crossfadeFadeInActive && m_crossfadeInRemaining > 0.0f;
                    const bool hasSourceFadeIn = src.EnableFadeIn && src.FadeInDuration > 0.0f;
                    const bool doFadeIn = hasSourceFadeIn || crossfadeFadeIn;
                    const float fadeInDuration = doFadeIn
                        ? (hasSourceFadeIn && crossfadeFadeIn
                            ? std::max(src.FadeInDuration, m_crossfadeInRemaining)
                            : (hasSourceFadeIn ? src.FadeInDuration : m_crossfadeInRemaining))
                        : 0.0f;
                    settings.Volume = doFadeIn ? 0.0f : src.Volume;
                    settings.Pitch = src.Pitch;
                    settings.Loop = src.Loop;
                    settings.Spatial3D = src.Spatial3D;
                    settings.Pan = src.Pan;

                    Audio::PlaybackHandle handle = m_audioService.Play(cueKey, settings, ToBus(src.Bus));
                    if (handle) {
                        m_activeSounds[e] = handle;
                        const float lowPassGain = src.EnableLowPass ? src.LowPassGain : 1.0f;
                        engine->SetInstanceLowPassGain(handle, lowPassGain);
                        if (src.PlayOnStart && !src.Loop) {
                            m_playOnStartPlayedCue[e] = src.CueId;
                        }

                        // Apply optional fade-in after the channel starts.
                        if (doFadeIn && fadeInDuration > 0.0f) {
							_fadeInHandle(handle, fadeInDuration, src.Volume);
                            LOG_DEBUG("AudioSystem: Fade-in started (duration=" << fadeInDuration << "s)");
                        }
                        LOG_DEBUG("AudioSystem: Playback started (handle ID=" << handle.Id << ")");
                    }
                    else {
                        LOG_ERROR("AudioSystem: Failed to start playback for " << cueKey);
                    }
                    return;
                }

                // stop playback when it should no longer play
                if (!shouldPlay && hasInstance) {
                    _stopSound(e, world);
                    return;
                }

                // update runtime parameters for active playback
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

                    const float lowPassGain = src.EnableLowPass ? src.LowPassGain : 1.0f;
                    engine->SetInstanceLowPassGain(handle, lowPassGain);

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

        // collect stale handles for entities without audio source
        std::vector<Entity> toRemove;
        for (auto& [entity, handle] : m_activeSounds) {
            if (processedEntities.find(entity) == processedEntities.end()) {
                toRemove.push_back(entity);
            }
        }

        for (auto entity : toRemove) {
            _stopSound(entity, world);
            m_playOnStartPlayedCue.erase(entity);
        }
    }

    // shutdown audio state owned by this system
    void AudioSystem::OnDestroy(World& world) {
        (void)world;

        if (auto it = g_audioSystemSubscriptions.find(this); it != g_audioSystemSubscriptions.end()) {
            if (it->second.sceneChanged.IsValid()) {
                Messaging::MessageSystem::Unsubscribe<Messaging::SceneChanged>(it->second.sceneChanged);
            }
            if (it->second.sceneChanging.IsValid()) {
                Messaging::MessageSystem::Unsubscribe<Messaging::SceneChanging>(it->second.sceneChanging);
            }
            g_audioSystemSubscriptions.erase(it);
        }

        m_world = nullptr;
        // Stop all active playback owned by this system.
        OnSceneStop();

        // Detach/release DSP node before system teardown.
        _shutdownMasterDsp();
        LOG_INFO("AudioSystem: Destroyed");
    }

    // describe system metadata for scheduler
    SystemMetadata AudioSystem::GetMetadata() const {
        ComponentAccessBuilder builder("Audio");
        // AudioSystem reads AudioSource and WorldTransform components
        // but uses them through custom service lookups, not direct ECS iteration.
        // Declaring minimal access for dependency tracking.
        // Execution parameters
        builder.SetExecutionOrder(50);
        builder.SetGroup(SystemGroup::Update);
        builder.SetRunMode(SystemRunMode::PlayOnly);
        return builder.Build();
    }

    // reset start state when scene starts
    void AudioSystem::OnSceneStart()
    {
        // reset scene transition flags
        m_hasStarted = true;
        m_sceneUnloadInProgress = false;
        m_allowCrossfadeOnUnload = false;
        m_crossfadeInDuration = 0.0f;
        m_crossfadeInRemaining = 0.0f;
        m_crossfadeFadeInActive = false;
        m_playOnStartPlayedCue.clear();
        LOG_DEBUG("AudioSystem: Scene started - PlayOnStart sounds will now play");
    }

    // stop all active scene audio immediately
    void AudioSystem::OnSceneStop()
    {
        // clear scene transition flags
        m_hasStarted = false;
        m_sceneUnloadInProgress = false;
        m_allowCrossfadeOnUnload = false;
        m_crossfadeInDuration = 0.0f;
        m_crossfadeInRemaining = 0.0f;
        m_crossfadeFadeInActive = false;
        m_playOnStartPlayedCue.clear();

        // Stop all currently playing sounds
        for (auto& [entity, handle] : m_activeSounds) {
            SafeStopAudio(m_audioService, handle, entity.Index, "OnSceneStop");
        }
        m_activeSounds.clear();

        LOG_DEBUG("AudioSystem: Scene stopped - all sounds stopped");
    }

    // stop one entity sound with optional fade out
    void AudioSystem::_stopSound(Entity entity, World& world, bool allowFade)
    {
        // find active sound handle for this entity
        auto it = m_activeSounds.find(entity);
        if (it == m_activeSounds.end()) return;

        Audio::PlaybackHandle handle = it->second;

        // Fade-out decision can touch ECS state; keep it exception-safe for teardown.
        bool shouldFade = false;
        float fadeDuration = 0.0f;
        if (allowFade) {
            try {
                if (world.Has<Components::AudioSource>(entity)) {
                    auto& src = world.Get<Components::AudioSource>(entity);
                    if (src.EnableFadeOut && src.FadeOutDuration > 0.0f) {
                        shouldFade = true;
                        fadeDuration = src.FadeOutDuration;
                    }
                }
            }
            catch (const std::exception& ex) {
                LOG_ERROR("AudioSystem: Fade-out query failed in _stopSound for entity "
                          << entity.Index << ": " << ex.what());
            }
            catch (...) {
                LOG_ERROR("AudioSystem: Fade-out query failed in _stopSound for entity "
                          << entity.Index << ": unknown exception");
            }
        }

        if (shouldFade) {
            try {
                _fadeOutHandle(handle, fadeDuration);
                m_activeSounds.erase(it);
                LOG_DEBUG("AudioSystem: Fade-out started for entity " << entity.Index
                          << " (duration=" << fadeDuration << "s)");
                return;
            }
            catch (const std::exception& ex) {
                LOG_ERROR("AudioSystem: Fade-out failed in _stopSound for entity "
                          << entity.Index << ": " << ex.what());
            }
            catch (...) {
                LOG_ERROR("AudioSystem: Fade-out failed in _stopSound for entity "
                          << entity.Index << ": unknown exception");
            }
            // Fall through to an immediate stop attempt.
        }

        // No fade-out (or fade-out failed); stop immediately.
        SafeStopAudio(m_audioService, handle, entity.Index, "_stopSound");
        m_activeSounds.erase(it);
        LOG_DEBUG("AudioSystem: Stopped sound for entity " << entity.Index);
    }

    // report whether gameplay audio should run
    bool AudioSystem::_isGamePlaying() const
    {
        // Engine always considers game as "playing"
        // Editor can override this behavior by managing AudioSystem directly
        return true;
    }

    // initialize legacy master dsp state
    bool AudioSystem::_initializeMasterDsp() {
        // Device-owned bus DSP routing now owns the master low-pass node.
        m_masterLowPassDsp = nullptr;
        m_masterLowPassAttached = false;
        return m_audioService.Device() != nullptr;
    }

    // release legacy master dsp state
    void AudioSystem::_shutdownMasterDsp() {
        // Device-owned bus DSP routing handles lifetime now.
        m_masterLowPassDsp = nullptr;
        m_masterLowPassAttached = false;
    }

    // fade in one handle from zero to target volume
    void AudioSystem::_fadeInHandle(Audio::PlaybackHandle handle, float duration, float targetVolume) {
        // validate handle and duration
        if (!handle || duration <= 0.0f) {
            return;
        }
        // start fade on audio engine
        if (auto* engine = m_audioService.Engine()) {
            engine->SetInstanceVolume(handle, 0.0f);
            engine->FadeInstance(handle, targetVolume, duration, false);
        }
    }

    // fade out one handle to silence
    void AudioSystem::_fadeOutHandle(Audio::PlaybackHandle handle, float duration) {
        // validate handle and duration
        if (!handle || duration <= 0.0f) {
            return;
        }
        // start terminal fade on audio engine
        if (auto* engine = m_audioService.Engine()) {
            engine->FadeInstance(handle, 0.0f, duration, true);
        }
    }

    // prepare active sounds for scene unload transitions
    void AudioSystem::OnSceneWillUnload(float fadeDuration, bool allowCrossfade) {
        // mark unload in progress and store crossfade settings
        m_sceneUnloadInProgress = true;
        m_allowCrossfadeOnUnload = allowCrossfade;
        m_crossfadeInDuration = allowCrossfade ? fadeDuration : 0.0f;
        m_crossfadeInRemaining = 0.0f;
        m_crossfadeFadeInActive = false;

        if (m_activeSounds.empty()) {
            if (fadeDuration > 0.0f) {
                FadeOutAllAudio(fadeDuration);
            }
            return;
        }

        // fallback to full stop if engine is unavailable
        auto* engine = m_audioService.Engine();
        if (!engine) {
            OnSceneStop();
            return;
        }

        // fade or stop each active handle
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
                SafeStopAudio(m_audioService, handle, entity.Index, "OnSceneWillUnload");
            }
        }

        // clear handle map so next scene can start clean
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

    // fade out every active sound in this system
    void AudioSystem::FadeOutAllAudio(float duration) {
        // use immediate stop when duration is invalid
        if (duration <= 0.0f) {
            LOG_WARNING("AudioSystem::FadeOutAllAudio called with duration <= 0, using immediate stop");
            for (auto& [entity, handle] : m_activeSounds) {
                SafeStopAudio(m_audioService, handle, entity.Index, "FadeOutAllAudio");
            }
            m_activeSounds.clear();
            return;
        }

        // request global fade out on audio engine
        if (auto* engine = m_audioService.Engine()) {
            engine->FadeOutAll(duration);
        }
        m_activeSounds.clear();

        LOG_DEBUG("AudioSystem: Fading out " << m_activeSounds.size()
                 << " sounds (duration=" << duration << "s)");
    }

    // report if there are active terminal fade outs
    bool AudioSystem::HasActiveFadeOuts() const {
        return m_audioService.Engine() ? m_audioService.Engine()->HasActiveFadeOuts() : false;
    }

    // return the longest remaining fade out time
    float AudioSystem::GetMaxFadeOutRemaining() const {
        return m_audioService.Engine() ? m_audioService.Engine()->GetMaxFadeOutRemaining() : 0.0f;
    }

    // report if a tracked entity handle is fading
    bool AudioSystem::IsEntityFading(Entity entity) const {
        // look up the current handle for this entity
        auto it = m_activeSounds.find(entity);
        if (it == m_activeSounds.end()) {
            return false;
        }
        return m_audioService.Engine() ? m_audioService.Engine()->IsHandleFading(it->second) : false;
    }
} // namespace ECS
