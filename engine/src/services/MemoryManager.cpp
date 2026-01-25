/* Start Header *****************************************************************/
/*!
\file   MemoryManager.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   25th January 2026
\brief
Implements the custom MemoryManager class for efficient memory allocation.
- Uses manual doubly-linked list (no STL containers)
- Metadata (MemoryBlock, MemoryPage structs) allocated with malloc
- Actual memory pools allocated with malloc at load/runtime
- First-fit strategy: Finds first free block large enough
- Block splitting: Divides blocks when larger than requested
- Adjacent merging: Combines neighboring free blocks
- Singleton pattern: Single global instance for entire game
*/
/* End Header *******************************************************************/

#include "services/MemoryManager.h"
#include <cstdlib> 

// ============================================================================
// CONSTRUCTORS
// ============================================================================

MemoryManager::MemoryBlock::MemoryBlock(void* p, int s)
	: data(p), size(s), allocated(false), next(nullptr), prev(nullptr) {}

MemoryManager::MemoryPage::MemoryPage(void* start, int size)
	: pageStart(start), pageSize(size), next(nullptr) {}

// ============================================================================
// MEMORY MANAGER IMPLEMENTATION
// ============================================================================

MemoryManager::MemoryManager(int totalBytes)
	: m_blockListHead(nullptr), m_pageListHead(nullptr), m_defaultPageSize(totalBytes),
	m_totalAllocated(0), m_totalFreed(0) {

	// Allocate the first memory page using raw malloc (this is the only OS allocation at load time)
	void* memoryPool = malloc(totalBytes);

	if (!memoryPool) {
		std::cerr << "ERROR: Failed to allocate initial memory pool!" << std::endl;
		return;
	}

	// Create the first page node
	MemoryPage* firstPage = (MemoryPage*)malloc(sizeof(MemoryPage));
	firstPage->pageStart = memoryPool;
	firstPage->pageSize = totalBytes;
	firstPage->next = nullptr;
	m_pageListHead = firstPage;

	// Create the first memory block (entire pool is free initially)
	// We need space for the MemoryBlock metadata, so allocate it using malloc
	MemoryBlock* initialBlock = (MemoryBlock*)malloc(sizeof(MemoryBlock));
	initialBlock->data = memoryPool;
	initialBlock->size = totalBytes;
	initialBlock->allocated = false;
	initialBlock->next = nullptr;
	initialBlock->prev = nullptr;

	// Set as head of the block list
	m_blockListHead = initialBlock;
}

MemoryManager::~MemoryManager() {
	// Free all memory pages
	MemoryPage* currentPage = m_pageListHead;
	while (currentPage != nullptr) {
		MemoryPage* nextPage = currentPage->next;
		free(currentPage->pageStart);  // Free the actual memory
		free(currentPage);              // Free the page metadata
		currentPage = nextPage;
	}

	// Free all memory block metadata
	MemoryBlock* currentBlock = m_blockListHead;
	while (currentBlock != nullptr) {
		MemoryBlock* nextBlock = currentBlock->next;
		free(currentBlock);  // Free the block metadata
		currentBlock = nextBlock;
	}
}

// ============================================================================
// ALLOCATION AND DEALLOCATION
// ============================================================================

