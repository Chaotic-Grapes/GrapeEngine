#include "ecs\systems\PhysicsSystem.h"
#include "ecs\systems\RendererSystem.h"
#include "PhysicsTest.h"
#include "services/Input.h"
#include "core/Application.h"
#include "ecs/Components.h"


using namespace ECS;
using namespace Sandbox;


void PhysicsTestScene::OnLoad(){
	currentTest = Tests::PhysicsCollisionResponse;
}

void PhysicsTestScene::OnUpdate() {
	if (Input::IsKeyDown(KEY_C)) {
		if (!testHandler) {

			//test cycling logic
			int current = static_cast<int>(currentTest);
			current++;
			// if current overflows over test 3 it sets back to test 1 
			if (current > static_cast<int>(Tests::BroadNarrowPhaseCollision)){
				current = static_cast<int>(Tests::PhysicsCollisionResponse);
			}
			
			// clear entities before next tests
			DestroyEntities();
			//set currentTest to current
			currentTest = static_cast<Tests>(current);
			// set to true to prevent rerunning of this loop 
			testHandler = true;
		}
	} 
	// when key is not down reset handler
	else {
		testHandler = false;
	}

	//switch case to switch around test cases
	switch (currentTest) {
	case Tests::PhysicsCollisionResponse:
		PhysicsCollisionResponse();
		break;
	case Tests::PhysicsForces:
		PhysicsForces();
		break;
	case Tests::BroadNarrowPhaseCollision:
		BroadNarrowPhaseCollision();
		break;
	}
}

void PhysicsTestScene::OnUnload() {
	//exit destroy entities
	DestroyEntities();
}

//helper function during unloading
void PhysicsTestScene::DestroyEntities() {
//	const ECS::World& world = GetWorld();
}

void PhysicsTestScene::PhysicsCollisionResponse() {

}

void PhysicsTestScene::PhysicsForces() {

		}

	
void PhysicsTestScene::BroadNarrowPhaseCollision() {

		}