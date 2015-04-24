#ifndef _LINEAR_ALLOCATOR_H_
#define _LINEAR_ALLOCATOR_H_

#include "DataTypes.h"
#include "Assert.h"
#include "SysAlloc.h"
#include "Allocator.h"

namespace Odin
{
	// The memory region handled by this allocator can never grow. Also, individual
	// allocations can never be freed. All allocations are freed at once by calling "reset".
	// "free" is just an empty function.
	class LinearAllocator : public Allocator
	{
	public:
		// Allocate virtual memory for the given size
		explicit LinearAllocator(size_t size);
		virtual ~LinearAllocator();

		// Initialize the allocator
		virtual bool init();

		// Use the allocated Memory Space passed to perform all allocations
		LinearAllocator(void* start, size_t size);
		
		// Allocate memory
		virtual void* allocate(size_t size, size_t alignment, size_t offset,
			const char* file_name = 0, uint32 line = 0, const char* func_name = 0);
		
		// Allocate a continuous array of fixed sized elements
		virtual void* callocate(size_t num_elements, size_t elem_size,
			const char* file_name = 0, uint32 line = 0, const char* func_name = 0);
		
		// Function to free memory
		virtual void deallocate(void* mem);
		
		// Return the amount of usable memory allocated at ptr
		virtual size_t getAllocSize(void* mem);
		
		// Return the total amount of memory allocated by this allocator
		virtual size_t getTotalAllocated();

		// Function to clear all the allocated memory
		void reset(void);
	private:
		// The total size of memory Space
		size_t mSize;
		// The starting address of Memory Space
		uint8* mStart;
		// The current starting address of the free chunk
		uint8* mCurrent;
	};
}

#endif	_LINEAR_ALLOCATOR_H_