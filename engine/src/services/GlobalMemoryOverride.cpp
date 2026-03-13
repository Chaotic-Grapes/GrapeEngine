/* Start Header *****************************************************************/
/*!
\file   GlobalMemoryOverride.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
Global operator new/delete overrides that route all heap allocations through
GrapeEngine's MemoryManager. This file is compiled into every module (Engine DLL,
Editor EXE, Runtime EXE) to ensure 100% memory tracking coverage.

C++ forbids declaring global operators with dllimport/dllexport, so they cannot
live inside the engine DLL's exported API directly. Instead, when the linker sees
operator new defined in any .cpp file it automatically prefers that definition over
the CRT default, giving us engine-wide tracking without any per-module integration work.
*/
/* End Header *******************************************************************/

#include "services/MemoryManager.h"
#include <new>

// ============================================================================
// Global new/delete operator implementations
// ============================================================================

// MSVC SAL annotation: marks the return pointer as non-null and writable for size bytes;
// placed before each operator new so the static analyser treats the return value correctly
#ifdef _MSC_VER
_Ret_notnull_ _Post_writable_byte_size_(size)
#endif

// Route all single-object allocations through the memory manager;
// throwing bad_alloc on failure is required by the standard so callers
// using new without a nothrow tag get the expected exception behaviour;
// logging is intentionally skipped here because the logger itself uses new,
// and logging inside the allocator would cause infinite recursion
void* operator new(size_t size) {
	void* ptr = MemoryManager::GetInstance().Allocate(static_cast<int>(size));

	if (!ptr) {
		throw std::bad_alloc();
	}

	return ptr;
}

// Route all single-object deallocations through the memory manager;
// null guard matches the standard's requirement that delete on nullptr is a no-op
void operator delete(void* ptr) noexcept {
	if (ptr != nullptr) {
		MemoryManager::GetInstance().Deallocate(ptr);
	}
}

#ifdef _MSC_VER
_Ret_notnull_ _Post_writable_byte_size_(size)
#endif

// Array form of operator new; same allocation path and recursion risk as the scalar version
void* operator new[](size_t size) {
	void* ptr = MemoryManager::GetInstance().Allocate(static_cast<int>(size));

	if (!ptr) {
		throw std::bad_alloc();
	}

	return ptr;
}

// Array form of operator delete; mirrors the scalar delete for symmetry
void operator delete[](void* ptr) noexcept {
	if (ptr != nullptr) {
		MemoryManager::GetInstance().Deallocate(ptr);
	}
}

// ============================================================================
// C++17 sized delete overloads (MSVC compliance)
// ============================================================================

// Sized scalar delete: C++17 allows the compiler to pass the original allocation size;
// we ignore the size parameter because our allocator tracks size internally
void operator delete(void* ptr, size_t) noexcept {
	if (ptr != nullptr) {
		MemoryManager::GetInstance().Deallocate(ptr);
	}
}

// Sized array delete: same as sized scalar delete but for array allocations
void operator delete[](void* ptr, size_t) noexcept {
	if (ptr != nullptr) {
		MemoryManager::GetInstance().Deallocate(ptr);
	}
}
