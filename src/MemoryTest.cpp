#include "Memory.h"
#include <iostream>

void TestBasicAlloc() {
    std::cout << "=== Testing Basic Allocation ===\n\n";

    // Basic allocations
    float* floatArray = NEW_ARRAY(float, 10);  // 10 * sizeof(float) = 40 bytes
    int* intPtr = NEW(int);                    // sizeof(int) = 4 bytes

    // Fill with some data
    for (int i{}; i < 10; i++) {
        floatArray[i] = i * 2.5f;
    }
    *intPtr = 42;

    std::cout << "Created float array and int\n";
    Memory::GetInstance().PrintStats();

    // SHOULD BE 44 BYTES OF MEMORY AT THIS STAGE

    DELETE_ARRAY(floatArray);
    // THEN 4
    DELETE(intPtr);
    // AND FINALLY 0 (EVERYTHING'S RELEASED)

    std::cout << "Cleaned up\n";
    Memory::GetInstance().PrintStats();
}

void TestMemoryLeak() {
    std::cout << "\n=== Testing Memory Leak Detection ===\n";

    // Create some data but forget to delete it
    unsigned char* largeData = NEW_ARRAY(unsigned char, 1024); // 1024 * sizeof(unsigned char) = 1024 bytes (1 KB)
    int* buffer = NEW_ARRAY(int, 50);  // 50 * sizeof(int) = 200 bytes

    std::cout << "Created large data and buffer\n";
    Memory::GetInstance().PrintStats();

    // SHOULD BE 1224 BYTES AT THIS STAGE
    
    // Only delete one of them (simulate forgetting the other)
    DELETE_ARRAY(buffer);
    // THEN 1024 BYTES

    std::cout << "Deleted buffer but forgot large data\n";
    // largeData is intentionally leaked
}

void TestMultipleAllocs() {
    std::cout << "\n=== Testing Multiple Small Allocations ===\n";

    // Create multiple small objects
    struct Point {
        float x, y, z; 
    };

    Point* p1 = NEW(Point);  // 3 * sizeof(float) = 12 bytes
    Point* p2 = NEW(Point);  // 12 bytes
    Point* p3 = NEW(Point);  // 12 bytes

    p1->x = 1.0f; p1->y = 2.0f; p1->z = 3.0f;
    p2->x = 4.0f; p2->y = 5.0f; p2->z = 6.0f;
    p3->x = 7.0f; p3->y = 8.0f; p3->z = 9.0f;

    std::cout << "Created 3 points\n";
    Memory::GetInstance().PrintStats();

    // SHOULD BE 36 BYTES AT THIS STAGE

    DELETE(p1);
    // 24 BYTES LEFT
    DELETE(p2);
    // 12 BYTES LEFT
    DELETE(p3);
    // 0

    std::cout << "Deleted all points\n";
    Memory::GetInstance().PrintStats();
}

void RunMemoryTests() {
    std::cout << "Starting OpenGL Memory Tracking Tests...\n\n";

    TestBasicAlloc();
    TestMultipleAllocs();
    TestMemoryLeak();

    // Memory leak report and statistics
    Memory::GetInstance().ReportLeaks();
    Memory::GetInstance().PrintStats();
}
