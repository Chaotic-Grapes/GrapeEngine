/* Start Header *****************************************************************/
/*!
\file   MemoryManager.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   25th January 2026
\brief
Declares the custom MemoryManager class for efficient game object memory allocation.
Provides:
- Pre-allocated memory pool using memory pages
- Address-ordered free linked list for optimal fragmentation reduction
- No STL containers (manual doubly-linked list implementation)
- Placement new support for object construction in pre-allocated memory
- Dynamic memory page extension when running out of memory
- Global new/delete operator overloading for automatic memory management
- Fragmentation reduction through adjacent block merging
- Debugging support via Dump() function
*/
/* End Header *******************************************************************/

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <iostream>
#include <cstddef> 
#include <mutex>
#include "Export.h"

class GRAPEENGINE_API MemoryManager {
private:
	// ============================================================================
	// DEBUG PATTERNS
	// ============================================================================

	// Memory signature patterns for debugging (similar to ObjectAllocator)
	static const unsigned char UNALLOCATED_PATTERN = 0xAA;  // New memory
	static const unsigned char ALLOCATED_PATTERN = 0xBB;    // In use
	static const unsigned char FREED_PATTERN = 0xCC;        // Just freed

	// ============================================================================
	// CONSTRUCTORS
	// ============================================================================

	// Memory block node: represents a single block in the free linked list
	// Used to track both allocated and free memory regions
	struct MemoryBlock {
		void* data;           // Pointer to actual memory
		int size;             // Size of this block in bytes
		bool allocated;       // true = allocated, false = free
		MemoryBlock* next;    // Next block in the linked list
		MemoryBlock* prev;    // Previous block in the linked list

		MemoryBlock(void* ptr, int s);
	};

	// Memory page: represents a single page of pre-allocated memory
	// Allows dynamic extension when initial pool is exhausted
	struct MemoryPage {
		void* pageStart;      // Start of this memory page
		int pageSize;         // Total size of this page
		MemoryPage* next;     // Next page in the list

		MemoryPage(void* start, int size);
	};

	MemoryBlock* m_blockListHead;   // Head of the memory block linked list (address-ordered)
	MemoryPage* m_pageListHead;     // Head of the memory page linked list
	int m_defaultPageSize;          // Size of each memory page
	int m_totalAllocated;           // Total bytes allocated
	int m_totalFreed;               // Total bytes freed
	bool m_debugMode;               // Enable/disable debug pattern filling
	std::recursive_mutex m_mutex;   // Thread safety

	// ============================================================================
	// MEMORY POOL EXTENSION
	// ============================================================================

	// Creates a new memory page and adds it to the page list
	// Called when running out of pre-allocated memory
	void _extendMemoryPool();

	// ============================================================================
	// FRAGMENTATION MANAGEMENT
	// ============================================================================

	// Merges adjacent free blocks to reduce fragmentation
	void _mergeAdjacentFreeBlocks(MemoryBlock* block);

	// ============================================================================
	// ADDRESS-ORDERED INSERTION
	// ============================================================================

	// Inserts a free block into the address-ordered free list
	// This significantly reduces fragmentation compared to insertion-order
	void _insertBlockAddressOrdered(MemoryBlock* block);

public:
	// ============================================================================
	// MEMORY MANAGER IMPLEMENTATION
	// ============================================================================

	// Initializes memory manager with initial pool size
	MemoryManager(int totalBytes, bool debugMode = false);

	// Destructor: frees all memory pages
	~MemoryManager();

	// ============================================================================
	// ALLOCATION AND DEALLOCATION
	// ============================================================================

	// Allocates a block of memory from the pool using address-ordered first-fit strategy
	void* Allocate(int size);

	// Deallocates a previously allocated block
	// Merges with adjacent free blocks to reduce fragmentation
	void Deallocate(void* ptr);

	// ============================================================================
	// DEBUGGING AND UTILITY
	// ============================================================================

	// Dumps the current state of the memory manager
	void Dump(std::ostream& os);

	// Enable or disable debug mode at runtime
	void SetDebugMode(bool enabled);

	// Get current debug mode state
	bool GetDebugMode() const { return m_debugMode; }

	// ============================================================================
	// SINGLETON PATTERN
	// ============================================================================

	// Returns singleton instance of the memory manager
	static MemoryManager& GetInstance();
};

#endif