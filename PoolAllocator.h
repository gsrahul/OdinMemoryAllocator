#ifndef _POOL_ALLOCATOR_H_
#define _POOL_ALLOCATOR_H_

#include "DataTypes.h"
#include "Allocator.h"
#include "FreeList.h"
#include "Assert.h"

namespace Odin
{
	class PoolAllocator : public Allocator
	{
	public:
		PoolAllocator(Allocator* allocator, size_t element_size, 
			size_t element_count,size_t alignment, size_t offset);
		virtual ~PoolAllocator();

		// Initialize
		virtual bool init();
		// Allocate memory
		virtual void* allocate(size_t size, size_t alignment, size_t offset,
			const char* file_name = 0, uint32 line = 0, const char* func_name = 0);

		// Allocate a continuous array of fixed sized elements
		virtual void* callocate(size_t num_elements, size_t elem_size,
			const char* file_name = 0, uint32 line = 0, const char* func_name = 0);
		
		// Function to free memory
		virtual void deallocate(void* mem);

		// Return the amount of usable memory allocated at ptr
		virtual size_t getAllocSize(void* ptr);

		// Return the total amount of memory allocated by this allocator
		virtual size_t getTotalAllocated();

		// Return the starting address of the pool memory area
		const uint8* getStartAddress() { return mStart; }
	private:
		// The total size of memory space
		size_t mSize;
		// Size of a chunk
		size_t mChunkSize;
		// Alignment
		size_t mAlignment;
		// Number of elements
		size_t mCount;
		// Offset
		size_t mOffset;
		// A Free list allocator
		FreeList *mFreeList;
		// The allocator to be used
		Allocator* mAllocator;
		// The starting address of Memory Space
		uint8* mStart;
	};
}

#endif	// _POOL_ALLOCATOR_H_
