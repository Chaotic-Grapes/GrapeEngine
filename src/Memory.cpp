#include "Memory.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>

// Store allocation details in map
void* Memory::RecordAlloc(size_t size, const char* file, int line, const char* function) {
	std::cout << "\nEntered RecordAlloc()\n";
	// Lock the mutex to ensure thread safety (allow multiple 
	// threads to use code/functions without causing UDB)
	std::lock_guard<std::mutex> lock(m_mutex);
	 
	// Allocate memory cause operator new must return a pointer to allocated memory
	// Trying to avoid infinite recursion by using malloc even though it's C and not C++
	void* ptr = malloc(size);
	if (!ptr) {
		std::cerr << "ERROR: malloc failed\n";
		return nullptr;
	}

	// Create allocation record
	AllocInfo info;
	info.size = size;
	info.file = file ? file : "unknown";
	info.line = line;
	info.function = function ? function : "unknown";

	// Store in the tracking map
	std::cout << "Storing in map\n";
	try {
		m_allocs[ptr] = info;
		std::cout << "Stored successfully\n";
	}
	catch (...) {
		std::cerr << "ERROR: failed to store in map\n";
		free(ptr);
		return nullptr;
	}

	// Update statistics
	m_totalAlloc += size;
	m_currentAlloc += size;

	// Update peak if current usage is higher
	if (m_currentAlloc > m_peakAlloc) {
		m_peakAlloc = m_currentAlloc;
	}

	std::cout << "Allocated " << size << " bytes at " << ptr << " from " << file
		<< ":" << line << " (" << function << "())\n";

	// Return allocated memory
	return ptr;
}

// Whenever memory is freed, find allocation record
// Mark it as freed
void Memory::RecordDealloc(void* ptr) { 
	if (!ptr) return;
	std::cout << "\nEntered RecordDealloc() for: " << ptr << '\n';

	// Lock the mutex to ensure thread safety
	std::lock_guard<std::mutex> lock(m_mutex);

	// Find the allocation record
	std::unordered_map<void*, AllocInfo>::iterator it = m_allocs.find(ptr);
	if (it != m_allocs.end()) {
		std::cout << "Found allocation record, removing...\n";

		// Update current allocated memory
		m_currentAlloc -= it->second.size;

		// Remove from map
		m_allocs.erase(it);

		// Print deallocation info for debugging
		std::cout << "Removed from map successfully\n";
	}
	else std::cout << "WARNING: pointer not found in tracking map\n";
		
	// Free the actual memory
	// Again trying to avoid infinite recursion
	std::cout << "Freeing memory at " << ptr << '\n';
	free(ptr);
}

// Check map for any allocations that were never freed
void Memory::ReportLeaks() const {
	// Lock the mutex to ensure thread safety
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_allocs.empty()) {
		std::cout << "No memory leaks detected\n";
		return;
	}

	std::cout << "\n===== MEMORY LEAK REPORT =====\n";
	std::cout << "Total leaks: " << m_allocs.size() << "\n";
	std::cout << "Total leaked memory: " << m_currentAlloc << " bytes\n";

	// Display each leak
	std::unordered_map<void*, AllocInfo>::const_iterator it;
	for (it = m_allocs.begin(); it != m_allocs.end(); it++) {
		const AllocInfo& info = it->second;
		std::cout << "Leak: " << info.size << " bytes at address " << it->first << "\n";
		std::cout << "Location: " << info.file << ":" << info.line << " in " << info.function << "()\n";
	}
}

// Updated after storing allocation details (recordAllocation)
// and whenever memory is freed (recordDeallocation)
void Memory::PrintStats() const {
	// Lock the mutex to ensure thread safety
	std::lock_guard<std::mutex> lock(m_mutex);

	std::cout << "\n===== MEMORY STATISTICS =====\n";
	std::cout << "Current allocated: " << m_currentAlloc << " bytes\n";
	std::cout << "Peak allocated: " << m_peakAlloc << " bytes\n";
	std::cout << "Total allocated: " << m_totalAlloc << " bytes\n";
	std::cout << "Active allocations: " << m_allocs.size() << "\n";
}

// Constructor implementation
Memory::Memory() : m_totalAlloc(0), m_currentAlloc(0), m_peakAlloc(0) {
	// Initialization code
	std::cout << "MemoryManager initialized\n";
}

// Destructor implementation
Memory::~Memory() {
	if (!m_allocs.empty()) {
		std::cerr << "\n\nWARNING: memory manager destroyed with " << m_allocs.size()
			<< (m_allocs.size() == 1 ? " allocation " : " allocations ")
			<< "still active\n";
		ReportLeaks();
	}
}

// Make sure there's only 1 instance of MemoryManager in the entire program
Memory& Memory::GetInstance() {
	static Memory instance;  // Created only once
	return instance;
}

// Public methods for RenderMemoryOverlay()
size_t Memory::GetCurrentAlloc() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_currentAlloc;
}

size_t Memory::GetPeakAlloc() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_peakAlloc;
}

size_t Memory::GetTotalAlloc() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_totalAlloc;
}

size_t Memory::GetAllocationCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_allocs.size();
}
