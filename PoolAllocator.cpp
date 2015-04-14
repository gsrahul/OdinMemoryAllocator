#include "PoolAllocator.h"

namespace Odin
{

	PoolAllocator::PoolAllocator(Allocator* allocator, size_t element_size,
		size_t element_count, size_t alignment, size_t offset) : mAllocator(allocator),
		mOffset(offset), mAlignment(alignment)
	{
		// Calculate the total size required by this pool
		size_t final_element_size = (element_size + (2 * offset)) & ~(alignment - 1);
		size_t total_size = (final_element_size * element_count) + alignment;

		mSize = total_size;
		mChunkSize = element_size;
	}
	//-------------------------------------------------------------------------------------------
	PoolAllocator::~PoolAllocator()
	{
		ASSERT_ERROR(mCount == 0, "Pool allocator has memory leaks");
		// Destroy the free list and deallocate the memory
		mFreeList->~FreeList();
		mAllocator->deallocate(static_cast<void*>(mStart));
		mFreeList = nullptr;
	}
	//------------------------------------------------------------------------------------------
	bool PoolAllocator::init()
	{
		// Allocate the required memory
		mStart = static_cast<uint8*>(mAllocator->allocate(mSize + sizeof(FreeList), 
				mAlignment, mOffset, 0, 0, 0));
		if (mStart)
		{
			// Initialize the free list. Use placement new
			mFreeList = new(static_cast<void*>(mStart)) FreeList(static_cast<void*>(mStart + sizeof(FreeList)),
				mSize, mChunkSize, mAlignment, mOffset);
			return true;
		}
		return false;
	}
	//------------------------------------------------------------------------------------------
	void* PoolAllocator::allocate(size_t size, size_t alignment, size_t offset,
		const char* file_name, uint32 line, const char* func_name)
	{
		ASSERT_ERROR(mChunkSize == size, "Size of chunk does not match the expected size in pool allocator");
		ASSERT_ERROR(mAlignment == alignment, "Alignment of chunk does not match the expected alignment in pool allocator");
		ASSERT_ERROR(mOffset == offset, "Offset of chunk does not match the expected offset in pool allocator");
		// Increment the number of allocations (Used for getTotalAllocated())
		++mCount;
		return mFreeList->obtainNode();
	}
	//------------------------------------------------------------------------------------------
	void* PoolAllocator::callocate(size_t num_elements, size_t elem_size,
		const char* file_name, uint32 line, const char* func_name)
	{
		return nullptr;
	}
	//------------------------------------------------------------------------------------------
	void PoolAllocator::deallocate(void* ptr)
	{
		if (ptr)
		{
			ASSERT_ERROR(ptr > mStart && ptr < (mStart + mSize), "Chunk returned does not belong to this pool");
			// Decrement the number of allocations (Used for getTotalAllocated())
			--mCount;
			mFreeList->returnNode(ptr);
		}
	}
	//------------------------------------------------------------------------------------------
	size_t PoolAllocator::getAllocSize(void* ptr)
	{
		return mChunkSize;
	}
	//------------------------------------------------------------------------------------------
	size_t PoolAllocator::getTotalAllocated()
	{
		return ((mChunkSize + (2 * mOffset)) * mCount);
	}
}