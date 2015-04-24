#ifndef _GENERAL_ALLOCATOR_H_
#define _GENERAL_ALLOCATOR_H_

#include "DataTypes.h"
#include "Allocator.h"
#include "MemAlloc.h"
#include "SysAlloc.h"
#include <mutex>

namespace Odin
{
	class GeneralAllocator : public Allocator
	{
	public:
		explicit GeneralAllocator(size_t initialSize,
			size_t page_size,
			size_t segment_granularity,
			size_t segment_threshold);
		virtual ~GeneralAllocator();

		// Initialize
		virtual bool init();

		// Allocate the specified amount of memory aligned to default 8-byte alignment
		virtual void* allocate(size_t size, size_t alignment, size_t offset,
			const char* file_name = 0, uint32 line = 0, const char* func_name = 0);

		// Allocate a continuous array of fixed sized elements
		virtual void* callocate(size_t num_elements, size_t elem_size,
			const char* file_name = 0, uint32 line = 0, const char* func_name = 0);

		// Free an allocation previously made with allocate
		virtual void deallocate(void* mem);

		// Return the amount of usable memory allocated at ptr
		virtual size_t getAllocSize(void* mem);

		// Return the total amount of memory allocated by this allocator
		virtual size_t getTotalAllocated();

		// Get the dlmalloc instance index based on size request
		int32 getInstIndexFromSize(size_t size);
	private:
		// Array of mutexes for all the dlmalloc instances
		std::mutex mMutex[21];
		// Array of dlmalloc instances
		MemorySpace* mSpace[21];
	};
}
#endif	// _GENERAL_ALLOCATOR_H_