// ******** NOTHING IS TO BE PLACED ABOVE _DEBUG ******** //
#ifdef _DEBUG

#include "Renderer2D.h"
#include "Physics2D.h"
#include "PhysicsCollision2DTestScene.h"
#include <crtdbg.h>
#include "Application.h"
#include <iostream>

// ********************* IMPORTANT ********************* //
// Declare your world creation function here, then define
// below RunTestMenu().
namespace {
    void TestPhysics2D();
    void RunTestMenu();
}
// ***************************************************** //

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    Engine::Application engine;

    RunTestMenu();

    engine.Run(true);
    return 0;
}

// ******* WORLD CREATION FUNCTIONS FOR TESTING ******* //
namespace {
    void RunTestMenu() {
        std::cout << "Select test scene: \n";
        std::cout << "1. Physics2D Test" << '\n';
        //std::cout << "2. Rendering Test" << '\n';
        //std::cout << "3. ECS Component Test" << '\n';

        int choice;
        std::cin >> choice;

        // Clear console
        // Not thread-safe but does it matter?
        system("cls");

        switch (choice) {
        case 1:
            TestPhysics2D();
            break;
            //case 2:
            //    std::cout << "Rendering Test - TODO" << '\n';
            //    break;
            //case 3:
            //    std::cout << "ECS Component Test - TODO" << '\n';
            //    break;
        default:
            std::cout << "Invalid choice" << '\n';
            break;
        }
    }

    void TestPhysics2D() {
        std::cout << "Starting Physics2D Test..." << '\n';

        // Create world and systems
        World& world = CREATE_WORLD();

        // Create and initialize test scene
        TestScene2D testScene;
        testScene.WorldWidth = 1600.0f;
        testScene.WorldHeight = 900.0f;
        testScene.DampingDelay = 7.0f;
        testScene.Initialize(&world, testScene.WorldWidth, testScene.WorldHeight);
    }
}

#endif