#pragma once

#include "ecs/World.h"
#include "../editor/AudioAssetLibrary.h"
#include "services/AudioService.h"
#include <unordered_map>
#include "ecs/Components.h"
#include "..

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
    AudioSystem(ECS::World& world);

    // Called every frame
    void Update(float dt);

private:
    ECS::World& m_world;

    // Map entity -> playing instance handle
    std::unordered_map<ECS::Entity, SoundHandle, ECS::EntityHash> m_activeSounds;
};
