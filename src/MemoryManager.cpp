#include "MemoryManager.h"
#include <iostream>
#include <iomanip>

// Store allocation details in map
void* MemoryManager::RecordAllocation(size_t size, const char* file, int line, const char* function) {
	// Lock the mutex to ensure thread safety (allow multiple 
	// threads to use code/functions without causing UDB)
	std::lock_guard<std::mutex> lock(m_mutex);
	 
	// Allocate memory cause operator new must return a pointer to allocated memory
	// Trying to avoid infinite recursion by using malloc even though it's C and not C++
	void* ptr = malloc(size);

	// Create allocation record
	AllocationInfo info;
	info.size = size;
	info.file = file;
	info.line = line;
	info.function = function;
	info.freed = false;

	// Store in the tracking map
	m_allocations[ptr] = info;

	// Update statistics
	m_totalAllocated += size;
	m_currentAllocated += size;

	// Update peak if current usage is higher
	if (m_currentAllocated > m_peakAllocated) {
		m_peakAllocated = m_currentAllocated;
	}

	// Print allocation info for debugging
#ifdef _DEBUG
	std::cout << "Allocated " << size << " bytes at " << ptr << " from " << file
		<< ":" << line << " (" << function << ")\n";
#endif

	// Return allocated memory
	return ptr;
}

// Whenever memory is freed, find allocation record
// Mark it as freed
void MemoryManager::RecordDeallocation(void* ptr) { 
	// Lock the mutex to ensure thread safety
	std::lock_guard<std::mutex> lock(m_mutex);

	// Find the allocation record
	std::unordered_map<void*, AllocationInfo>::iterator it = m_allocations.find(ptr);
	if (it != m_allocations.end()) {
		// Mark as freed (not needed anymore since we're removing it)
		it->second.freed = true;

		// Update current allocated memory
		m_currentAllocated -= it->second.size;

		// Print deallocation info for debugging
#ifdef _DEBUG
		std::cout << "Freed memory at " << ptr << "\n";
#endif

		// Remove from map
		m_allocations.erase(it);
	}

	// Free the actual memory
	// Again trying to avoid infinite recursion
	free(ptr);
}

// Check map for any allocations that were never freed
void MemoryManager::ReportLeaks() const {
	// Lock the mutex to ensure thread safety
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_allocations.empty()) {
		std::cout << "No memory leaks detected\n";
		return;
	}

	std::cout << "\n===== MEMORY LEAK REPORT =====\n";
	std::cout << "Total leaks: " << m_allocations.size() << "\n";
	std::cout << "Total leaked memory: " << m_currentAllocated << " bytes\n\n";

	// Display each leak
	std::unordered_map<void*, AllocationInfo>::const_iterator it;
	for (it = m_allocations.begin(); it != m_allocations.end(); it++) {
		const AllocationInfo& info = it->second;
		std::cout << "Leak: " << info.size << " bytes at address " << it->first << "\n";
		std::cout << "  Location: " << info.file << ":" << info.line << " in " << info.function << "\n\n";
	}
}

// Updated after storing allocation details (recordAllocation)
// and whenever memory is freed (recordDeallocation)
void MemoryManager::PrintStats() const {
	// Lock the mutex to ensure thread safety
	std::lock_guard<std::mutex> lock(m_mutex);

	std::cout << "\n===== MEMORY STATISTICS =====\n";
	std::cout << "Current allocated: " << m_currentAllocated << " bytes\n";
	std::cout << "Peak allocated: " << m_peakAllocated << " bytes\n";
	std::cout << "Total allocated: " << m_totalAllocated << " bytes\n";
	std::cout << "Active allocations: " << m_allocations.size() << "\n";
}

// Constructor implementation (add this)
MemoryManager::MemoryManager() : m_totalAllocated(0), m_currentAllocated(0), m_peakAllocated(0) {
	// Initialization code
	std::cout << "MemoryManager initialized\n";
}

// Make sure there's only 1 instance of MemoryManager in the entire program
MemoryManager& MemoryManager::GetInstance() {
	static MemoryManager instance;  // Created only once
	return instance;
}

// Global operators that track ALL allocations
void* operator new(size_t size) {
	return MemoryManager::GetInstance().RecordAllocation(size, "unknown", 0, "unknown");
}

void* operator new[](size_t size) {
	return MemoryManager::GetInstance().RecordAllocation(size, "unknown", 0, "unknown");
}

void operator delete(void* ptr) noexcept {
	// Always check for null pointer
	if (ptr) MemoryManager::GetInstance().RecordDeallocation(ptr);
}

void operator delete[](void* ptr) noexcept {
	// Always check for null pointer
	if (ptr) MemoryManager::GetInstance().RecordDeallocation(ptr);
}
