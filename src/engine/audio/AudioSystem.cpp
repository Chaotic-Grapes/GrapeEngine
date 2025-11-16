#include "AudioSystem.h"

AudioSystem::AudioSystem(ECS::World& world)
    : m_world(world)
{
}

/*
    Update(dt)
    ----------
    This is the heart of your audio pipeline during runtime.

    Steps:
    1. Iterate over all AudioSource components (and optionally WorldTransform)
    2. For each:
         - Resolve its CueId via AudioAssetLibrary
         - Ensure audio is loaded using AudioService
         - If PlayOnStart and NOT already playing -> play it
         - If looping is changed -> update FMOD channel
         - If Spatial3D -> push 3D position to FMOD
*/
void AudioSystem::Update(float dt)
{
    AudioAssetLibrary& lib = AudioAssetLibrary::Get();

    // Iterate every AudioSource in the world
    m_world.Each<Components::AudioSource, Components::WorldTransform>(
        [&](ECS::Entity e,
            Components::AudioSource& src,
            Components::WorldTransform& xform)
        {
            // Resolve sound from CueId
            const auto* clip = lib.FindById(src.CueId);

            if (!clip)
            {
                // No valid audio clip assigned -> stop if previously playing
                if (m_activeSounds.count(e))
                {
                    AudioService::Get().Stop(m_activeSounds[e]);
                    m_activeSounds.erase(e);
                }
                return;
            }

            AudioService& audio = AudioService::Get();

            // Lazily load this clip
            SoundHandle handle = audio.Load(clip->path);
            if (!handle.IsValid())
                return; // Could not load -> ignore

            // Start sound on first time (PlayOnStart)
            if (src.PlayOnStart && !m_activeSounds.count(e))
            {
                SoundHandle instance = audio.Play(handle, /*loop=*/src.Loop, src.Volume, src.Pitch);
                m_activeSounds[e] = instance;
            }

            // If already playing, update properties each frame
            if (m_activeSounds.count(e))
            {
                SoundHandle inst = m_activeSounds[e];

                audio.SetVolume(inst, src.Volume);
                audio.SetPitch(inst, src.Pitch);
                audio.SetLoop(inst, src.Loop);

                if (src.Spatial3D)
                {
                    // Convert world transform position to FMOD listener space
                    audio.Set3DPosition(inst, xform.Position.x, xform.Position.y, xform.Position.z);
                }

                // If sound finished playing and not looping -> erase
                if (!src.Loop && audio.IsStopped(inst))
                {
                    m_activeSounds.erase(e);
                }
            }
        }
    );
}