void* MemoryManager::Allocate(int size) {
	// Get a block of memory of a specified size from the pool
	// Traverse the free linked list to find a suitable block (first-fit strategy)
	MemoryBlock* current = m_blockListHead;

	while (current != nullptr) {
		// Check if this block is free and large enough
		if (!current->allocated && current->size >= size) {
			// Found a suitable block!

			// If the block is the right size, allocate it directly
			if (current->size == size) {
				current->allocated = true;
				m_totalAllocated += size;
				return current->data;
			}

			// If the block is larger than needed, split it
			// Allocated + remaining free block
			void* allocatedPtr = current->data;

			// Create new block representing leftover free memory
			// Remaining block = Original size - Requested size
			MemoryBlock* remainingBlock = (MemoryBlock*)malloc(sizeof(MemoryBlock));
			remainingBlock->data = static_cast<char*>(current->data) + size;
			remainingBlock->size = current->size - size;

			// Allocated block = Requested (allocated == true)
			// So, remaining block (allocated == false)
			remainingBlock->allocated = false;
			remainingBlock->next = current->next;
			remainingBlock->prev = current;

			// Update the next block's prev pointer if it exists
			if (current->next != nullptr) {
				current->next->prev = remainingBlock;
			}

			// Resize the current block to match allocated size
			current->size = size;
			current->allocated = true;

			// Insert the remaining block after the allocated block
			current->next = remainingBlock;

			m_totalAllocated += size;
			// Return pointer to allocated memory
			return allocatedPtr;
		}

		current = current->next;
	}

	// On failure (no suitable block found) - need to extend memory pool
	std::cout << "WARNING: Out of memory! Extending memory pool by "
		<< m_defaultPageSize << " bytes..." << std::endl;

	_extendMemoryPool();

	// Try allocating again after extending
	return Allocate(size);
}

void MemoryManager::Deallocate(void* ptr) {
	// So when we deallocate, we need to merge with adjacent free blocks

	// Example (deallocate 15-byte block):
	// 1. [10 alloc] [20 free] [15 alloc] [25 free]
	// 2. [10 alloc] [20 free] [15 free] [25 free]
	// 3. [10 alloc] [60 free]; merged (20 + 15 + 25)

	if (ptr == nullptr) return;

	// Free previously allocated memory block
	// Find the block corresponding to this pointer
	MemoryBlock* current = m_blockListHead;

	while (current != nullptr) {
		if (current->allocated && current->data == ptr) {
			// If block is found, mark the block as free
			current->allocated = false;
			m_totalFreed += current->size;

			// Merge with adjacent free blocks to reduce fragmentation
			_mergeAdjacentFreeBlocks(current);
			return;
		}

		current = current->next;
	}

	// If block not found, do nothing (or warn)
	std::cerr << "WARNING: Attempted to deallocate invalid pointer!" << std::endl;
}

// ============================================================================
// MEMORY POOL EXTENSION
// ============================================================================

void MemoryManager::_extendMemoryPool() {
	// Allocate a new memory page at runtime
	void* newPageMemory = malloc(m_defaultPageSize);

	if (!newPageMemory) {
		std::cerr << "CRITICAL ERROR: Failed to extend memory pool!" << std::endl;
		return;
	}

	// Create new page metadata
	MemoryPage* newPage = (MemoryPage*)malloc(sizeof(MemoryPage));
	newPage->pageStart = newPageMemory;
	newPage->pageSize = m_defaultPageSize;
	newPage->next = nullptr;

	// Add to the end of the page list
	MemoryPage* currentPage = m_pageListHead;
	while (currentPage->next != nullptr) {
		currentPage = currentPage->next;
	}
	currentPage->next = newPage;

	// Create a new free block for this page
	MemoryBlock* newBlock = (MemoryBlock*)malloc(sizeof(MemoryBlock));
	newBlock->data = newPageMemory;
	newBlock->size = m_defaultPageSize;
	newBlock->allocated = false;
	newBlock->next = nullptr;
	newBlock->prev = nullptr;

	// Add to the end of the block list
	MemoryBlock* currentBlock = m_blockListHead;
	while (currentBlock->next != nullptr) {
		currentBlock = currentBlock->next;
	}
	currentBlock->next = newBlock;
	newBlock->prev = currentBlock;

	std::cout << "Memory pool extended successfully! New page added: "
		<< m_defaultPageSize << " bytes" << std::endl;
}

// ============================================================================
// FRAGMENTATION MANAGEMENT
// ============================================================================

