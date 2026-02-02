/* Start Header *****************************************************************/
/*!
\file   MemoryManager.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   25th January 2026
\brief
Implements the custom MemoryManager class for efficient memory allocation
- Uses embedded headers (MemoryBlock) within the memory pool
- Maintains a FREE LIST only (FreeBlock) for O(1) allocation in best cases
- Address-ordered free linked list for coalescing
- First-fit strategy: Finds first free block large enough
- Block splitting: Divides blocks when larger than requested
- Adjacent merging: Combines neighboring free blocks (coalescing)
- Singleton pattern: Single global instance for entire game
*/
/* End Header *******************************************************************/

#include "services/MemoryManager.h"
#include <cstdlib>
#include <cstring>
#include <vector>
#include <chrono>
#include <random>
#include <ctime>
#include <new> 

// ============================================================================
// CONSTRUCTORS
// ============================================================================

MemoryManager::MemoryPage::MemoryPage(void* start, int size)
	: pageStart(start), pageSize(size), next(nullptr) {}

// ============================================================================
// MEMORY MANAGER IMPLEMENTATION
// ============================================================================

MemoryManager::MemoryManager(int totalBytes, bool debugMode)
	: m_freeListHead(nullptr), m_pageListHead(nullptr), m_defaultPageSize(totalBytes),
	m_totalAllocated(0), m_totalFreed(0), m_debugMode(debugMode) 
{
	// Allocate the first memory page using raw malloc
	void* memoryPool = malloc(totalBytes);

	if (!memoryPool) {
		// CRITICAL: Failed to allocate initial pool
		return;
	}

	// Fill with unallocated pattern if debug mode is enabled
	if (m_debugMode) memset(memoryPool, UNALLOCATED_PATTERN, totalBytes);

	// Create the first page node (metadata for the page itself)
	MemoryPage* firstPage = (MemoryPage*)malloc(sizeof(MemoryPage));
	if (!firstPage) {
		free(memoryPool);
		return;
	}

	firstPage->pageStart = memoryPool;
	firstPage->pageSize = totalBytes;
	firstPage->next = nullptr;
	m_pageListHead = firstPage;

	// Initialize the first free block at the start of the pool
	// We use "placement" style initialization (writing directly to memory)
	FreeBlock* initialBlock = (FreeBlock*)memoryPool;
	
	// Set header
	initialBlock->header.size = totalBytes - sizeof(MemoryBlock); // Remainder is data
	initialBlock->header.allocated = false;
	
	// Set free list pointers
	initialBlock->next = nullptr;
	initialBlock->prev = nullptr;

	// Set as head of the free list
	m_freeListHead = initialBlock;
}

MemoryManager::~MemoryManager() {
	// Free all memory pages
	MemoryPage* currentPage = m_pageListHead;
	while (currentPage != nullptr) {
		MemoryPage* nextPage = currentPage->next;
		
		// Free the actual memory pool
		free(currentPage->pageStart);
		
		// Free the page metadata
		free(currentPage);
		currentPage = nextPage;
	}
}

// ============================================================================
// ALLOCATION AND DEALLOCATION
// ============================================================================

void* MemoryManager::Allocate(int size) {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	static thread_local int recursionDepth = 0;
	if (recursionDepth > 5) return nullptr;
	recursionDepth++;

	// Align size to 16 bytes to ensure proper alignment
	// AND ensure we have enough space for FreeBlock pointers when freed (16 bytes min)
	const int ALIGNMENT = 16;
	size_t alignedSize = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
	if (alignedSize < ALIGNMENT) alignedSize = ALIGNMENT; // Min size = 16

	// First-fit strategy: Find first free block that fits
	// We only traverse the FREE LIST, which is much faster than traversing all blocks
	FreeBlock* current = m_freeListHead;

	while (current != nullptr) {
		if (current->header.size >= alignedSize) {
			// Found a suitable block

			// Check if we can split the block
			// We need enough space for the requested size + a new header + at least min data size (16)
			size_t requiredSpace = alignedSize + sizeof(MemoryBlock) + ALIGNMENT;

			if (current->header.size >= requiredSpace) {
				// SPLIT the block
				
				// 1. Calculate split point
				size_t originalSize = current->header.size;
				
				// New block (remainder) starts after the allocated part
				// Address = current + HeaderSize + AlignedSize
				char* splitAddress = (char*)current + sizeof(MemoryBlock) + alignedSize;
				FreeBlock* newBlock = (FreeBlock*)splitAddress;

				// 2. Initialize new block (remainder)
				newBlock->header.size = originalSize - alignedSize - sizeof(MemoryBlock);
				newBlock->header.allocated = false;
				
				// 3. Update Free List
				// Instead of removing current and inserting new, we can just replace 'current' with 'newBlock' in the list
				// This maintains the address order naturally!
				newBlock->next = current->next;
				newBlock->prev = current->prev;
				
				if (newBlock->next != nullptr) newBlock->next->prev = newBlock;
				if (newBlock->prev != nullptr) newBlock->prev->next = newBlock;
				
				// If current was head, update head
				if (current == m_freeListHead) m_freeListHead = newBlock;

				// 4. Update current block (allocated part)
				current->header.size = alignedSize;
			}
			else {
				// EXACT FIT (or close enough not to split)
				// Remove current from the free list
				if (current->prev != nullptr) current->prev->next = current->next;
				if (current->next != nullptr) current->next->prev = current->prev;
				
				// If current was head, update head
				if (current == m_freeListHead) m_freeListHead = current->next;
			}

			// Mark as allocated
			current->header.allocated = true;
			m_totalAllocated += current->header.size;

			// Pointer to the data (immediately after header)
			void* dataPtr = (char*)current + sizeof(MemoryBlock);

			// Fill with allocated pattern
			if (m_debugMode) memset(dataPtr, ALLOCATED_PATTERN, current->header.size);

			recursionDepth--;
			return dataPtr;
		}

		current = current->next;
	}

	// If we reach here, no suitable block found
	_extendMemoryPool();

	// Try again
	void* result = Allocate(size);
	recursionDepth--;
	return result;
}

