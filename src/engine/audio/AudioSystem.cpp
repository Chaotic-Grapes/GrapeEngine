#include "../engine/audio/AudioSystem.h"
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
void AudioSystem::Update(float /* dt if used*/)
{
    AudioAssetLibrary& lib = AudioAssetLibrary::Get();

    // If the device isn't ready, do nothing
    Audio::FmodAudioDevice* device = m_audioService.Device();
    if (!device)
        return;

    m_world.Each<ECS::Components::AudioSource, ECS::Components::WorldTransform>(
        [&](ECS::Entity e,
            ECS::Components::AudioSource& src,
            ECS::Components::WorldTransform& /*xform*/)
        {
           // If no que make sure there isnt a clip playing
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

            // CueID-> for path resolution
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

            // Use the asset path as a unique cue key for FMOD
            std::string cueKey = clip->path;

            // Cue is loaded
            Audio::SoundParams params{};
            // params.is3D = true; // if you later want this to be 3D
            m_audioService.LoadCue(cueKey, clip->path, params);

            // Check if entity has a playing soundclip
            auto it = m_activeSounds.find(e);
            bool hasInstance = (it != m_activeSounds.end());

            // If no setplayback
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

            // IF playing -> set vol/pitch
            Audio::PlaybackHandle handle = it->second;

            device->SetInstanceVolume(handle, src.Volume);
            device->SetInstancePitch(handle, src.Pitch);

        }
    );
}
