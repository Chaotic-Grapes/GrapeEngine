// ******** NOTHING IS TO BE PLACED ABOVE _DEBUG ******** //
#ifdef _DEBUG

#include "PhysicsCollision2DTest.h"
#include <crtdbg.h>
#include "Application.h"
#include <iostream>

// ********************* IMPORTANT ********************* //
// Declare your world creation function here, then define
// below RunTestMenu().
namespace {
    void TestPhysics2D();
    void TestGraphics();
    void RunTestMenu();
}
// ***************************************************** //

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    RunTestMenu();

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
            //case 3:
            //    std::cout << "ECS Component Test - TODO" << '\n';
            //    break;
        case 2:
            TestGraphics();
            break;
        default:
            std::cout << "Invalid choice" << '\n';
            break;
        }
    }

    void TestPhysics2D() {
        std::cout << "Starting Physics2D Test..." << '\n';

        Engine::Application engine;
        Sandbox::PhysicsCollision2DTestScene game = Sandbox::PhysicsCollision2DTestScene(1600, 900, 7);
        engine.Run(game, true);
    }

    void TestGraphics() {
        std::cout << "Starting Graphics Test..." << '\n';

        Engine::Application engine;
    }
}

#endif