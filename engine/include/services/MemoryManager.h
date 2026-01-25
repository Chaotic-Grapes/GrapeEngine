#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <iostream>
#include <cstddef> 

class MemoryManager {
private:
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

	MemoryBlock* blockListHead;   // Head of the memory block linked list
	MemoryPage* pageListHead;     // Head of the memory page linked list
	int defaultPageSize;          // Size of each memory page
	int totalAllocated;           // Total bytes allocated
	int totalFreed;               // Total bytes freed

	// Creates a new memory page and adds it to the page list
	// Called when running out of pre-allocated memory
	void extendMemoryPool();

	// Merges adjacent free blocks to reduce fragmentation
	void mergeAdjacentFreeBlocks(MemoryBlock* block);

public:
	// Initializes memory manager with initial pool size
	MemoryManager(int totalBytes);

	// Destructor: frees all memory pages
	~MemoryManager();

	// Allocates a block of memory from the pool using first-fit strategy
	void* allocate(int size);

	// Deallocates a previously allocated block
	// Merges with adjacent free blocks to reduce fragmentation
	void deallocate(void* ptr);

	// Dumps the current state of the memory manager
	void dump(std::ostream& os);

	// Returns singleton instance of the memory manager
	static MemoryManager& getInstance();
};

// ============================================================================
// GLOBAL NEW/DELETE OPERATOR OVERLOADING
// ============================================================================

// Global new operator override
void* operator new(size_t size);

// Global delete operator override
void operator delete(void* ptr) noexcept;

// Global new[] operator override for arrays
void* operator new[](size_t size);

// Global delete[] operator override for arrays
void operator delete[](void* ptr) noexcept;

#endif