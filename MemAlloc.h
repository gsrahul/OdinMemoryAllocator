#include "DataTypes.h"
#include "SysAlloc.h"
#include "CompileOptions.h"
#include <mutex>

#ifndef _MEM_ALLOC_H_
#define _MEM_ALLOC_H_

namespace Odin
{
	// The minimum alignment
	const size_t kDefaultAlignment = 8;

	// Forward declaration
	struct MemoryChunk;
	struct MemoryTreeChunk;

	const uint32 kNumSmallBins = 32;
	const uint32 kNumTreeBins = 32;

	// An opaque type representing an independent region of space that
	// supports Alloc, etc.
	// Internal book keeping for a segment
	struct MemorySpace
	{
		uint32 small_map;							// Bitmap of small chunks
		uint32 tree_map;							// Bitmap of large chunks
		MemoryChunk* dv;							// Designated victim
		MemoryChunk* top;							// Top chunk
		size_t dv_size;								// Size of dv
		size_t top_size;							// Size of top chunk

		MemoryChunk* small_bins[(kNumSmallBins + 1) * 2];

		MemoryTreeChunk* tree_bins[kNumTreeBins];	// Array of tree chunk pointers
		size_t magic;								// Seed value for checksum calculation
		uint8* least_addr;							// The starting address of this segment
		uint32 curr_page_index;						// The current committed page within this segment (Index starts from 1)

		size_t page_size;							// Page size granularity
		size_t segment_granularity;					// Segment size granularity
		size_t segment_threshold;					// Size beyond which an independent call to	
													// ReserveSegmentFunc is made to satisfy allocation

		size_t footprint;
		size_t max_footprint;

		std::mutex memory_lock;						// Mutex
	};

	void* alloc(MemorySpace* msp, size_t bytes);
	void* allocAligned(MemorySpace* msp, size_t alignment, size_t bytes, size_t offset);
	void* calloc(MemorySpace* msp, size_t num_elements, size_t elem_size);

	// This function returns true if it ends up deleting the memory segment when no allocaations exist in it
	bool free(MemorySpace* msp, void* mem);

	// Get the address of MemorySpace from the footer
	MemorySpace* getMemorySpaceAddr(void* mem);
	
	// Get the size of a memory chunk
	size_t getChunkSize(void* mem);

	// Creates and returns a new MemorySpace or NULL on failure
	// The initial size of the segment will be initialSize or segmentGranularity
	// if initialSize is 0.
	// pageSize is the minimum granularity at which memory can be Reserved,
	// Released, Committed and Decommitted. It is also the guaranteed alignment
	// of these operations.
	// segmentGranularity is the size of any given independent segment.
	// segmentThreshold is the size of an allocation request above which an
	// independent call to ReserveSegmentFuncwill be used to satisfy the request,
	// ensuring the allocation request will not be pooled with other allocations
	// within this segment.
	MemorySpace* createMemorySpace(size_t initialSize,
		size_t page_size,
		size_t segment_granularity,
		size_t segment_threshold);

	// Same as CreateMemorySpace but specifies a base segment. If no
	// ReserveSegmentFunc is specified, MemorySpace will not grow beyond the
	// initial size. If Commit and Decommit functions are specified, the
	// base segment will be committed and decommitted like any other segment.
	// This function assumes the segment space is only reserved and pages are not committed
	// (ofcourse, only if commit and decommit functions are not NULL)
	MemorySpace* createMemorySpaceWithBase(void* baseSegment,
		size_t baseSegmentSize,
		size_t page_size,
		size_t segment_granularity,
		size_t segment_threshold);

	// Destroy the given memory space and return it back to the system, returning the number of bytes freed
	size_t destroyMemoryRegion(MemorySpace* msp);

	// Return the current amount of memory allocated from this segment
	size_t getTotalReservedMemory(MemorySpace* msp);

	// Return the highest memory request for this segment
	size_t getMaxReservedMemory(MemorySpace* msp);

	// Return the amount of usable memory in a memory space
	size_t getUsableSize(MemorySpace* msp);

	void validateMemorySpace(MemorySpace* msp);
}
#endif