void MemoryManager::Deallocate(void* ptr) {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (ptr == nullptr) return;

	// Retrieve the block header (located immediately before the data)
	// We cast to FreeBlock* because we are about to treat it as one
	FreeBlock* block = (FreeBlock*)((char*)ptr - sizeof(MemoryBlock));

	// Sanity check
	if (!block->header.allocated) {
		// Double free or corruption
		return;
	}

	// Fill with freed pattern
	if (m_debugMode) memset(ptr, FREED_PATTERN, block->header.size);

	// Mark as free
	block->header.allocated = false;
	m_totalFreed += block->header.size;

	// Insert back into the free list (address ordered)
	_insertBlockAddressOrdered(block);

	// Merge with adjacent free blocks
	_mergeAdjacentFreeBlocks(block);
}

// ============================================================================
// MEMORY POOL EXTENSION
// ============================================================================

void MemoryManager::_extendMemoryPool() {
	// Allocate a new memory page
	void* newPageMemory = malloc(m_defaultPageSize);
	if (!newPageMemory) return;

	if (m_debugMode) memset(newPageMemory, UNALLOCATED_PATTERN, m_defaultPageSize);

	MemoryPage* newPage = (MemoryPage*)malloc(sizeof(MemoryPage));
	if (!newPage) {
		free(newPageMemory);
		return;
	}
	newPage->pageStart = newPageMemory;
	newPage->pageSize = m_defaultPageSize;
	newPage->next = nullptr;

	// Add to page list
	MemoryPage* lastPage = m_pageListHead;
	while (lastPage->next != nullptr) lastPage = lastPage->next;
	lastPage->next = newPage;

	// Create a new free block for this page
	FreeBlock* newBlock = (FreeBlock*)newPageMemory;
	newBlock->header.size = m_defaultPageSize - sizeof(MemoryBlock);
	newBlock->header.allocated = false;
	newBlock->next = nullptr;
	newBlock->prev = nullptr;
	
	// Insert into free list (it will likely be at the end, but use ordered insert to be safe)
	_insertBlockAddressOrdered(newBlock);
}

// ============================================================================
// ADDRESS-ORDERED INSERTION
// ============================================================================

void MemoryManager::_insertBlockAddressOrdered(FreeBlock* block) {
	if (block == nullptr) return;

	// Case 1: Empty list
	if (m_freeListHead == nullptr) {
		m_freeListHead = block;
		block->next = nullptr;
		block->prev = nullptr;
		return;
	}

	// Case 2: Insert at head (address is smaller than head)
	if (block < m_freeListHead) {
		block->next = m_freeListHead;
		block->prev = nullptr;
		m_freeListHead->prev = block;
		m_freeListHead = block;
		return;
	}

	// Case 3: Insert somewhere in the middle or end
	FreeBlock* current = m_freeListHead;
	
	// Find the node after which we should insert
	// We want: current < block < current->next
	while (current->next != nullptr && current->next < block) {
		current = current->next;
	}

	// Insert after current
	block->next = current->next;
	block->prev = current;

	if (current->next != nullptr) {
		current->next->prev = block;
	}
	current->next = block;
}

// ============================================================================
// FRAGMENTATION MANAGEMENT
// ============================================================================

