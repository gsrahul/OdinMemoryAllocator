#include "FreeList.h"

namespace Odin
{
	FreeList::FreeList(void* start, size_t size, size_t element_size, size_t alignment, size_t offset)
	{
		ASSERT_ERROR(((alignment & (alignment - 1)) == 0), "Alignment is not a power of 2");

		// Add the guard bytes size 
		size_t final_element_size = (2 * offset) + element_size;

		uint8* aligned_ptr = static_cast<uint8*>(start);
		// Offset pointer first, align it, and offset it back
		aligned_ptr += offset;
		aligned_ptr = reinterpret_cast<uint8*>(((reinterpret_cast<size_t>(aligned_ptr)+
			(alignment - 1)) & ~(alignment - 1)));
		// Now subtract the offset
		aligned_ptr -= offset;

		// Initialize the memory pool
		size_t element_count = ((size - (aligned_ptr - start)) / final_element_size) - 1;
		for (size_t element_index = 0; element_index < element_count; ++element_index)
		{
			uint8* curr_element = static_cast<uint8*>(aligned_ptr)+(element_index * final_element_size);
			*(reinterpret_cast<uint8**>(curr_element)) = curr_element + element_count;
		}
		// Set the last node's link to NULL
		*(reinterpret_cast<uint8**>(&(static_cast<uint8*>(aligned_ptr)[element_count * final_element_size]))) = nullptr;
		// Set the HEAD
		mNext = static_cast<uint8*>(aligned_ptr);
	}
	//-----------------------------------------------------------------------------------------------
	FreeList::~FreeList()
	{
	}
	//-----------------------------------------------------------------------------------------------
	void* FreeList::obtainNode(void)
	{
		// Are there any free nodes?
		if (mNext == nullptr)
		{
			// The free list is empty
			return nullptr;
		}

		// Obtain the node at the tip of the free list
		uint8* curr_ptr = mNext;
		mNext = *(reinterpret_cast<uint8**>(mNext));
		return static_cast<void*>(curr_ptr);
	}
	//-----------------------------------------------------------------------------------------------
	void FreeList::returnNode(void* ptr)
	{
		// Add the returned node to the tip of the free list
		*(reinterpret_cast<uint8**>(ptr)) = mNext;
		mNext = static_cast<uint8*>(ptr);
	}
}