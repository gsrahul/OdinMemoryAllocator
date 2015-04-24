#include "GeneralAllocator.h"

namespace Odin
{
	GeneralAllocator::GeneralAllocator(size_t initialSize,
		size_t page_size,
		size_t segment_granularity,
		size_t segment_threshold)
	{
	}
	//------------------------------------------------------------------------------------------
	GeneralAllocator::~GeneralAllocator()
	{
		for (uint32 i = 0; i < 21; ++i)
		{
			if (mSpace[i] != nullptr)
			{
				destroyMemoryRegion(mSpace[i]);
			}
		}
	}
	//------------------------------------------------------------------------------------------
	bool GeneralAllocator::init()
	{
		// This allocator creates 20 dlmalloc instances covering allocations at every 8 byte
		// size interval less than 64 bytes and every 16 byte interval between 64 and 256 bytes.
		// And it creates one dlmalloc instance for allocations larger than 256 bytes.
		for (int32 i = 0; i < 20; ++i)
		{
			// The dlmalloc instances for allocations less than 256 bytes will have a segment size of 
			// 64KB and a page size of 64KB too
			mSpace[i] = createMemorySpace(65536,
				65536, 65536, 8192);
			if (mSpace[i] == NULL)
				return false;
		}
		// The instance for allocations larger than 256 bytes will have a segment size of 32MB
		// and a page size of 64KB
		mSpace[20] = createMemorySpace(65536,
			65536, 33554432, 8388608);
		if (mSpace[20] == NULL)
			return false;

		return true;
	}
	//------------------------------------------------------------------------------------------
	void* GeneralAllocator::allocate(size_t size, size_t alignment, size_t offset,
		const char* file_name, uint32 line, const char* func_name)
	{
		// Get the index of the dlmalloc instance based on the request size
		int32 index = getInstIndexFromSize(size);
		if (index > -1)
		{
			std::lock_guard<std::mutex> guard(mMutex[index]);
			return allocAligned(mSpace[index], alignment, size, offset);
		}
		else
			return nullptr;
	}
	//-----------------------------------------------------------------------------------------
	void* GeneralAllocator::callocate(size_t num_elements, size_t elem_size,
		const char* file_name, uint32 line, const char* func_name)
	{
		// Get the index of the dlmalloc instance based on the request size
		int32 index = getInstIndexFromSize(elem_size);
		if (index > -1)
		{
			std::lock_guard<std::mutex> guard(mMutex[index]);
			return calloc(mSpace[index], num_elements, elem_size);
		}
		else
			return nullptr;
	}
	//-----------------------------------------------------------------------------------------
	void GeneralAllocator::deallocate(void* mem)
	{
		if (mem)
		{
			// Get the address of the MemorySpace from the footer
			MemorySpace* msp = getMemorySpaceAddr(mem);
			if (msp)
			{
				free(msp, mem);
			}
			else
			{
				// This address was allocated from one of the segments with size 
				// request greater than threshold. So just call free from 
				// the first available segment.
				for (uint32 i = 0; i < 21; ++i)
				{
					std::lock_guard<std::mutex> guard(mMutex[i]);
					if (mSpace[i])
					{
						free(mSpace[i], mem);
						break;
					}
				}
			}
		}
	}
	//-----------------------------------------------------------------------------------------
	size_t GeneralAllocator::getAllocSize(void* mem)
	{
		return getChunkSize(mem);
	}
	//-----------------------------------------------------------------------------------------
	size_t GeneralAllocator::getTotalAllocated()
	{
		size_t total_footprint = 0;
		for(uint32 i = 0; i < 21; ++i)
		{
			std::lock_guard<std::mutex> guard(mMutex[i]);
			if(mSpace[i])
				total_footprint += mSpace[i]->footprint;
		}
		return total_footprint;
	}
	//-----------------------------------------------------------------------------------------
	int32 GeneralAllocator::getInstIndexFromSize(size_t size)
	{
		uint32 index = 0;
		if (size < 64)
			index = size >> 3;
		else if (size >= 64 && size < 256)
			uint32 index = (size >> 4) + 4; // Since 16 byte allocations are housed between index 8 and index 18
		else
			// Allocation request is greater than of equal to 256
			index = 20;

		// Lock the particular dlmalloc instance
		std::lock_guard<std::mutex> guard(mMutex[index]);
		// See if the segment exists. Since, each segment is released as soon as it contains no
		// allocations, this particular segment may not exist
		if (mSpace[index])
			return index;
		else
		{
			// Segment doesn't exist, create it again
			if (index == 20)
			{
				mSpace[index] = createMemorySpace(65536,
					65536, 33554432, 8388608);
				if (mSpace[index])
					// Created the segment, go ahead and return the index
					return index;
				else
					// Something went wrong, return -1
					return -1;
			}
			else
			{
				// Segment doesn't exist, create it again
				mSpace[index] = createMemorySpace(65536,
					65536, 65536, 8192);
				if (mSpace[index])
					return index;
				else
					// Something went wrong, return -1
					return -1;
			}
		}
	}
}