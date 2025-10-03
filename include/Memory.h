/**
 * @file Memory.h
 * @author Foo Rui Qin
 * @date 2024
 * @brief Memory tracking and management system for debugging memory leaks
 * 
 * This file defines the Memory class which provides a comprehensive memory tracking
 * system for debugging memory leaks and monitoring memory usage. Features include:
 * - Automatic allocation and deallocation tracking with source location
 * - Memory leak detection and reporting
 * - Memory usage statistics (current, peak, total allocations)
 * - Thread-safe operations using mutex protection
 * - Template-based allocation helpers with macro convenience
 * - Singleton pattern for global access
 * - Integration with logging system for detailed debugging
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <mutex>

/**
 * @brief Information structure for tracking memory allocations
 * 
 * Stores metadata about each memory allocation including size,
 * source location (file, line, function), and allocation status.
 */
struct AllocInfo {
	size_t size = 0;        ///< Size of the allocated memory block in bytes
	std::string file;       ///< Source file where allocation occurred
	std::string function;   ///< Function name where allocation occurred
	int line = 0;           ///< Line number where allocation occurred
};

/**
 * @brief Memory tracking and management system
 * 
 * The Memory class provides a singleton-based memory tracking system that monitors
 * all memory allocations and deallocations in the application. It helps detect
 * memory leaks, track memory usage patterns, and provides detailed statistics
 * for debugging and optimization purposes.
 * 
 * Key features:
 * - Thread-safe allocation/deallocation tracking
 * - Memory leak detection and reporting
 * - Real-time memory usage statistics
 * - Source location tracking for debugging
 * - Singleton pattern for global access
 * 
 * Usage example:
 * @code
 * // Use macros for automatic tracking
 * int* ptr = NEW(int);
 * int* array = NEW_ARRAY(int, 100);
 * DELETE(ptr);
 * DELETE_ARRAY(array);
 * 
 * // Check for leaks and statistics
 * Memory::GetInstance().ReportLeaks();
 * Memory::GetInstance().PrintStats();
 * @endcode
 */
class Memory {
public:
	/**
	 * @brief Record a memory allocation with tracking information
	 * @param size Size of the memory block to allocate in bytes
	 * @param file Source file name where allocation occurs
	 * @param line Line number where allocation occurs
	 * @param function Function name where allocation occurs
	 * @return void* Pointer to allocated memory, or nullptr if allocation failed
	 * 
	 * This function allocates memory using malloc and stores tracking information
	 * including source location and size. Updates memory statistics and ensures
	 * thread safety through mutex locking.
	 */
	void* RecordAlloc(size_t size, const char* file, int line, const char* function);

	/**
	 * @brief Record a memory deallocation and update tracking
	 * @param ptr Pointer to memory block to deallocate
	 * 
	 * Finds the allocation record, updates memory statistics, removes the
	 * tracking entry, and frees the memory using free(). Thread-safe operation.
	 */
	void RecordDealloc(void* ptr);

	/**
	 * @brief Generate a report of all memory leaks
	 * 
	 * Scans all tracked allocations that haven't been freed and generates
	 * a detailed report including leak locations, sizes, and total leaked memory.
	 * Outputs results through the logging system.
	 */
	void ReportLeaks() const;

	/**
	 * @brief Print comprehensive memory usage statistics
	 * 
	 * Outputs current memory usage, peak usage, total allocated memory,
	 * and number of active allocations through the logging system.
	 */
	void PrintStats() const;

	/**
	 * @brief Get the singleton instance of the Memory manager
	 * @return Memory& Reference to the singleton Memory instance
	 * 
	 * Ensures only one Memory instance exists throughout the application
	 * lifetime using the singleton pattern.
	 */
	static Memory& GetInstance();

	/**
	 * @brief Get current allocated memory size
	 * @return size_t Current amount of memory in use (bytes)
	 */
	size_t GetCurrentAlloc() const;
	
