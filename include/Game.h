#ifndef GAME_H
#define GAME_H

#include "ecs/World.h"

class Game {
public:
    virtual ~Game() = default;

    virtual void OnStart(SceneManager& sceneManager) {}
    virtual void OnUpdate(World& world) {}
    virtual void OnShutdown(World& world) {}
};

#endif
