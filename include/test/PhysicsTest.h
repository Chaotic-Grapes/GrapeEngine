#pragma once
#include "Game.h"
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "ecs/systems/PhysicsSystem.h"
#include "ecs/systems/RendererSystem.h"
#include "ecs/systems/LifetimeSystem.h"
#include <vector>
#include <memory>


namespace Sandbox {
	class PhysicsTestScene : public Scenes::Scene {
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

		// tests cases
		void PhysicsCollisionResponse();
		void PhysicsForces();
		void BroadNarrowPhaseCollision();

		//helper functions
		void DestroyEntities();
	};
}