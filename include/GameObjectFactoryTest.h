#ifndef GAMEOBJECTFACTORYTEST_H
#define GAMEOBJECTFACTORYTEST_H
#include "systems/WindowManager.h"
#if _DEBUG

#include "ecs/GameObjectFactory.h"
#include "ecs/Scene.h"

namespace Sandbox {
    class GameObjectFactoryTestScene : public Scene {
    public:
        GameObjectFactoryTestScene() : Scene("GameObjectFactoryTestScene") {
            CREATE_WINDOW("GameObjectFactoryTestScene", 1600, 900);
        }

        void OnLoad() override;

        void OnUpdate() override {}

        void OnUnload() override {}
    };
}
#endif
#endif
