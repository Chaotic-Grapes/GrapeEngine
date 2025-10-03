/**
 * @file Memory.cpp
 * @author Foo Rui Qin
 * @date 2024
 * @brief Implementation of the Memory class for memory tracking and leak detection
 * 
 * This file implements the Memory class which provides comprehensive memory tracking
 * capabilities for debugging and optimization. Key implementation features include:
 * - Thread-safe memory allocation and deallocation using mutex protection
 * - Detailed allocation tracking with source location information
 * - Memory leak detection and comprehensive reporting
 * - Real-time memory usage statistics and monitoring
 * - Integration with malloc/free for low-level memory management
 * - Singleton pattern implementation for global access
 * - Comprehensive logging integration for debugging output
 * - Exception handling for robust memory operations
 */

#include "Memory.h"
#include "systems/Logger.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>

// Store allocation details in map
void* Memory::RecordAlloc(size_t size, const char* file, int line, const char* function) {
	LOG_DEBUG("Entered RecordAlloc()");
	// Lock the mutex to ensure thread safety (allow multiple 
	// threads to use code/functions without causing UDB)
	std::lock_guard<std::mutex> lock(m_mutex);
	 
	// Allocate memory cause operator new must return a pointer to allocated memory
	// Trying to avoid infinite recursion by using malloc even though it's C and not C++
	void* ptr = malloc(size);
	if (!ptr) {
		LOG_ERROR("malloc failed");
		return nullptr;
	}

	// Create allocation record
	AllocInfo info;
	info.size = size;
	info.file = file ? file : "unknown";
	info.line = line;
	info.function = function ? function : "unknown";

	// Store in the tracking map
	LOG_DEBUG("Storing in map");
	try {
		m_allocs[ptr] = info;
		LOG_DEBUG("Stored successfully");
	}
	catch (...) {
		LOG_ERROR("Failed to store in map");
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

	std::ostringstream oss;
	oss << "Allocated " << size << " bytes at " << ptr << " from "
		<< file << ":" << line << " (" << function << "())";
	LOG_DEBUG(oss.str());

	// Return allocated memory
	return ptr;
}

// Whenever memory is freed, find allocation record
// Mark it as freed
void Memory::RecordDealloc(void* ptr) { 
	if (!ptr) return;
	LOG_DEBUG("Entered RecordDealloc() for: " << ptr);

	// Lock the mutex to ensure thread safety
	std::lock_guard<std::mutex> lock(m_mutex);

	// Find the allocation record
	std::unordered_map<void*, AllocInfo>::iterator it = m_allocs.find(ptr);
	if (it != m_allocs.end()) {
		LOG_DEBUG("Found allocation record, removing...");

		// Update current allocated memory
		m_currentAlloc -= it->second.size;

		// Remove from map
		m_allocs.erase(it);

		// Print deallocation info for debugging
		LOG_DEBUG("Removed from map successfully");
	}
	else LOG_WARNING("Pointer not found in tracking map");
		
	// Free the actual memory
	// Again trying to avoid infinite recursion
	LOG_DEBUG("Freeing memory at " << ptr);
	free(ptr);
}

// Check map for any allocations that were never freed
void Memory::ReportLeaks() const {
	// Lock the mutex to ensure thread safety
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_allocs.empty()) {
		LOG_INFO("No memory leaks detected");
		return;
	}

	std::ostringstream oss;
	oss << "\n===== MEMORY LEAK REPORT =====\n";
	oss << "Total leaks: " << m_allocs.size() << "\n";
	oss << "Total leaked memory: " << m_currentAlloc << " bytes\n";

	// Display each leak
	std::unordered_map<void*, AllocInfo>::const_iterator it;
	for (it = m_allocs.begin(); it != m_allocs.end(); it++) {
		const AllocInfo& info = it->second;
		oss << "Leak: " << info.size << " bytes at address " << it->first << "\n";
		oss << "Location: " << info.file << ":" << info.line << " in " << info.function << "()\n";
	}

	LOG_WARNING(oss.str());
}

// Updated after storing allocation details (recordAllocation)
// and whenever memory is freed (recordDeallocation)
void Memory::PrintStats() const {
	// Lock the mutex to ensure thread safety
	std::lock_guard<std::mutex> lock(m_mutex);

	std::ostringstream oss;
	oss << "\n===== MEMORY STATISTICS =====\n";
	oss << "Current allocated: " << m_currentAlloc << " bytes\n";
	oss << "Peak allocated: " << m_peakAlloc << " bytes\n";
	oss << "Total allocated: " << m_totalAlloc << " bytes\n";
	oss << "Active allocations: " << m_allocs.size() << "\n";

	LOG_INFO(oss.str());
}

// Constructor implementation
Memory::Memory() : m_totalAlloc(0), m_currentAlloc(0), m_peakAlloc(0) {
	// Initialization code
	LOG_INFO("MemoryManager initialized");
}

// Destructor implementation
Memory::~Memory() {
	if (!m_allocs.empty()) {
		std::ostringstream oss;
		oss << "\n\nWARNING: memory manager destroyed with " << m_allocs.size()
			<< (m_allocs.size() == 1 ? " allocation " : " allocations ")
			<< "still active";
		LOG_WARNING(oss.str());
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
