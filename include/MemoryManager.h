#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <mutex>

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
	void RecordAllocation(void* ptr, size_t size, const char* file, int line, const char* function);

	// Whenever memory is freed, find allocation record
	// Mark it as freed
	void RecordDeallocation(void* ptr);

	// Check map for any allocations that were never freed
	void ReportLeaks() const;

	// Updated after storing allocation details (recordAllocation)
	// and whenever memory is freed (recordDeallocation)
	void PrintStats() const;

private:
	// Memory tracking system that logs every allocation 
	// and deallocation (AllocationInfo)
	std::unordered_map<void*, AllocationInfo> m_allocations;

	// Mutex = "mutual exclusion"
	// Ensures only 1 thread (rendering, audio, resource loading) 
	// updates the memory tracking data at a time
	mutable std::mutex m_mutex; // Mutable allows const methods to lock the mutex

	// Statistics
	size_t m_totalAllocated;    // Lifetime total of all memory ever allocated
	size_t m_currentAllocated;  // Amount of memory currently in use
	size_t m_peakAllocated;     // Maximum amount of memory used at any one time
};
