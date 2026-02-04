#include "services/MemoryManager.h"
#include <new>

// ============================================================================
// GLOBAL NEW/DELETE OPERATOR IMPLEMENTATIONS
// ============================================================================

/*
CREATED BECAUSE OF OPERATOR REDEFINITION ISSUES: C++ forbids declaring global 
operators with dllimport/dllexport

Alternative: when linker sees that operator new has been defined in ANY .cpp file 
(like GlobalMemoryOverride), it automatically uses that definition instead of the 
default one provided by the CRT

So, this file is compiled into every module (Engine DLL, Editor EXE, Runtime EXE)
to ensure 100% memory tracking coverage and there's no need to integrate MM via 
Logger (original approach)
*/

#ifdef _MSC_VER
_Ret_notnull_ _Post_writable_byte_size_(size)
#endif

// Global new operator: routes all allocations through our memory manager
void* operator new(size_t size) {
	void* ptr = MemoryManager::GetInstance().Allocate(static_cast<int>(size));

	if (!ptr) {
		// CRITICAL: Allocation failed but we can't log it (would cause infinite recursion)
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

#ifdef _MSC_VER
_Ret_notnull_ _Post_writable_byte_size_(size)
#endif

// Global new[] operator for arrays
void* operator new[](size_t size) {
	void* ptr = MemoryManager::GetInstance().Allocate(static_cast<int>(size));

	if (!ptr) {
		// CRITICAL: Allocation failed but we can't log it (would cause infinite recursion)
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

// MSVC specific sized deletes (C++17 standard compliance)
void operator delete(void* ptr, size_t) noexcept {
	if (ptr != nullptr) {
		MemoryManager::GetInstance().Deallocate(ptr);
	}
}

void operator delete[](void* ptr, size_t) noexcept {
	if (ptr != nullptr) {
		MemoryManager::GetInstance().Deallocate(ptr);
	}
}
