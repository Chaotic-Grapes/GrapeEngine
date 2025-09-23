#ifndef SCENE_H
#define SCENE_H

#include "ecs/World.h"

class Scene {
public:
    virtual ~Scene() = default;

    virtual void OnLoad(World& world) = 0;
    virtual void OnUnload(World& world) {}
};

#endif
