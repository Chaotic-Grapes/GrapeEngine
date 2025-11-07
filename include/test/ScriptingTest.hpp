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

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef SCRIPTINGTEST_HPP
#define SCRIPTINGTEST_HPP

#include "scene/TestScene.h"
#include "ecs/World.h"
#include "ecs/systems/ScriptSystem.h"
#include "ecs/systems/RendererSystem.h"
#include <memory>

namespace Sandbox {    
    /**
     * @brief Test scene demonstrating C# scripting with multiple unique behaviors.
     * 
     * Each object demonstrates independent script execution with unique state.
     */
    class ScriptingTestScene : public Scenes::TestScene {
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

        // Persistent scene camera (packed id). Kept separate from scripted
        // controller entities so it survives any scene spawn/clears.
        uint64_t m_cameraEntity = 0;

        // Helper methods
        void _initializeScriptSystem();
        void _createScriptedEntities();
        void _createPlayer();
        void _createEnemy(int enemyNumber);
        //void _createCollectible(int collectibleNumber);
    };
}

#endif
