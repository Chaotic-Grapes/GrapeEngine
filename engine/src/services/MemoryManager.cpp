/* Start Header *****************************************************************/
/*!
\file   MemoryManager.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   25th January 2026
\brief
Implements the custom MemoryManager class for efficient memory allocation.
- Uses manual doubly-linked list (no STL containers)
- Address-ordered free list for reduced fragmentation
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
#include <cstring>

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

MemoryManager::MemoryManager(int totalBytes, bool debugMode)
	: m_blockListHead(nullptr), m_pageListHead(nullptr), m_defaultPageSize(totalBytes),
	m_totalAllocated(0), m_totalFreed(0), m_debugMode(debugMode) {

	// Allocate the first memory page using raw malloc (this is the only OS allocation at load time)
	void* memoryPool = malloc(totalBytes);

	if (!memoryPool) {
		// CRITICAL: Failed to allocate initial pool, cannot proceed
		return;
	}

	printf("MemoryManager: Initial pool allocated successfully!\n");
	fflush(stdout);

	// Fill with unallocated pattern if debug mode is enabled
	if (m_debugMode) memset(memoryPool, UNALLOCATED_PATTERN, totalBytes);

	// Create the first page node
	MemoryPage* firstPage = (MemoryPage*)malloc(sizeof(MemoryPage));
	if (!firstPage) {
		// CRITICAL: Failed to allocate page metadata
		free(memoryPool);
		return;
	}
	firstPage->pageStart = memoryPool;
	firstPage->pageSize = totalBytes;
	firstPage->next = nullptr;
	m_pageListHead = firstPage;

	// Create the first memory block (entire pool is free initially)
	// We need space for the MemoryBlock metadata, so allocate it using malloc
	MemoryBlock* initialBlock = (MemoryBlock*)malloc(sizeof(MemoryBlock));
	if (!initialBlock) {
		// CRITICAL: Failed to allocate block metadata
		free(firstPage);
		free(memoryPool);
		return;
	}
	initialBlock->data = memoryPool;
	initialBlock->size = totalBytes;
	initialBlock->allocated = false;
	initialBlock->next = nullptr;
	initialBlock->prev = nullptr;

	// Set as head of the block list
	m_blockListHead = initialBlock;

	// Debug mode is now active (if enabled)
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
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	// RECURSION PROTECTION: Prevent infinite loops if pool extension fails repeatedly
	static thread_local int recursionDepth = 0;

	if (recursionDepth > 5) {
		// Too many recursive calls: malloc is probably exhausted or pool extension is broken
		// Return nullptr to prevent stack overflow
		recursionDepth = 0;
		return nullptr;
	}

	recursionDepth++;

	// Align size to 16 bytes to ensure proper alignment for all allocations
	const int ALIGNMENT = 16;
	int alignedSize = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
	if (alignedSize < ALIGNMENT) alignedSize = ALIGNMENT;

	// Get a block of memory of a specified size from the pool
	// Traverse the address-ordered free linked list to find a suitable block (first-fit strategy)
	MemoryBlock* current = m_blockListHead;

	while (current != nullptr) {
		// Check if this block is free and large enough
		if (!current->allocated && current->size >= alignedSize) {
			// Found a suitable block

			// If the block is the right size, allocate it directly
			if (current->size == alignedSize) {
				current->allocated = true;
				m_totalAllocated += alignedSize;

				// Fill with allocated pattern if debug mode enabled
				if (m_debugMode) memset(current->data, ALLOCATED_PATTERN, alignedSize);

				recursionDepth--;
				return current->data;
			}

			// If the block is larger than needed, split it
			// Allocated + remaining free block
			void* allocatedPtr = current->data;

			// Create new block representing leftover free memory
			// Remaining block = Original size - Requested size
			MemoryBlock* remainingBlock = (MemoryBlock*)malloc(sizeof(MemoryBlock));
			if (!remainingBlock) {
				// Can't allocate metadata, but we can still use the whole block
				current->allocated = true;
				m_totalAllocated += current->size;

				// Fill with allocated pattern if debug mode enabled
				if (m_debugMode) memset(current->data, ALLOCATED_PATTERN, current->size);

				recursionDepth--;
				return current->data;
			}
			remainingBlock->data = static_cast<char*>(current->data) + alignedSize;
			remainingBlock->size = current->size - alignedSize;

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
			current->size = alignedSize;
			current->allocated = true;

			// Insert the remaining block after the allocated block
			current->next = remainingBlock;

			m_totalAllocated += alignedSize;

			// Fill with allocated pattern if debug mode enabled
			if (m_debugMode) memset(allocatedPtr, ALLOCATED_PATTERN, alignedSize);

			// Return pointer to allocated memory
			recursionDepth--;
			return allocatedPtr;
		}

		current = current->next;
	}

	// On failure (no suitable block found): need to extend memory pool
	// WARNING: Out of memory, extending pool

	_extendMemoryPool();

	// Try allocating again after extending
	void* result = Allocate(size); // Use original size, it will be aligned again in recursion
	recursionDepth--;
	return result;
}

void MemoryManager::Deallocate(void* ptr) {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

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
			// Fill with freed pattern if debug mode enabled
			if (m_debugMode) memset(current->data, FREED_PATTERN, current->size);

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
	// WARNING: Attempted to deallocate invalid pointer
}

// ============================================================================
// MEMORY POOL EXTENSION
// ============================================================================

void MemoryManager::_extendMemoryPool() {
	// Allocate a new memory page at runtime
	void* newPageMemory = malloc(m_defaultPageSize);

	if (!newPageMemory) {
		// CRITICAL: Failed to extend memory pool
		return;
	}

	// Fill with unallocated pattern if debug mode enabled
	if (m_debugMode) memset(newPageMemory, UNALLOCATED_PATTERN, m_defaultPageSize);

	// Create new page metadata
	MemoryPage* newPage = (MemoryPage*)malloc(sizeof(MemoryPage));
	if (!newPage) {
		// CRITICAL: Failed to allocate page metadata
		free(newPageMemory);
		return;
	}
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
	if (!newBlock) {
		// CRITICAL: Failed to allocate block metadata
		// Page is added but no block: this will leak, but prevents crash
		return;
	}
	newBlock->data = newPageMemory;
	newBlock->size = m_defaultPageSize;
	newBlock->allocated = false;
	newBlock->next = nullptr;
	newBlock->prev = nullptr;

	// Insert this new block in address-ordered position
	_insertBlockAddressOrdered(newBlock);

	// INFO: Pool extended successfully
}

// ============================================================================
// ADDRESS-ORDERED INSERTION
// ============================================================================

void MemoryManager::_insertBlockAddressOrdered(MemoryBlock* block) {
	// Insert block into the free list in address order
	// This reduces fragmentation significantly
	if (block == nullptr) return;

	// Empty list case
	if (m_blockListHead == nullptr) {
		m_blockListHead = block;
		block->next = nullptr;
		block->prev = nullptr;
		return;
	}

	// Insert at beginning if this block has the lowest address
	if (block->data < m_blockListHead->data) {
		block->next = m_blockListHead;
		block->prev = nullptr;
		m_blockListHead->prev = block;
		m_blockListHead = block;
		return;
	}

	// Find the correct position in the address-ordered list
	MemoryBlock* current = m_blockListHead;
	while (current->next != nullptr && current->next->data < block->data) current = current->next;

	// Insert after current
	block->next = current->next;
	block->prev = current;

	if (current->next != nullptr) current->next->prev = block;
	current->next = block;
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
			// Blocks are adjacent: merge them
			MemoryBlock* nextBlock = block->next;

			// Merge sizes
			block->size += nextBlock->size;
			block->next = nextBlock->next;

			if (nextBlock->next != nullptr) nextBlock->next->prev = block;

			// Remove next block
			free(nextBlock);  // Free the metadata of the merged block
		}
	}

	// Try merging with previous block if previous is free
	if (block->prev != nullptr && !block->prev->allocated) {
		// Check if blocks are adjacent in memory
		void* expectedCurrentStart = static_cast<char*>(block->prev->data) + block->prev->size;

		if (expectedCurrentStart == block->data) {
			// Blocks are adjacent: merge them
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

void MemoryManager::SetDebugMode(bool enabled) {
	m_debugMode = enabled;
	// Debug mode changed (logging removed to prevent recursion)
}

void MemoryManager::Dump(std::ostream& os) {
	// Print information about the block structure
	os << "========================================" << std::endl;
	os << "MEMORY MANAGER STATE" << std::endl;
	os << "========================================" << std::endl;
	os << "Debug Mode: " << (m_debugMode ? "ENABLED" : "DISABLED") << std::endl;
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

	// Display all blocks (in address order)
	os << "Memory Block List (Address-Ordered):" << std::endl;
	os << "----------------------------------------" << std::endl;

	MemoryBlock* current = m_blockListHead;
	int blockNum = 0;
	int totalFreeBlocks = 0;
	int totalFreeBytes = 0;

	while (current != nullptr) {
		os << "Block #" << blockNum << ": ";
		os << "[Addr: " << current->data << "] ";
		os << "[Size: " << current->size << " bytes] ";
		os << "[Status: " << (current->allocated ? "ALLOCATED" : "FREE") << "]";

		if (!current->allocated) {
			totalFreeBlocks++;
			totalFreeBytes += current->size;
		}

		os << std::endl;

		current = current->next;
		blockNum++;
	}

	os << "----------------------------------------" << std::endl;
	os << "Total Blocks: " << blockNum << std::endl;
	os << "Free Blocks: " << totalFreeBlocks << std::endl;
	os << "Free Memory: " << totalFreeBytes << " bytes" << std::endl;
	os << "========================================" << std::endl;
}

// ============================================================================
// SINGLETON PATTERN
// ============================================================================

MemoryManager& MemoryManager::GetInstance() {
// Static instance with 1MB default pool size
// Debug mode enabled in debug builds
#ifdef _DEBUG
	static MemoryManager instance(10 * 1024 * 1024, true);  // 10 MB default, debug ON
#else
	static MemoryManager instance(10 * 1024 * 1024, false); // 10 MB default, debug OFF
#endif
	return instance;
}
