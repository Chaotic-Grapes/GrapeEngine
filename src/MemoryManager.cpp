#include "MemoryManager.h"
#include <iostream>
#include <iomanip>

// Store allocation details in map
void MemoryManager::RecordAllocation(void* ptr, size_t size, const char* file, int line, const char* function) {
	// Lock the mutex to ensure thread safety (allow multiple 
	// threads to use code/functions without causing UDB)
	std::lock_guard<std::mutex> lock(m_mutex);

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
	std::cout << "Allocated " << size << " bytes at " << ptr
		<< " from " << file << ":" << line << " (" << function << ")\n";
#endif
}

// Whenever memory is freed, find allocation record
// Mark it as freed
void MemoryManager::RecordDeallocation(void* ptr) {
	// Lock the mutex to ensure thread safety
	std::lock_guard<std::mutex> lock(m_mutex);

	// Find the allocation record
	std::unordered_map<void*, AllocationInfo>::iterator it = m_allocations.find(ptr);
	if (it != m_allocations.end()) {
		// Mark as freed
		it->second.freed = true;

		// Update current allocated memory
		m_currentAllocated -= it->second.size;

		// Remove from map
		m_allocations.erase(it);

		// Print deallocation info for debugging
#ifdef _DEBUG
		std::cout << "Freed memory at " << ptr << "\n";
#endif
	}
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
