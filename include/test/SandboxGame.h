/* Start Header *****************************************************************/
/*!
\file   SandboxGame.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   22nd September 2025
\brief
Declares the SandboxGame class which extends the Game base class. This class provides
custom implementations for the OnStart, OnUpdate, and OnShutdown methods to define
the behavior of the sandbox game. It uses the SceneManager to manage different
game scenes and their transitions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

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