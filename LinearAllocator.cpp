#include "LinearAllocator.h"


namespace Odin
{
	LinearAllocator::LinearAllocator(size_t size) : mStart(nullptr), mSize(size)
	{
	}
	//------------------------------------------------------------------------------------------
	LinearAllocator::LinearAllocator(void* start, size_t size) : mStart(static_cast<uint8*>(start)),
		mSize(size)
	{
	}
	//------------------------------------------------------------------------------------------
	bool LinearAllocator::init()
	{
		if (mStart == nullptr)
		{
			// Align the size to page granularity
			// Hardcode it for now to 64kB
			size_t granularity = 65536;
			size_t size = (mSize + (granularity - 1)) & ~(granularity - 1);
			mStart = static_cast<uint8*>(SysAlloc::reserveCommitSegment(size));
			if (!mStart)
				return false;
			mCurrent = mStart;
		}
		return true;
	}
	//------------------------------------------------------------------------------------------
	LinearAllocator::~LinearAllocator()
	{
		ASSERT_ERROR(getTotalAllocated() == 0, "Linear allocator has memory leaks");
		if (mStart)
		{
			SysAlloc::releaseSegment(static_cast<void*>(mStart), mSize);
			mStart = mCurrent = nullptr;
		}
	}
	//------------------------------------------------------------------------------------------
	void* LinearAllocator::allocate(size_t size, size_t alignment, size_t offset,
		const char* file_name, uint32 line, const char* func_name)
	{
		// Make sure the alignment is a power of 2
		ASSERT_ERROR(((alignment & (alignment - 1)) == 0), "Alignment is not a power of 2");
		if (alignment < kDefaultAlignment)
			alignment = kDefaultAlignment;
		// Add sizeof(size_t) to offset for storing the chunk size
		offset += sizeof(size_t);
		// Offset pointer first, align it, and offset it back
		mCurrent += offset;
		// Get the nearest aligned address
		mCurrent = reinterpret_cast<uint8*>(((reinterpret_cast<size_t>(mCurrent)+
			(alignment - 1)) & ~(alignment - 1)));
		// Now subtract the offset
		mCurrent -= offset;

		uint8* user_ptr = mCurrent;
		mCurrent += size;

		if (mCurrent >= (mStart + mSize))
		{
			// We're out of memory
			return nullptr;
		}

		// Store the size
		*(reinterpret_cast<size_t*>(user_ptr)) = size;
		user_ptr += sizeof(size_t);

		// Make sure the pointer to be returned confirms to the requested alignment
		ASSERT_ERROR(((reinterpret_cast<size_t>(user_ptr) & (alignment - 1)) == 0),
			"Pointer to be returned is not %d aligned", alignment);

		return static_cast<void*>(user_ptr);
	}
	//------------------------------------------------------------------------------------------
	void* LinearAllocator::callocate(size_t num_elements, size_t elem_size,
		const char* file_name, uint32 line, const char* func_name)
	{
		return nullptr;
	}
	//------------------------------------------------------------------------------------------
	void LinearAllocator::deallocate(void* mem)
	{
		// This function is empty
	}
	//------------------------------------------------------------------------------------------
	void LinearAllocator::reset(void)
	{
		// Reset the pointer to the start
		mCurrent = mStart;
	}
	//------------------------------------------------------------------------------------------
	size_t LinearAllocator::getAllocSize(void* mem)
	{
		uint8* ptr = static_cast<uint8*>(mem);
		ptr -= sizeof(size_t);
		return *(reinterpret_cast<size_t*>(ptr));
	}
	//------------------------------------------------------------------------------------------
	size_t LinearAllocator::getTotalAllocated()
	{
		return (mCurrent - mStart);
	}
}