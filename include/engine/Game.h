#ifndef GAME_H
#define GAME_H

#include "scene/SceneManager.h"

class Game {
public:
    virtual ~Game() = default;

    // Called once when the engine starts; set initial scene here
    virtual void OnStart(Scenes::SceneManager& sceneManager) { (void)sceneManager; }

    // Called every frame; use sceneManager to switch levels if needed
    virtual void OnUpdate(Scenes::SceneManager& sceneManager) { (void)sceneManager; }

    // Called once before shutdown
    virtual void OnShutdown(Scenes::SceneManager& sceneManager) { (void)sceneManager; }
};

#endif
