#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <mutex>
#include <new> 

struct AllocationInfo {
	// What was allocated (size), where it was located from (which file/function/line), 
	// whether it's still allocated or was freed
	size_t size = 0;
	std::string file, function;
	int line = 0;
	bool freed = false;
};

class MemoryManager {
public:
	// Store allocation details in map
	void* RecordAllocation(size_t size, const char* file, int line, const char* function);

	// Whenever memory is freed, find allocation record
	// Mark it as freed
	void RecordDeallocation(void* ptr);  // Remove void* return type

	// Check map for any allocations that were never freed
	void ReportLeaks() const;

	// Updated after storing allocation details (recordAllocation)
	// and whenever memory is freed (recordDeallocation)
	void PrintStats() const;

	// Make sure there's only 1 instance of MemoryManager in the entire program
	static MemoryManager& GetInstance();

private:
	// Memory tracking system that logs every allocation 
	// and deallocation (AllocationInfo)
	std::unordered_map<void*, AllocationInfo> m_allocations;

	// Mutex = "mutual exclusion"
	// Ensures only 1 thread (rendering, audio, resource loading) 
	// Updates the memory tracking data at a time
	mutable std::mutex m_mutex; // Mutable allows const methods to lock the mutex

	// Statistics
	size_t m_totalAllocated = 0;    // Lifetime total of all memory ever allocated
	size_t m_currentAllocated = 0;  // Amount of memory currently in use
	size_t m_peakAllocated = 0;     // Maximum amount of memory used at any one time

	// Make constructor private so no one can create instances
	MemoryManager();

	// Prevent copying
	MemoryManager(const MemoryManager&) = delete;
	MemoryManager& operator=(const MemoryManager&) = delete;
};

// Right now we have to manually call RecordAllocation/RecordDeallocation
// Automate this? By overriding global new/delete
void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
