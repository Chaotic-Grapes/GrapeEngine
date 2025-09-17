#include "Engine.h"
#include "Time.h"
#include <iostream>
#include "MemoryTest.h"

int main() {
	// Test memory tracking first
	RunMemoryTests();

	auto* engine = new Engine::Engine();
	engine->AttachSystem(new Time());

	engine->Initialize();
    engine->Run();

	engine->DestroySystems();

    return 0;
}
