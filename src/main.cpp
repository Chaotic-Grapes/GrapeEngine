#include "Engine.h"
#include "Time.h"
#include "MemoryManager.h"
#include <iostream>

void TestMemoryManager() {
    std::cout << "=== Testing Memory Manager ===\n";

    // Test 1: Basic allocation
    int* number = new int(42);
    std::cout << "Allocated int: " << *number << "\n";
    delete number;

    // Test 2: Array allocation  
    int* array = new int[10];
    std::cout << "Allocated array of 10 ints\n";
    delete[] array;

    // Test 3: Object allocation
    struct TestObject {
        float x, y, z;
        std::string name;
    };

    TestObject* obj = new TestObject{ 1.0f, 2.0f, 3.0f, "test" };
    std::cout << "Allocated object: " << obj->name << "\n";
    delete obj;

    // Show stats
    MemoryManager::GetInstance().PrintStats();

    // Check for leaks (should be none)
    MemoryManager::GetInstance().ReportLeaks();

    std::cout << "=== Test Complete ===\n";
}

int main() {
    // Test memory manager first
    TestMemoryManager();

	auto* engine = new Engine::Engine();
	engine->AttachSystem(new Time());

	engine->Initialize();
    engine->Run();

	engine->DestroySystems();

    // Final leak check
    MemoryManager::GetInstance().ReportLeaks();

    return 0;
}
