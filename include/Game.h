#ifndef GAME_H
#define GAME_H

#include "ecs/SceneManager.h"

class Game {
public:
    virtual ~Game() = default;

    // Called once when the engine starts; set initial scene here
    virtual void OnStart(SceneManager& sceneManager) {}

    // Called every frame; use sceneManager to switch levels if needed
    virtual void OnUpdate(SceneManager& sceneManager) {}

    // Called once before shutdown
    virtual void OnShutdown(SceneManager& sceneManager) {}
};

#endif
