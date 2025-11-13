/* Start Header *****************************************************************/
/*!
\file   Game.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   15th September 2025
\brief
Base class for all game instances. Provides the core game loop and lifecycle management.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

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
