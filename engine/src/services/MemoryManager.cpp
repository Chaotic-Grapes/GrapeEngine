#include "services/MemoryManager.h"

MemoryManager::MemoryBlock::MemoryBlock(void* p, int s) : data(p), size(s), allocated(false) {}

MemoryManager::MemoryManager(int totalBytes) {
	// Allocate a single raw block of memory for the pool
	memoryPool = new char[totalBytes];

	// Initialize the memory block list with one large free block
	MemoryBlock initialBlock(memoryPool, totalBytes);
	initialBlock.allocated = false;

	// Add the initial block to the list
	memoryBlock.push_back(initialBlock);
}

MemoryManager::~MemoryManager() { delete[] static_cast<char*>(memoryPool); }

void* MemoryManager::allocate(int size) {
	// Get a block of memory of a specified size from the pool
	std::list<MemoryBlock>::iterator it = std::find_if(
		memoryBlock.begin(), memoryBlock.end(), [size](const MemoryBlock& block) {
			return !block.allocated && block.size >= size; 
		}
	);

	// On failure, return nullptr
	if (it == memoryBlock.end()) return nullptr;

	// If the block is the right size, allocate it directly
	if (it->size == size) {
		it->allocated = true;
		return it->data;
	}

	// If the block is larger than needed, split it
	// Allocated + remaining free block
	void* allocatedPtr = it->data;

	// Create new block representing leftover free memory
	// Remaining block = Original size - Requested size
	MemoryBlock remainingBlock(static_cast<char*>(it->data) + size, it->size - size);

	// Allocated block = Requested (allocated == true)
	// So, remaining block (allocated == false)
	remainingBlock.allocated = false;

	// Resize the current block to match allocated size
	it->size = size;
	it->allocated = true;

	// Insert the remaining block after the allocated block
	memoryBlock.insert(std::next(it), remainingBlock);

	// Return pointer to allocated memory
	return allocatedPtr;
}

void MemoryManager::deallocate(void* ptr) {
	// So when we deallocate, we need to merge with adjacent free blocks

	// Example (dellocate 15-byte block):
	// 1. [10 alloc] [20 free] [15 alloc] [25 free]
	// 2. [10 alloc] [20 free] [15 free] [25 free]
	// 3. [10 alloc] [60 free]; merged (20 + 15 + 25)

	// Free previously allocated memory block
	std::list<MemoryBlock>::iterator it = std::find_if(
		memoryBlock.begin(), memoryBlock.end(), [ptr](const MemoryBlock& block) {
			return block.allocated && block.data == ptr; 
		}
	);

	// If block not found, do nothing
	if (it == memoryBlock.end()) return;

	// If block is found, mark the block as free
	it->allocated = false;

	// Try merging with previous block if previous is free
	if (it != memoryBlock.begin()) {
		// Iterator to previous block
		std::list<MemoryBlock>::iterator prevIt = std::prev(it);

		// Check if it's allocated
		if (!prevIt->allocated) {
			// If not (i.e. free)
			prevIt->size += it->size;   // Merge sizes
			it = memoryBlock.erase(it); // Remove current block
			it = prevIt;                // Move iterator to previous block
		}
	}

	// Try merging with next block if next is free
	std::list<MemoryBlock>::iterator nextIt = std::next(it);

	// Check if it's allocated
	if (nextIt != memoryBlock.end() && !nextIt->allocated) {
		it->size += nextIt->size;      // Merge sizes
		memoryBlock.erase(nextIt);     // Remove next block
	}
}

void MemoryManager::dump(std::ostream& os) {
	// Print information about the block structure
	for (std::list<MemoryBlock>::iterator it = memoryBlock.begin(); it != memoryBlock.end(); it++) {
		// IDK TBD
	}
}
