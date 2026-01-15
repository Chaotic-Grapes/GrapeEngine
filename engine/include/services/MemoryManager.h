#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <iostream>
#include <list>
#include <memory>

class MemoryManager {
private:
	void* memoryPool;

	struct MemoryBlock {
		void* data;
		int size;
		bool allocated;
		MemoryBlock(void* ptr, int size);
	};

	std::list<MemoryBlock> memoryBlock;

public:
	MemoryManager(int totalBytes);
	~MemoryManager();
	void* allocate(int size);
	void deallocate(void* ptr);
	void dump(std::ostream& os);

	// OVERLOAD GLOBAL NEW AND DELETE OPERATORS
	void* operator new(size_t size) {
		std::cout << "Overloading new operator with size: " << size << std::endl;
		void* ptr = malloc(size);
		return ptr;
	}

	void operator delete(void* ptr) {
		std::cout << "Overloading delete operator" << std::endl;
		free(ptr);
	}
};

#endif