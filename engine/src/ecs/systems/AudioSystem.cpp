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
#include "services/TimeSystem.h"
#include <algorithm>
#include "ecs/StringTable.h"
#include <filesystem>
#include <fmod_dsp_effects.h>

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
        // Ensure cue registry is populated before the first entity update.
        m_audioService.CueRegistry().Refresh(Engine::ProjectPaths::GetProjectRoot());

        // Build the master DSP node once per AudioSystem lifetime.
        _initializeMasterDsp();

        // Subscribe to scene change events to clean up audio state
        Messaging::MessageSystem::Subscribe<Messaging::SceneChanged>([this](const Messaging::SceneChanged& msg) {
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
                processedEntities.insert(e);

                // ----------------------------------------------------------------
                // Handle CueId = 0 (no audio assigned)
                // ----------------------------------------------------------------
                if (src.CueId == 0) {
                    m_playOnStartPlayedCue.erase(e);
                    _stopSound(e, world);
                    return;
                }

                // ----------------------------------------------------------------
                // Resolve cueId -> clip info
                // ----------------------------------------------------------------
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
                    if (src.PlayOnStart && !src.Loop) {
                        src.PlayOnStart = false;
                    }
                }
                if (m_sceneUnloadInProgress && !hasInstance) {
                    return;
                }

                // ----------------------------------------------------------------
                // Determine if this sound should be playing
                // ----------------------------------------------------------------
                bool shouldPlay = false;

                if (!src.PlayOnStart || src.Loop) {
                    m_playOnStartPlayedCue.erase(e);
                }

                if (src.PlayOnStart) {
                    // PlayOnStart gating:
                    // 1) Game must be in play mode.
                    // 2) System must have received OnSceneStart() in this session.
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
                    // (You can add manual Play() API later if needed)
                    shouldPlay = false;
                }

                // ----------------------------------------------------------------
                // Handle starting/stopping based on game state
                // ----------------------------------------------------------------
                if (!isPlaying) {
                    // Game is paused/stopped - stop all sounds
                    if (hasInstance) {
						// Stop current instance when gameplay is not running.
                        _stopSound(e, world);
                    }
                    return;
                }

                // ----------------------------------------------------------------
                // Start playback if needed
                // ----------------------------------------------------------------
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
            m_playOnStartPlayedCue.erase(entity);
        }
    }

    void AudioSystem::OnDestroy(World& world) {
        (void)world;
        m_world = nullptr;
        // Stop all active playback owned by this system.
        OnSceneStop();

        // Detach/release DSP node before system teardown.
        _shutdownMasterDsp();
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
        m_crossfadeInDuration = 0.0f;
        m_crossfadeInRemaining = 0.0f;
        m_crossfadeFadeInActive = false;
        m_playOnStartPlayedCue.clear();
        LOG_DEBUG("AudioSystem: Scene started - PlayOnStart sounds will now play");
    }

    void AudioSystem::OnSceneStop()
    {
        m_hasStarted = false;
        m_sceneUnloadInProgress = false;
        m_allowCrossfadeOnUnload = false;
        m_crossfadeInDuration = 0.0f;
        m_crossfadeInRemaining = 0.0f;
        m_crossfadeFadeInActive = false;
        m_playOnStartPlayedCue.clear();

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

    bool AudioSystem::_initializeMasterDsp() {
        // Make init idempotent by cleaning up any previous DSP first.
        _shutdownMasterDsp();

        // Resolve FMOD device from service.
        auto* device = m_audioService.Device();
        if (!device) {
            LOG_WARNING("AudioSystem: Cannot initialize master DSP (audio device unavailable)");
            return false;
        }

        // Grab low-level FMOD objects needed to build the DSP graph.
        FMOD::System* system = device->GetSystem();
        FMOD::ChannelGroup* master = device->GetMasterChannelGroup();
        if (!system || !master) {
            LOG_WARNING("AudioSystem: Cannot initialize master DSP (FMOD system/master unavailable)");
            return false;
        }

        // Allocate a built-in low-pass DSP unit.
        FMOD::DSP* dsp = nullptr;
        FMOD_RESULT result = system->createDSPByType(FMOD_DSP_TYPE_LOWPASS, &dsp);
        if (result != FMOD_OK || !dsp) {
            LOG_ERROR("AudioSystem: Failed to create master low-pass DSP (FMOD result " << static_cast<int>(result) << ")");
            return false;
        }

        // Start near full-band so audio is unchanged until gameplay drives cutoff.
        constexpr float kDefaultCutoffHz = 22000.0f;
        dsp->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, kDefaultCutoffHz);

        // Insert at index 0 so it sits early in the master chain.
        result = master->addDSP(0, dsp);
        if (result != FMOD_OK) {
            LOG_ERROR("AudioSystem: Failed to attach master low-pass DSP (FMOD result " << static_cast<int>(result) << ")");
            // DSP was created but not attached; release immediately.
            dsp->release();
            return false;
        }

        // Store ownership state for runtime use and shutdown cleanup.
        m_masterLowPassDsp = dsp;
        m_masterLowPassAttached = true;
        LOG_INFO("AudioSystem: Master low-pass DSP created and attached");
        return true;
    }

    void AudioSystem::_shutdownMasterDsp() {
        // Nothing to do if DSP was never created.
        if (!m_masterLowPassDsp) {
            m_masterLowPassAttached = false;
            return;
        }

        // Remove DSP from FMOD graph first, then release object memory.
        if (m_masterLowPassAttached) {
            if (auto* device = m_audioService.Device()) {
                if (FMOD::ChannelGroup* master = device->GetMasterChannelGroup()) {
                    // Ignore return code here; release below is still required.
                    master->removeDSP(m_masterLowPassDsp);
                }
            }
            m_masterLowPassAttached = false;
        }

        // Release FMOD DSP object and clear owning pointer.
        m_masterLowPassDsp->release();
        m_masterLowPassDsp = nullptr;
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
        m_crossfadeInDuration = allowCrossfade ? fadeDuration : 0.0f;
        m_crossfadeInRemaining = 0.0f;
        m_crossfadeFadeInActive = false;

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
