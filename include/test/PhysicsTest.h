/**
* @Name: Dalton koh, 2403250
* @email: d.koh@digipen.edu
* @file PhysicsTest.h
* @brief Test scene declarations for exercising the 2D PhysicsSystem.
*
* @details
* Declares a Sandbox scene that cycles through functions declared here
* in class PhysicsTestScene
*/

#pragma once
#include "Game.h"
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "ecs/systems/PhysicsSystem.h"
#include "ecs/systems/RendererSystem.h"
#include "ecs/systems/LifetimeSystem.h"
#include "scene/TestScene.h"
#include <vector>
#include <memory>

/**
* @class PhysicsTestScene
* @brief  sandbox scene to drive physics demonstrations.
*/
namespace Sandbox {
	class PhysicsTestScene : public Scenes::TestScene {
	public:
		
		//store test iteration vars in enum
		enum class Tests {
			PhysicsCollisionResponse = 100,
			PhysicsForces,
			BroadNarrowPhaseCollision
		};

		// override to prevent base class being in the mix
		void OnLoad() override;   // init
		void OnUpdate() override; // main loop running iterating tests
		void OnUnload() override; // on unloading 

	private:
		
		// Teststate handlers
		bool testHandler = false;
		Tests currentTest{ Tests::PhysicsCollisionResponse };

		// world dimensions for easy use 
		float worldWidth = 1600.f;
		float worldHeight = 900.f;

		// test entities storage
		std::vector<uint64_t> testEntities;

		// Systems - NEED THIS FOR RENDERING
		std::shared_ptr<ECS::RendererSystem> m_rendererSystem;

		// Layer for rendering
		uint16_t m_testLayer = 0;

		// Persistent scene camera (packed id). Kept separate from testEntities so
		// it survives test clears.
		uint64_t m_cameraEntity = 0;

		// tests cases
		void PhysicsCollisionResponse();
		void PhysicsForces();
		void BroadNarrowPhaseCollision();

		//helper functions

		//destroy entities
		void DestroyEntities();
		// create wall boundaries
		void CreateStaticWall(float x, float y, float width, float height);
	};
}