#ifndef _MEMORY_ARENA_H_
#define _MEMORY_ARENA_H_

#include "Datatypes.h"
#include "Allocator.h"
#include "BoundsCheckingPolicy.h"
#include "MemoryTrackingPolicy.h"
#include "Assert.h"

namespace Odin
{
	/*
		This class allows you to add any combination of bounds checking and memory tracking policies to your allocations.
		Bear in mind this class's member function calls will cost an additional lock cost and so should be only during
		development.
	*/
	template <class BoundsCheckingPolicy, class MemoryTrackingPolicy>
	class MemoryArena : public Allocator	
	{
		public:
			MemoryArena(const Allocator& allocator) : mAllocator(allocator) {}
			~MemoryArena() {}
			
		// Allocate the specified amount of memory aligned to default 8-byte alignment
		virtual void* allocate(size_t size, size_t offset = 0, 
			const char* file_name = 0, uint32 line = 0, const char* func_name = 0)
		{
			const size_t new_size = original_size + BoundsCheckingPolicy::kSizeFront + BoundsCheckingPolicy::kSizeBack;
			std::lock_guard<std::mutex> lock(mMutex);
			uint8* ptr = static_cast<uint8*>(m_allocator.allocate(new_size, kDefaultAlignment, MemoryTrackingPolicy::kOffset + BoundsCheckingPolicy::kSizeFront,
																		file_name, line, func_name));
			if(ptr == nulptr)
				return nullptr;
			mBoundsChecker.guardFront(ptr + MemoryTrackingPolicy::kOffset);
			mBoundsChecker.guardBack(ptr + MemoryTrackingPolicy::kOffset + BoundsCheckingPolicy::kSizeFront + original_size);
			mMemoryTracker.onAlloc(ptr, new_size, kDefaultAlignment, file_name, line, func_name);
			return static_cast<void*>(ptr + MemoryTrackingPolicy::kOffset + BoundsCheckingPolicy::kSizeFront);
		}

		// Allocate the specified amount of memory aligned to the specified alignment
		virtual void* allocateAligned(size_t size, size_t alignment, size_t offset = 0,
			const char* file_name = 0, uint32 line = 0, const char* func_name = 0)
		{
			const size_t new_size = original_size + BoundsCheckingPolicy::kSizeFront + MemoryTrackingPolicy::kOffset + BoundsCheckingPolicy::kSizeBack;
			std::lock_guard<std::mutex> lock(mMutex);
			uint8* ptr = static_cast<uint8*>(m_allocator.allocateAligned(new_size, alignment, MemoryTrackingPolicy::kOffset + BoundsCheckingPolicy::kSizeFront,
																		file_name, line, func_name));
			if(ptr == nulptr)
				return nullptr;
			mBoundsChecker.guardFront(ptr + MemoryTrackingPolicy::kOffset);
			mBoundsChecker.guardBack(ptr + MemoryTrackingPolicy::kOffset + BoundsCheckingPolicy::kSizeFront + original_size);
			mMemoryTracker.onAlloc(ptr, new_size, kDefaultAlignment, file_name, line, func_name);
			return static_cast<void*>(ptr + BoundsCheckingPolicy::kSizeFront);
		}

		// Allocate a continuous array of fixed sized elements
		virtual void* callocate(size_t num_elements, size_t elem_size,
			const char* file_name = 0, uint32 line = 0, const char* func_name = 0)
		{
			ASSERT_ERROR(num_elements != 0, "Number of elements is 0");
			ASSERT_ERROR(elem_size != 0, "Element size is 0");
			// Round up the size to default 8-byte granularity
        	const size_t new_size = num_elements * elem_size + MemoryTrackingPolicy::kOffset + BoundsCheckingPolicy::kSizeFront + BoundsCheckingPolicy::kSizeBack;
			std::lock_guard<std::mutex> lock(mMutex);
			uint8* ptr = static_cast<uint8*>(m_allocator.callocate(new_size, kDefaultAlignment, MemoryTrackingPolicy::kOffset + BoundsCheckingPolicy::kSizeFront,
																		file_name, line, func_name));
			if(ptr == nulptr)
				return nullptr;
			
			mBoundsChecker.guardFront(ptr + MemoryTrackingPolicy::kOffset);
			mBoundsChecker.guardBack(ptr + MemoryTrackingPolicy::kOffset + BoundsCheckingPolicy::kSizeFront + original_size);
			mMemoryTracker.onAlloc(ptr, new_size, kDefaultAlignment, file_name, line, func_name);
			return static_cast<void*>(ptr + MemoryTrackingPolicy::kOffset + BoundsCheckingPolicy::kSizeFront);
		}

		// Free an allocation previously made with allocate
		virtual void deallocate(void* mem)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			if(mem)
			{
				uint8* ptr = static_cast<uint8*>(mem) - BoundsCheckingPolicy::kSizeFront - MemoryTrackingPolicy::kOffset;
				size_t alloc_size = getAllocSize(ptr);
				mBoundsChecker.checkFront(ptr + MemoryTrackingPolicy::kOffset);
				mBoundsChecker.checkBack(ptr + MemoryTrackingPolicy::kOffset + BoundsCheckingPolicy::kSizeFront + alloc_size);
				mMemoryTracker.onDealloc(ptr);
			}	
			
		}

		// Return the amount of usable memory allocated at ptr
		virtual size_t getAllocSize(void* mem)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			return allocator.getAllocSize(mem);
		}

		// Return the total amount of memory allocated by this allocator
		virtual size_t getTotalAllocated()
		{
			stdd::lock_guard<std::mutex> lock(mMutex);
			return allocator.getTotalAllocated();
		}
			
		private:
			const Allocator& mAllocator;
			BoundsCheckingPolicy mBoundsChecker;
			MemoryTrackingPolicy mMemoryTracker;
			std::mutex mMutex;
	}
}

#endif