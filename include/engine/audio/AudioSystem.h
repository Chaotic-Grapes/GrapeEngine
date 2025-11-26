#pragma once

#include "ecs/World.h"
#include "services/AudioService.h"
#include <unordered_map>
#include "ecs/Components.h"
#include "audio/FmodAudioDevice.h"

/*
   AudioSystem
   -----------
   Runs every frame.

   Responsibilities:
   - For each entity with AudioSource:
       * Resolve CueId -> file path using AudioAssetLibrary
       * Lazily load that sound (only once)
       * Automatically play sounds with PlayOnStart=true
       * Update looping state
       * Update 3D position if Spatial3D=true and position component exists
   - Keep track of currently playing sound handles
*/
class AudioSystem
{
public:
    AudioSystem(ECS::World& world, Services::AudioService& audioService);

    // Called every frame to update audio
    void Update(float dt);

    // Called when the scene/game starts playing
    void OnSceneStart();

    // Called when the scene/game stops playing
    void OnSceneStop();

private:
    ECS::World& m_world;
    Services::AudioService& m_audioService;

    // Map entity -> playing instance handle
    std::unordered_map<ECS::Entity, Audio::PlaybackHandle, ECS::EntityHash> m_activeSounds;

    // Track if the scene has started (for PlayOnStart logic)
    bool m_hasStarted;

    // Helper methods
    void _stopSound(ECS::Entity entity);
    bool _isGamePlaying() const;
};
