#include "Engine.h"
#include "Time.h"
#include "ResourceManager.h"
#include <iostream>
#include "MemoryTest.h"

int main() {
	// Test memory tracking first
	RunMemoryTests();

	// Quick ResourceManager test
	ResourceManager rm;
	auto tex = rm.Get<RTexture>("test.png");
	auto audio = rm.Get<RAudio>("test.wav");
	std::cout << "ResourceManager test complete" << std::endl;

	auto* engine = new Engine::Engine();
	engine->AttachSystem(new Time());

	engine->Initialize();
    engine->Run();

	engine->DestroySystems();

    return 0;
}
