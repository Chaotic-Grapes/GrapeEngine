#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <mutex>

struct AllocInfo {
	// What was allocated (size), where it was located from (which file/function/line), 
	// whether it's still allocated or was freed
	size_t size = 0;
	std::string file, function;
	int line = 0;
	bool freed = false;
};

class Memory {
public:
	// Store allocation details in map
	void* RecordAlloc(size_t size, const char* file, int line, const char* function);

	// Whenever memory is freed, find allocation record
	// Mark it as freed
	void RecordDealloc(void* ptr);

	// Check map for any allocations that were never freed
	void ReportLeaks() const;

	// Updated after storing allocation details (recordAllocation)
	// and whenever memory is freed (recordDeallocation)
	void PrintStats() const;

	// Make sure there's only 1 instance of MemoryManager in the entire program
	static Memory& GetInstance();

private:
	// Memory tracking system that logs every allocation 
	// and deallocation (AllocationInfo)
	std::unordered_map<void*, AllocInfo> m_allocs;

	// Mutex = "mutual exclusion"
	// Ensures only 1 thread (rendering, audio, resource loading) 
	// Updates the memory tracking data at a time
	mutable std::mutex m_mutex; // Mutable allows const methods to lock the mutex

	// Statistics
	size_t m_totalAlloc = 0;    // Lifetime total of all memory ever allocated
	size_t m_currentAlloc = 0;  // Amount of memory currently in use
	size_t m_peakAlloc = 0;     // Maximum amount of memory used at any one time

	// Make constructor private so no one can create instances
	Memory();

	// Prevent copying
	Memory(const Memory&) = delete;
	Memory& operator=(const Memory&) = delete;
};

// Helper templates for typed allocation (new and new[])
template <typename T>
T* TrackedAlloc(const char* file, int line, const char* function) {
	void* ptr = Memory::GetInstance().RecordAlloc(sizeof(T), file, line, function);
	return static_cast<T*>(ptr);
}

template <typename T>
T* TrackedAllocArray(size_t count, const char* file, int line, const char* function) {
	void* ptr = Memory::GetInstance().RecordAlloc(sizeof(T) * count, file, line, function);
	return static_cast<T*>(ptr);
}

// Macros that capture file/line/function automatically
// Even though both delete and delete[] just need to free the pointer, 
// it's better to separate them for type safety
#define NEW(type) TrackedAlloc<type>(__FILE__, __LINE__, __FUNCTION__)
#define NEW_ARRAY(type, count) TrackedAllocArray<type>(count, __FILE__, __LINE__, __FUNCTION__)
#define DELETE(ptr) Memory::GetInstance().RecordDealloc(ptr)
#define DELETE_ARRAY(ptr) Memory::GetInstance().RecordDealloc(ptr)