void MemoryManager::_mergeAdjacentFreeBlocks(FreeBlock* block) {
	// Try merging with NEXT block
	if (block->next != nullptr) {
		// Check physical adjacency
		// Address of next block should be: (char*)block + sizeof(Header) + block->size
		char* nextBlockAddr = (char*)block + sizeof(MemoryBlock) + block->header.size;
		
		if ((void*)nextBlockAddr == (void*)block->next) {
			FreeBlock* nextBlock = block->next;
			
			// Absorb next block
			block->header.size += sizeof(MemoryBlock) + nextBlock->header.size;
			
			// Update links to skip nextBlock
			block->next = nextBlock->next;
			if (block->next != nullptr) {
				block->next->prev = block;
			}
			
			// nextBlock is now gone (merged)
		}
	}

	// Try merging with PREV block
	if (block->prev != nullptr) {
		// Check physical adjacency
		char* currentBlockAddr = (char*)block->prev + sizeof(MemoryBlock) + block->prev->header.size;
		
		if ((void*)currentBlockAddr == (void*)block) {
			FreeBlock* prevBlock = block->prev;
			
			// Absorb current block into previous
			prevBlock->header.size += sizeof(MemoryBlock) + block->header.size;
			
			// Update links to skip block
			prevBlock->next = block->next;
			if (prevBlock->next != nullptr) {
				prevBlock->next->prev = prevBlock;
			}
			
			// block is now gone (merged)
		}
	}
}

// ============================================================================
// DEBUGGING AND UTILITY
// ============================================================================

void MemoryManager::SetDebugMode(bool enabled) {
	m_debugMode = enabled;
}

void MemoryManager::Dump(std::ostream& os) {
	os << "========================================" << std::endl;
	os << "MEMORY MANAGER STATE (Free List Only)" << std::endl;
	os << "========================================" << std::endl;
	os << "Total Allocated: " << m_totalAllocated << " bytes" << std::endl;
	os << "Total Freed: " << m_totalFreed << " bytes" << std::endl;
	os << "Currently In Use: " << (m_totalAllocated - m_totalFreed) << " bytes" << std::endl;
	
	// Count free blocks
	int freeBlockCount = 0;
	size_t freeMemory = 0;
	FreeBlock* current = m_freeListHead;
	while (current != nullptr) {
		freeBlockCount++;
		freeMemory += current->header.size;
		current = current->next;
	}
	
	os << "Free Blocks Count: " << freeBlockCount << std::endl;
	os << "Total Free Memory: " << freeMemory << " bytes" << std::endl;
	os << "----------------------------------------" << std::endl;
	
	// Print first 20 free blocks to avoid spam
	current = m_freeListHead;
	int count = 0;
	while (current != nullptr && count < 20) {
		os << "Free Block #" << count << ": [Addr: " << current << "] [Size: " << current->header.size << "]" << std::endl;
		current = current->next;
		count++;
	}
	if (current != nullptr) os << "... (more blocks hidden)" << std::endl;
}

double MemoryManager::Benchmark(bool useCustom, int allocationCount, int minSize, int maxSize) {
	// Allocate test arrays using malloc/free to avoid interference
	void** pointers = (void**)malloc(allocationCount * sizeof(void*));
	int* sizes = (int*)malloc(allocationCount * sizeof(int));

	// Sanity check
	if (!pointers || !sizes) {
		if (pointers) free(pointers);
		if (sizes) free(sizes);
		return -1.0;
	}

	// Initialize random sizes
	srand(static_cast<unsigned int>(time(NULL)));

	// Generate random sizes for each allocation
	for (int i{}; i < allocationCount; i++) {
		sizes[i] = minSize + (rand() % (maxSize - minSize + 1));
		pointers[i] = nullptr;
	}

	// Start timing
	auto start = std::chrono::high_resolution_clock::now();

	if (useCustom) {
		// Test Custom Allocator
		for (int i{}; i < allocationCount; i++) pointers[i] = Allocate(sizes[i]);
		for (int i{}; i < allocationCount; i++) if (pointers[i]) Deallocate(pointers[i]);
	}
	else {
		// Test Standard Allocator (malloc/free)
		// MUST use malloc/free to avoid custom allocator interference
		// If we used new/delete, we would just be benchmarking our custom allocator against itself
		for (int i{}; i < allocationCount; i++) pointers[i] = malloc(sizes[i]);
		for (int i{}; i < allocationCount; i++) if (pointers[i]) free(pointers[i]);
	}

	// Stop timing
	auto end = std::chrono::high_resolution_clock::now();

	// Calculate elapsed time in milliseconds
	std::chrono::duration<double, std::milli> elapsed = end - start;

	// Free test arrays
	free(pointers);
	free(sizes);

	// Return elapsed time
	return elapsed.count();
}

MemoryManager& MemoryManager::GetInstance() {
	// Static instance with 10 MB default pool size
	static MemoryManager instance(10 * 1024 * 1024, false);
	return instance;
}
