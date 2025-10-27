/* Start Header *****************************************************************/
/*!
\file   ScriptingTest.hpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   27th October 2025
\brief
Header for the ScriptingTestScene, demonstrating C# scripting integration.
This scene creates multiple game objects with different C# scripts to prove
that the scripting system works with unique behaviors per object.

RUBRIC REQUIREMENTS:
- Each game object runs its own logic/scripts
- At least 2 different objects with unique logic/behaviour each
- Proven to work in demo

Scripts Used:
- TestGame.PlayerController (figure-8 movement, green)
- TestGame.EnemyAI (patrol behavior, red)
- TestGame.CollectibleItem (bobbing + rainbow colors)

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef SCRIPTINGTEST_HPP
#define SCRIPTINGTEST_HPP

#include "scene/Scene.h"
#include "ecs/World.h"
#include "ecs/systems/ScriptSystem.h"
#include "ecs/systems/RendererSystem.h"
#include <memory>

namespace Sandbox {    
    /**
     * @brief Test scene demonstrating C# scripting with multiple unique behaviors.
     * 
     * This scene creates 5 entities with 3 different C# script types:
     * 1. Player - figure-8 movement (TestGame.PlayerController)
     * 2. Enemy 1 - patrol behavior (TestGame.EnemyAI)
     * 3. Enemy 2 - patrol behavior (TestGame.EnemyAI, separate instance)
     * 4. Collectible 1 - bobbing + rainbow (TestGame.CollectibleItem)
     * 5. Collectible 2 - bobbing + rainbow (TestGame.CollectibleItem, separate instance)
     * 
     * Each object demonstrates independent script execution with unique state.
     */
    class ScriptingTestScene : public Scenes::Scene {
    public:
        ScriptingTestScene() = default;
        ~ScriptingTestScene() override = default;

        void OnLoad() override;
        void OnUpdate() override;
        void OnUnload() override;

    private:
        // Systems
        std::shared_ptr<ECS::ScriptSystem> m_scriptSystem;
        std::shared_ptr<ECS::RendererSystem> m_rendererSystem;

        // Test entities (for debugging/verification)
        ECS::Entity m_playerEntity;
        ECS::Entity m_enemy1Entity;
        ECS::Entity m_enemy2Entity;
        ECS::Entity m_rotatingEntity;
        ECS::Entity m_oscillatingEntity;

        // Scene settings
        float m_worldWidth = 1280.0f;
        float m_worldHeight = 720.0f;

        // Layer IDs
        uint32_t m_playerLayer;
        uint32_t m_enemyLayer;
        uint32_t m_propsLayer;

        // Helper methods
        void _initializeScriptSystem();
        void _createScriptedEntities();
        void _createPlayer();
        void _createEnemy(float x, float y, int enemyNumber);
        void _createRotatingObject();
        void _createOscillatingObject();
        void _createWorldBoundaries();
    };
}

#endif
