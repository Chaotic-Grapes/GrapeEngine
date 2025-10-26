#ifndef SANDBOXGAME_H
#define SANDBOXGAME_H

#include "Game.h"
#include "scene/SceneManager.h"

class SandboxGame final : public Game {
public:
		void OnStart(Scenes::SceneManager& sceneManager) override;
		void OnUpdate(Scenes::SceneManager& sceneManager) override;
		void OnShutdown(Scenes::SceneManager& sceneManager) override;
};

#endif