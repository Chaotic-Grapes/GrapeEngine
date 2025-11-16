#include "AudioSystem.h"
#include "../editor/AudioAssetLibrary.h"
#include "../engine/services/AudioService.h"
#include "audio/FmodAudioDevice.h"
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
void AudioSystem::Update(float dt)
{
    AudioAssetLibrary& lib = AudioAssetLibrary::Get();

    // If the device isn't ready, do nothing
    Audio::FmodAudioDevice* device = m_audioService.Device();
    if (!device)
        return;

    m_world.Each<ECS::Components::AudioSource, ECS::Components::WorldTransform>(
        [&](ECS::Entity e,
            ECS::Components::AudioSource& src,
            ECS::Components::WorldTransform& xform)
        {
            // -----------------------------------------------------
            // 1) If no cue is assigned, make sure we stop any sound
            // -----------------------------------------------------
            if (src.CueId == 0)
            {
                auto it = m_activeSounds.find(e);
                if (it != m_activeSounds.end())
                {
                    // m_activeSounds stores Audio::PlaybackHandle,
                    // which matches AudioService::Stop(...)
                    m_audioService.Stop(it->second, Audio::StopMode::Immediate);
                    m_activeSounds.erase(it);
                }
                return;
            }

            // -----------------------------------------------------
            // 2) Resolve CueId -> clip info (contains path)
            // -----------------------------------------------------
            const auto* clip = lib.FindById(src.CueId);
            if (!clip)
            {
                // CueId is set but library has no matching clip
                // -> stop any playing instance
                auto it = m_activeSounds.find(e);
                if (it != m_activeSounds.end())
                {
                    m_audioService.Stop(it->second, Audio::StopMode::Immediate);
                    m_activeSounds.erase(it);
                }
                return;
            }

            // We’ll use the asset path as a unique cue key for FMOD
            std::string cueKey = clip->path;

            // -----------------------------------------------------
            // 3) Ensure this cue is loaded in the device
            // -----------------------------------------------------
            Audio::SoundParams params{};
            // params.is3D = true; // if you later want this to be 3D
            m_audioService.LoadCue(cueKey, clip->path, params);

            // -----------------------------------------------------
            // 4) Check if this entity already has a playing instance
            // -----------------------------------------------------
            auto it = m_activeSounds.find(e);
            bool hasInstance = (it != m_activeSounds.end());

            // -----------------------------------------------------
            // 5) If no instance yet -> start playback once
            // -----------------------------------------------------
            if (!hasInstance)
            {
                Audio::PlaySettings settings{};
                settings.volume = src.Volume;
                settings.pitch = src.Pitch;
                settings.loop = src.Loop;
                // settings.mode stays default (PlayMode::Single)

                Audio::PlaybackHandle handle = m_audioService.Play(cueKey, settings);
                if (handle)   // operator bool() => handle.Id != 0
                {
                    m_activeSounds[e] = handle;
                }
                return; // just started it; we'll update next frame
            }

            // -----------------------------------------------------
            // 6) Already playing -> update volume/pitch each frame
            // -----------------------------------------------------
            Audio::PlaybackHandle handle = it->second;

            device->SetInstanceVolume(handle, src.Volume);
            device->SetInstancePitch(handle, src.Pitch);

            // If later you want positional audio and have a way to
            // reconstruct position from WorldTransform.Matrix, you can
            // compute a Vec3 and call:
            //
            //   Audio::Vec3 pos{ x, y, z };
            //   Audio::Vec3 vel{ 0.0f, 0.0f, 0.0f };
            //   device->SetInstancePosition(handle, pos, vel);
            //
            // For now we keep it as simple 2D audio.
        }
    );
}