void MemoryManager::_mergeAdjacentFreeBlocks(MemoryBlock* block) {
	if (block == nullptr) return;

	// Try merging with next block if next is free
	if (block->next != nullptr && !block->next->allocated) {
		// Check if blocks are adjacent in memory
		void* expectedNextStart = static_cast<char*>(block->data) + block->size;

		if (expectedNextStart == block->next->data) {
			// Blocks are adjacent - merge them!
			MemoryBlock* nextBlock = block->next;

			// Merge sizes
			block->size += nextBlock->size;
			block->next = nextBlock->next;

			if (nextBlock->next != nullptr) {
				nextBlock->next->prev = block;
			}

			// Remove next block
			free(nextBlock);  // Free the metadata of the merged block
		}
	}

	// Try merging with previous block if previous is free
	if (block->prev != nullptr && !block->prev->allocated) {
		// Check if blocks are adjacent in memory
		void* expectedCurrentStart = static_cast<char*>(block->prev->data) + block->prev->size;

		if (expectedCurrentStart == block->data) {
			// Blocks are adjacent - merge them!
			MemoryBlock* prevBlock = block->prev;

			// Merge sizes
			prevBlock->size += block->size;
			prevBlock->next = block->next;

			if (block->next != nullptr) {
				block->next->prev = prevBlock;
			}

			// Remove current block
			free(block);  // Free the metadata of the current block
		}
	}
}

// ============================================================================
// DEBUGGING AND UTILITY
// ============================================================================

void MemoryManager::Dump(std::ostream& os) {
	// Print information about the block structure
	os << "========================================" << std::endl;
	os << "MEMORY MANAGER STATE" << std::endl;
	os << "========================================" << std::endl;
	os << "Total Allocated: " << m_totalAllocated << " bytes" << std::endl;
	os << "Total Freed: " << m_totalFreed << " bytes" << std::endl;
	os << "Currently In Use: " << (m_totalAllocated - m_totalFreed) << " bytes" << std::endl;
	os << std::endl;

	// Count pages
	int pageCount = 0;
	MemoryPage* page = m_pageListHead;
	while (page != nullptr) {
		pageCount++;
		page = page->next;
	}
	os << "Total Memory Pages: " << pageCount << std::endl;
	os << std::endl;

	// Display all blocks
	os << "Memory Block List:" << std::endl;
	os << "----------------------------------------" << std::endl;

	MemoryBlock* current = m_blockListHead;
	int blockNum = 0;

	while (current != nullptr) {
		os << "Block #" << blockNum << ": ";
		os << "[Addr: " << current->data << "] ";
		os << "[Size: " << current->size << " bytes] ";
		os << "[Status: " << (current->allocated ? "ALLOCATED" : "FREE") << "]";
		os << std::endl;

		current = current->next;
		blockNum++;
	}

	os << "========================================" << std::endl;
}

// ============================================================================
// SINGLETON PATTERN
// ============================================================================

MemoryManager& MemoryManager::GetInstance() {
	// Static instance with 1MB default pool size
	static MemoryManager instance(1024 * 1024);  // 1 MB default
	return instance;
}

// ============================================================================
// GLOBAL NEW/DELETE OPERATOR IMPLEMENTATIONS
// ============================================================================

// Global new operator: routes all allocations through our memory manager
void* operator new(size_t size) {
	void* ptr = MemoryManager::GetInstance().Allocate(static_cast<int>(size));

	if (!ptr) {
		std::cerr << "ERROR: Global new failed to allocate " << size << " bytes!" << std::endl;
		throw std::bad_alloc();
	}

	return ptr;
}

// Global delete operator: routes all deallocations through our memory manager
void operator delete(void* ptr) noexcept {
	if (ptr != nullptr) {
		MemoryManager::GetInstance().Deallocate(ptr);
	}
}

// Global new[] operator for arrays
void* operator new[](size_t size) {
	void* ptr = MemoryManager::GetInstance().Allocate(static_cast<int>(size));

	if (!ptr) {
		std::cerr << "ERROR: Global new[] failed to allocate " << size << " bytes!" << std::endl;
		throw std::bad_alloc();
	}

	return ptr;
}

// Global delete[] operator for arrays
void operator delete[](void* ptr) noexcept {
	if (ptr != nullptr) {
		MemoryManager::GetInstance().Deallocate(ptr);
	}
}