	/**
	 * @brief Get peak allocated memory size
	 * @return size_t Maximum amount of memory used at any point (bytes)
	 */
	size_t GetPeakAlloc() const;
	
	/**
	 * @brief Get total allocated memory size
	 * @return size_t Lifetime total of all memory ever allocated (bytes)
	 */
	size_t GetTotalAlloc() const;
	
	/**
	 * @brief Get number of active allocations
	 * @return size_t Count of currently tracked allocations
	 */
	size_t GetAllocationCount() const;

private:
	/**
	 * @brief Private constructor for singleton pattern
	 * 
	 * Initializes memory statistics and logs initialization.
	 */
	Memory();
	
	/**
	 * @brief Private destructor
	 * 
	 * Reports any remaining memory leaks if allocations are still active.
	 */
	~Memory();

	/**
	 * @brief Deleted copy constructor to prevent copying
	 */
	Memory(const Memory&) = delete;
	
	/**
	 * @brief Deleted assignment operator to prevent copying
	 */
	Memory& operator=(const Memory&) = delete;

	std::unordered_map<void*, AllocInfo> m_allocs;  ///< Map of active allocations with their tracking info
	mutable std::mutex m_mutex;                     ///< Mutex for thread-safe operations (mutable for const methods)
	size_t m_totalAlloc = 0;                        ///< Lifetime total of all memory ever allocated
	size_t m_currentAlloc = 0;                      ///< Amount of memory currently in use
	size_t m_peakAlloc = 0;                         ///< Maximum amount of memory used at any one time
};

/**
 * @brief Template function for tracked allocation of single objects
 * @tparam T Type of object to allocate
 * @param file Source file name
 * @param line Line number
 * @param function Function name
 * @return T* Pointer to allocated object of type T
 * 
 * Allocates memory for a single object of type T and registers it
 * with the memory tracking system.
 */
template <typename T>
T* TrackedAlloc(const char* file, int line, const char* function) {
	void* ptr = Memory::GetInstance().RecordAlloc(sizeof(T), file, line, function);
	return static_cast<T*>(ptr);
}

/**
 * @brief Template function for tracked allocation of arrays
 * @tparam T Type of array elements
 * @param count Number of elements to allocate
 * @param file Source file name
 * @param line Line number
 * @param function Function name
 * @return T* Pointer to allocated array of type T
 * 
 * Allocates memory for an array of count elements of type T and
 * registers it with the memory tracking system.
 */
template <typename T>
T* TrackedAllocArray(size_t count, const char* file, int line, const char* function) {
	void* ptr = Memory::GetInstance().RecordAlloc(sizeof(T) * count, file, line, function);
	return static_cast<T*>(ptr);
}

/**
 * @defgroup MemoryMacros Memory Tracking Macros
 * @brief Convenience macros for tracked memory operations
 * 
 * These macros automatically capture file, line, and function information
 * for memory tracking without requiring manual parameter passing.
 * @{
 */

/**
 * @brief Allocate memory for a single object with tracking
 * @param type Type of object to allocate
 * @return Pointer to allocated object
 */
#define NEW(type) TrackedAlloc<type>(__FILE__, __LINE__, __FUNCTION__)

/**
 * @brief Allocate memory for an array with tracking
 * @param type Type of array elements
 * @param count Number of elements to allocate
 * @return Pointer to allocated array
 */
#define NEW_ARRAY(type, count) TrackedAllocArray<type>(count, __FILE__, __LINE__, __FUNCTION__)

/**
 * @brief Deallocate memory for a single object with tracking
 * @param ptr Pointer to object to deallocate
 */
#define DELETE(ptr) Memory::GetInstance().RecordDealloc(ptr)

/**
 * @brief Deallocate memory for an array with tracking
 * @param ptr Pointer to array to deallocate
 */
#define DELETE_ARRAY(ptr) Memory::GetInstance().RecordDealloc(ptr)

/** @} */ // end of MemoryMacros group

#endif