#ifndef SANDBOXGAME_H
#define SANDBOXGAME_H

#include "Game.h"
#include "ecs/SceneManager.h"

class SandboxGame final : public Game {
public:
	void OnStart(SceneManager& sceneManager) override;
	void OnUpdate(SceneManager& sceneManager) override;
	void OnShutdown(SceneManager& sceneManager) override;
};

#endif