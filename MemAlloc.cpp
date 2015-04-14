#include "DataTypes.h"
#include "MemAlloc.h"
#include "Assert.h"

#if ODIN_COMPILER == ODIN_COMPILER_MSVC
// Store the current warning settings
#   pragma warning (push)
// Turn off compiler warning for applying unary minus operator to unsigned type
#pragma warning (disable : 4146)
#endif

namespace Odin
{
	// Constants declaration
	const uint32 kSmallBinShift = 3;
	const uint32 kTreeBinShift = 8;
	const size_t kSizeTBitSize = sizeof(size_t) << 3;
	// Alignment mask
	const size_t kAlignmentMask = (kDefaultAlignment - 1);
	// Chunk overhead for any allocation
	const size_t kChunkOverhead = sizeof(size_t) << 1;
	const size_t kMinLargeSize = 1 << kTreeBinShift;
	const size_t kMaxSmallSize = kMinLargeSize - 1;
	const size_t kMaxSmallRequest = kMaxSmallSize - kAlignmentMask - kChunkOverhead;

	// Previous in_use and current in_use bits
	const size_t kPinuseBit = 0x1;
	const size_t kCinuseBit = 0x2;
	const size_t kInuseBits = kPinuseBit | kCinuseBit;
	//---------------------------------------------------------------------------------------------------------------
	// A chunk of memory maintained using boundary tag
	struct MemoryChunk
	{
		size_t prev_foot;	// Size of the previous block (if free)
		size_t head;		// Size of this chunk
		MemoryChunk* fd;	// Pointer to the next chunk
		MemoryChunk* bk;	// Pointer to the previous chunk
	};

	struct MemoryTreeChunk
	{
		size_t prev_foot;	// Size of the previous block (if free)
		size_t head;		// Size of this chunk
		MemoryTreeChunk* fd;		// Pointer to the next chunk
		MemoryTreeChunk* bk;		// Pointer to the previous chunk

		MemoryTreeChunk* child[2];	// The two children of this node
		MemoryTreeChunk* parent;	// Parent of this node
		uint32 index;				// Index in the tree bin
	};

	const size_t kMinChunkSize = (sizeof(MemoryChunk)+kAlignmentMask)  & ~kAlignmentMask;

	// Lower and upper bound on request size (not chunk size)
	const size_t kMaxRequest = (-kMinChunkSize) << 2;
	const size_t kMinRequest = kMinChunkSize - kChunkOverhead - 1;

	// Pad requested number of bytes into a usable size
	static FORCEINLINE size_t padRequest(size_t size)
	{
		return ((size + kChunkOverhead + kAlignmentMask) & ~kAlignmentMask);
	}

	// Pad request, checking for minimum or maximum
	static FORCEINLINE size_t requestToSize(size_t size)
	{
		if (size < kMinChunkSize)
			return kMinChunkSize;
		else
			return padRequest(size);
	}

	// Extraction of size
	static FORCEINLINE size_t chunkSize(MemoryChunk* ptr)
	{
		return (ptr->head & ~(kInuseBits));
	}
	static FORCEINLINE size_t chunkSize(MemoryTreeChunk* ptr)
	{
		return (ptr->head & ~(kInuseBits));
	}

	// Move the chunk pointer by an offset and treat the new address as a chunk pointer
	static FORCEINLINE MemoryChunk* chunkPlusOffset(MemoryChunk* ptr, size_t offset)
	{
		return reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(ptr)+offset);
	}
	static FORCEINLINE MemoryChunk* chunkMinusOffset(MemoryChunk* ptr, size_t offset)
	{
		return reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(ptr)-offset);
	}

	// Get the pointer to the next chunk
	static FORCEINLINE MemoryChunk* nextChunk(MemoryChunk* ptr)
	{
		return reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(ptr)+(ptr->head & ~(kInuseBits)));
	}

	// Get the pointer to the previous chunk
	static FORCEINLINE MemoryChunk* previousChunk(MemoryChunk* ptr)
	{
		return reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(ptr)-(ptr->prev_foot));
	}

	// A helper function to access tree children
	static FORCEINLINE MemoryTreeChunk* leftmostChild(MemoryTreeChunk* ptr)
	{
		if (ptr->child[0] != 0)
			return ptr->child[0];
		else
			return ptr->child[1];
	}	
	//--------------------------------------------------------------------------------------------------------------
	// Check if the given address is aligned
	static FORCEINLINE bool isAligned(size_t ptr_addr)
	{
		return ((ptr_addr & kAlignmentMask) == 0);
	}

	// Offset needed to align the given address
	static FORCEINLINE size_t alignmentOffset(size_t ptr_addr)
	{
		return ((ptr_addr & kAlignmentMask) == 0) ? 0 :
			((kDefaultAlignment - (ptr_addr & kAlignmentMask)) & kAlignmentMask);
	}
	//--------------------------------------------------------------------------------------------------------------
	// Bin Indexing
	static FORCEINLINE bool isSmall(size_t size)
	{
		return ((size >> kSmallBinShift) < kNumSmallBins);
	}

	static FORCEINLINE uint32 getSmallBinIndex(size_t size)
	{
		return	static_cast<uint32>(size >> kSmallBinShift);
	}

	static FORCEINLINE size_t getSmallIndexToSize(uint32 index)
	{
		return static_cast<size_t>(index << kSmallBinShift);
	}

	// Addressing into bins by index
	static FORCEINLINE MemoryChunk* smallBinAt(MemorySpace* msp, uint32 index)
	{
		return reinterpret_cast<MemoryChunk*>(&msp->small_bins[index << 1]);
	}

	static FORCEINLINE MemoryTreeChunk** treeBinAt(MemorySpace* msp, uint32 index)
	{
		return &(msp->tree_bins[index]);
	}

	// Compute tree index based on size
	static FORCEINLINE uint32 computeTreeIndex(size_t size)
	{
#if ODIN_COMPILER == ODIN_COMPILER_MSVC
		size_t index = size >> kTreeBinShift;
		if (index == 0)
			return 0;
		else if (index > 0xFFFF)
			return kNumTreeBins - 1;
		else
		{
			unsigned long bit_index;
			_BitScanReverse(&bit_index, index);
			return static_cast<uint32>((static_cast<uint32>(bit_index) << 1) + 
				((static_cast<uint32>(size) >> (static_cast<uint32>(bit_index)+
				(kTreeBinShift - 1)) & 1)));
		}
#endif
	}

	static FORCEINLINE uint32 leftshiftForTreeIndex(uint32 index)
	{
		if (index == kNumSmallBins - 1)
			return 0;
		else
			return ((index >> 1) + kTreeBinShift - 1);
	}

	static FORCEINLINE size_t minsizeForTreeIndex(uint32 index)
	{
		size_t x = ((1 << ((index >> 1) + kTreeBinShift)) |
			((static_cast<size_t>(index & 1)) << ((index >> 1) + kTreeBinShift - 1)));
		size_t y = x;
		return y;
	}
	//--------------------------------------------------------------------------------------------------------------
	// Conversion from chunk header to user pointer and back
	static FORCEINLINE void* chunkToMemory(MemoryChunk* ptr)
	{
#if ODIN_DEBUG == 1
		return reinterpret_cast<void*>(reinterpret_cast<uint8*>(ptr)+kChunkOverhead);
#else
		return reinterpret_cast<void*>(reinterpret_cast<uint8*>(ptr)+(sizeof(size_t) << 1));
#endif
	}
	static FORCEINLINE void* chunkToMemory(MemoryTreeChunk* ptr)
	{
#if ODIN_DEBUG == 1
		return reinterpret_cast<void*>(reinterpret_cast<uint8*>(ptr)+kChunkOverhead);
#else
		return reinterpret_cast<void*>(reinterpret_cast<uint8*>(ptr)+(sizeof(size_t) << 1));
#endif
	}

	static FORCEINLINE MemoryChunk* memoryToChunk(void* ptr)
	{
#if ODIN_DEBUG == 1
		return reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(ptr)-kChunkOverhead);
#else
		return reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(ptr)-(sizeof(size_t) << 1));
#endif
	}
	//--------------------------------------------------------------------------------------------------------------
	// Check if address of next chunk "next" is higher than base chunk "ptr"
	static FORCEINLINE bool okNext(MemoryChunk* ptr, MemoryChunk* next)
	{
		return (reinterpret_cast<uint8*>(ptr) < reinterpret_cast<uint8*>(next));
	}
	//--------------------------------------------------------------------------------------------------------------
	// Extraction of fields from head words
	static FORCEINLINE size_t getCInuse(MemoryChunk* ptr)
	{
		return ptr->head & kCinuseBit;
	}

	static FORCEINLINE size_t getPInuse(MemoryChunk* ptr)
	{
		return ptr->head & kPinuseBit;
	}

	// Check if chunk has inuse status
	static FORCEINLINE bool isInuse(MemoryChunk* ptr)
	{
		return ((ptr->head & kInuseBits) != kPinuseBit);
	}

	// Extract next field's PINUSE bit
	static FORCEINLINE size_t nextPInuse(MemoryChunk* ptr)
	{
		return (nextChunk(ptr)->head) & kPinuseBit;
	}
	//--------------------------------------------------------------------------------------------------------------
	// Functions to set size and chunks of flags

	// Set foot of inuse chunk to address of Memory Space
	static FORCEINLINE void markInuseFoot(MemorySpace* msp, MemoryChunk* ptr, size_t size)
	{
		reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(ptr)+size)->prev_foot = reinterpret_cast<size_t>(msp);
	}

	// Set foot of inuse chunk to NULL. This is done for chunks which are of size 
	// greater than the segment threshold
	static FORCEINLINE void markInuseFootNull(MemoryChunk* ptr, size_t size)
	{
		reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(ptr)+size)->prev_foot = 0;
	}

	// Set size as well as cinuse flags of this chunk and pinuse of next chunk
	static FORCEINLINE void setSizeInuse(MemorySpace* msp, MemoryChunk* ptr, size_t size)
	{
		ptr->head = (ptr->head & kPinuseBit) | size | kCinuseBit;
		reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(ptr)+size)->head |= kPinuseBit;

		markInuseFoot(msp, ptr, size);
	}

	// Set size as well as pinuse and cinuse flags of this chunk and pinuse of next chunk
	static FORCEINLINE void setSizeInusePinuse(MemorySpace* msp, MemoryChunk* ptr, size_t size)
	{
		ptr->head = (ptr->head & kPinuseBit) | size | kCinuseBit;
		reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(ptr)+size)->head |= kPinuseBit;

		markInuseFoot(msp, ptr, size);
	}

	// Set pinuse and cinuse flags (and size) of this chunk only
	static FORCEINLINE void setSizePinuseOfInuseChunk(MemorySpace* msp, MemoryChunk* ptr, size_t size)
	{
		ptr->head = size | kPinuseBit | kCinuseBit;

		markInuseFoot(msp, ptr, size);
	}

	// Set size and pinuse flag of this free chunk
	static FORCEINLINE void setSizePinuseOfFreeChunk(MemorySpace* msp, MemoryChunk* ptr, size_t size)
	{
		ptr->head = size | kPinuseBit;
		reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(ptr)+size)->prev_foot = size;
	}
	//--------------------------------------------------------------------------------------------------------------
	// Operations on bin maps
	static FORCEINLINE uint32 indexToBit(uint32 index) { return (1 << index); }

	// Mark/Clear bits in bitmap with given index
	static FORCEINLINE void markSmallMap(MemorySpace* msp, uint32 index) { msp->small_map |= indexToBit(index); }
	static FORCEINLINE void clearSmallMap(MemorySpace* msp, uint32 index) { msp->small_map &= ~indexToBit(index); }
	static FORCEINLINE bool isSmallMapMarked(MemorySpace* msp, uint32 index) { return (msp->small_map & indexToBit(index)) != 0; }

	static FORCEINLINE void markTreeMap(MemorySpace* msp, uint32 index) { msp->tree_map |= indexToBit(index); }
	static FORCEINLINE void clearTreeMap(MemorySpace* msp, uint32 index) { msp->tree_map &= ~indexToBit(index); }
	static FORCEINLINE bool isTreeMapMarked(MemorySpace* msp, uint32 index) { return (msp->tree_map & indexToBit(index)) != 0; }

	// Compute the index corresponding to a given bit
	static FORCEINLINE uint32 computeBitToIndex(uint32 mask)
	{
#if ODIN_COMPILER == ODIN_COMPILER_MSVC
		unsigned long index;
		_BitScanForward(&index, mask);
		return static_cast<uint32>(index);
#endif
	}
	//--------------------------------------------------------------------------------------------------------------
	// Helper functions to validate chunks

	// Check properties of top chunk
	static void checkTopChunk(MemorySpace* msp)
	{
		MemoryChunk* top = msp->top;
		size_t sz = top->head & ~kInuseBits;	// Third lower bit can be set
		ASSERT_ERROR(isAligned(reinterpret_cast<size_t>(chunkToMemory(top))),
						"Top chunk is not aligned");
		ASSERT_ERROR(sz == msp->top_size, "Mismatch in top chunk size information");
		ASSERT_ERROR(sz > 0, "Top chunk size is zero");
		ASSERT_ERROR(getPInuse(top), "PINUSE bit of top chunk is not set");
	}

	// Check properties of inuse chunk
	static void checkInuseChunk(MemorySpace* msp, MemoryChunk* ptr)
	{
		ASSERT_ERROR(isAligned(reinterpret_cast<size_t>(chunkToMemory(ptr))),
			"Chunk is not aligned");
		ASSERT_ERROR(isInuse(ptr), "CINUSE bit is not set for this chunk");
		ASSERT_ERROR(nextPInuse(ptr), "PINUSE bit of next chunk is not set");
		ASSERT_ERROR(getPInuse(ptr) || (nextChunk(previousChunk(ptr)) == ptr), "Previous chunk offset is not correct");
	}

	// Check properties of free chunk
	static void checkFreeChunk(MemorySpace* msp, MemoryChunk* ptr)
	{
		size_t size = chunkSize(ptr);
		MemoryChunk* next_ptr = chunkPlusOffset(ptr, size);
		ASSERT_ERROR(isAligned(reinterpret_cast<size_t>(ptr)), "Free chunk is not aligned");
		ASSERT_ERROR(!isInuse(ptr), "CINUSE bit is set for a free chunk");
		ASSERT_ERROR(!nextPInuse(ptr), "PINUSE bit is set for next chunk");
		if (ptr != msp->dv && ptr != msp->top)
		{
			if (size >= kMinChunkSize)
			{
				ASSERT_ERROR(((size & kAlignmentMask) == 0), "Size of free chunk is not aligned");
				ASSERT_ERROR(isAligned(reinterpret_cast<size_t>(chunkToMemory(ptr))), "Chunk to memory address is not aligned");
				ASSERT_ERROR((next_ptr->prev_foot == size), "Value stored in prev_foot field of next chunk is not equal to this free chunk's size");
				ASSERT_ERROR(getPInuse(ptr), "PINUSE bit is not set for this free chunk");
				ASSERT_ERROR(((next_ptr == msp->top) || isInuse(next_ptr)),
					"Next chunk is not top or next chunk's CINUSE bit is not set");
				ASSERT_ERROR((ptr->fd->bk == ptr), "Fd/bk pointer error");
				ASSERT_ERROR((ptr->bk->fd == ptr), "Fd/bk pointer error");
			}
			else
				ASSERT_ERROR((size == sizeof(size_t), "Marker is not equal to sizeof(size_t)"));
		}
	}

	// Check properties of alloced chunk at the point they are alloced
	static void checkAllocedChunk(MemorySpace* msp, void* mem, size_t size)
	{
		if (mem != 0)
		{
			MemoryChunk* ptr = memoryToChunk(mem);
			size_t sz = ptr->head & ~(kInuseBits);
			ASSERT_ERROR(isAligned(reinterpret_cast<size_t>(mem)), "Memory is not aligned");
			ASSERT_ERROR(((sz & kAlignmentMask) == 0), "Size is not aligned");
			ASSERT_ERROR((sz >= kMinChunkSize), "Size of alloced chunk is less than minimum chunk size");
			ASSERT_ERROR((sz >= size), "Size in chunk's head field is less than the actual chunk size");
			// Size is less than kMinChunkSize more than request
			ASSERT_ERROR(sz < (size + kMinChunkSize), "Size of alloced chunk is greater than minimum chunk size more than request");
		}
	}

	// Check a tree and its subtrees
	static void checkTree(MemorySpace* msp, MemoryTreeChunk* tptr)
	{
		MemoryTreeChunk* head = 0;
		MemoryTreeChunk* curr_tptr = tptr;
		uint32 tindex = tptr->index;
		size_t tsize = chunkSize(tptr);
		uint32 idx = computeTreeIndex(tsize);
		ASSERT_ERROR(tindex == idx, "Index mismatch of tree chunk");
		ASSERT_ERROR(tsize >= kMinLargeSize, "Chunk size less than minimum required tree chunk size");
		ASSERT_ERROR(tsize >= minsizeForTreeIndex(idx), "Chunk size less than minimum chunk size for tree bin index %d", idx);
		ASSERT_ERROR((idx == kNumTreeBins - 1) || (tsize < minsizeForTreeIndex(idx + 1)), 
						"Chunk size greater than maximum chunk size for tree bin index %d", idx);

		do
		{
			// Traverse through chain of same sized nodes
			ASSERT_ERROR(isAligned(reinterpret_cast<size_t>(chunkToMemory(tptr))), 
								"Free chunk in tree bin index %d is not aligned", idx);
			ASSERT_ERROR(curr_tptr->index == tindex, "Chunk in tree bin index %d has incorrect index", idx);
			ASSERT_ERROR(chunkSize(curr_tptr) == tsize, "Size mismatch in tree bin index %d", idx);
			ASSERT_ERROR(!isInuse(reinterpret_cast<MemoryChunk*>(curr_tptr)), "Chunk in tree bin index %d marked as in use", idx);
			ASSERT_ERROR(!nextPInuse(reinterpret_cast<MemoryChunk*>(curr_tptr)),
				"Chunk adjacent to chunk in tree bin index %d has PINUSE bit set", idx);
			ASSERT_ERROR(curr_tptr->fd->bk == curr_tptr, "Fd/bk pointer error in tree bin index %d", idx);
			ASSERT_ERROR(curr_tptr->bk->fd == curr_tptr, "Fd/bk pointer error in tree bin index %d", idx);
			if (curr_tptr->parent == 0)
			{
				ASSERT_ERROR(curr_tptr->child[0] == 0, "Chunk in tree bin index %d with no parent has a child", idx);
				ASSERT_ERROR(curr_tptr->child[1] == 0, "Chunk in tree bin index %d with no parent has a child", idx);
			}
			else
			{
				ASSERT_ERROR(head == 0, "Head pointer in tree bin index %d is not NULL", idx);
				head = curr_tptr;
				ASSERT_ERROR(curr_tptr->parent != curr_tptr, "Chunk in tree bin index %d is a parent of itselt", idx);
				ASSERT_ERROR(curr_tptr->parent->child[0] == curr_tptr ||
					curr_tptr->parent->child[1] == curr_tptr ||
					*(reinterpret_cast<MemoryTreeChunk**>(curr_tptr->parent)) == curr_tptr,
					"Parent / child pointer error in tree bin index %d", idx);
				if (curr_tptr->child[0] != 0)
				{
					ASSERT_ERROR(curr_tptr->child[0]->parent == curr_tptr,
						"Chunk's child in tree bin index %d does not refer the chunk as its parent", idx);
					ASSERT_ERROR(curr_tptr->child[0] != curr_tptr, "Chunk in tree bin index %d is a child of itself", idx);
					checkTree(msp, curr_tptr->child[0]);
				}
				if (curr_tptr->child[1] != 0)
				{
					ASSERT_ERROR(curr_tptr->child[1]->parent == curr_tptr,
						"Chunk's child in tree bin index %d does not refer the chunk as its parent", idx);
					ASSERT_ERROR(curr_tptr->child[1] != curr_tptr, "Chunk in tree bin index %d is a child of itself", idx);
					checkTree(msp, curr_tptr->child[1]);
				}
				if (curr_tptr->child[0] != 0 && curr_tptr->child[1] != 0)
				{
					ASSERT_ERROR(chunkSize(curr_tptr->child[0]) < chunkSize(curr_tptr->child[1]),
						"Size of chunk's left child in tree bin %d is greater than the size of right child", idx);
				}
			}
			curr_tptr = curr_tptr->fd;
		} while (curr_tptr != tptr);
		ASSERT_ERROR(head != 0, "Head pointer in tree bin index %d is NULL", idx);
	}

	// Check all chunks in a treebin
	static void checkTreebin(MemorySpace* msp, uint32 index)
	{
		MemoryTreeChunk* tptr = *treeBinAt(msp, index);
		bool empty = (msp->tree_map & (1U << index)) == 0;
		if (tptr == 0)
			ASSERT_ERROR(empty, "Treebin at index %d is empty", index);
		if (!empty)
			checkTree(msp, tptr);
	}

	// Check all chunks in a smallbin
	static void checkSmallbin(MemorySpace* msp, uint32 index)
	{
		MemoryChunk* bin_ptr = smallBinAt(msp, index);
		MemoryChunk* ptr = bin_ptr->bk;
		bool empty = (msp->small_map & (1U << index)) == 0;
		if (ptr == bin_ptr)
			ASSERT_ERROR(empty, "Smallbin at index %d is empty", index);
		if (!empty)
		{
			for (; ptr != bin_ptr; ptr = ptr->bk)
			{
				size_t size = chunkSize(ptr);
				// Each chunk should be free
				checkFreeChunk(msp, ptr);
				// Check if chunk belongs in bin
				ASSERT_ERROR(getSmallBinIndex(size) == index, "Chunk in small bin in index %d not placed in correct bin", index);
				ASSERT_ERROR((ptr->bk == bin_ptr) || (chunkSize(ptr->bk) == chunkSize(ptr)), 
							"Chunk sizes in the same bin index %d do not match", index);
				// Next chunk should be in use
				MemoryChunk* next_ptr = nextChunk(ptr);
			}
		}
	}

	// Find a particular chunk in a bin
	static bool findInBin(MemorySpace* msp, MemoryChunk* ptr)
	{
		size_t size = chunkSize(ptr);
		if (isSmall(size))
		{
			uint32 small_index = getSmallBinIndex(size);
			MemoryChunk* bptr = smallBinAt(msp, small_index);
			if (isSmallMapMarked(msp, small_index))
			{
				MemoryChunk* curr_ptr = bptr;
				do
				{
					if (curr_ptr == ptr)
						return true;
				} while ((curr_ptr = curr_ptr->fd) != bptr);
			}
		}
		else
		{
			uint32 tindex = computeTreeIndex(size);
			if (isTreeMapMarked(msp, tindex))
			{
				MemoryTreeChunk* tptr = *treeBinAt(msp, tindex);
				size_t sizebits = size << leftshiftForTreeIndex(tindex);
				while (tptr != 0 && chunkSize(tptr) != size)
				{
					tptr = tptr->child[(sizebits >> (kSizeTBitSize - 1)) & 1];
					sizebits <<= 1;
				}
				if (tptr != 0)
				{
					MemoryTreeChunk* curr_tptr = tptr;
					do
					{
						if (curr_tptr == reinterpret_cast<MemoryTreeChunk*>(ptr))
							return true;
					} while ((curr_tptr = curr_tptr->fd) != tptr);
				}
			}
		}
		return false;
	}

	// Traverse each chunk and check it, return total
	static size_t traverseAndCheck(MemorySpace* msp)
	{
		size_t size_sum = 0;
		size_sum += msp->top_size;
		MemoryChunk* curr_ptr = memoryToChunk(reinterpret_cast<void*>(msp));	// Get the chunk for MemorySpace struct
		curr_ptr = nextChunk(curr_ptr);											// Get the next chunk (first allocatable chunk)
		MemoryChunk* last_ptr = nullptr;
		ASSERT_ERROR(getPInuse(curr_ptr), "The first chunk in the segment does not have its PINUSE bit set");
		while (reinterpret_cast<uint8*>(curr_ptr) >= msp->least_addr && 
			reinterpret_cast<uint8*>(curr_ptr) <= msp->least_addr + msp->footprint && 
			curr_ptr != msp->top)
		{
			size_sum += chunkSize(curr_ptr);
			if(isInuse(curr_ptr))
			{
				ASSERT_ERROR(!findInBin(msp, curr_ptr), "In use chunk present in free bin");
				checkInuseChunk(msp, curr_ptr);
			}
			else
			{
				ASSERT_ERROR((curr_ptr == msp->dv) || (findInBin(msp, curr_ptr)), "Free chunk is neither DV nor is present in free bin");
				ASSERT_ERROR((last_ptr == 0) || isInuse(last_ptr), "Two consecutive free chunks present");
				checkFreeChunk(msp, curr_ptr);
			}
			last_ptr = curr_ptr;
			curr_ptr = nextChunk(curr_ptr);
		}
		return size_sum;
	}
	//--------------------------------------------------------------------------------------------------------------
	// Linking and unlinking chunks (small and large)

	// Insert a free chunk into a small bin
	static void insertSmallChunk(MemorySpace* msp, MemoryChunk* ptr, size_t size)
	{
		ASSERT_ERROR(size >= kMinChunkSize, "Size of small chunk is less than minimum chunk size");
		uint32 index = getSmallBinIndex(size);
		MemoryChunk* back = smallBinAt(msp, index);

		if (!isSmallMapMarked(msp, index))
			markSmallMap(msp, index);

		MemoryChunk* forward = back->fd;
		back->fd = ptr;
		forward->bk = ptr;
		ptr->fd = forward;
		ptr->bk = back;
	}

	// Unlink a free chunk from a small bin
	static void unlinkSmallChunk(MemorySpace* msp, MemoryChunk* ptr, size_t size)
	{
		MemoryChunk* forward = ptr->fd;
		MemoryChunk* back = ptr->bk;

		uint32 index = getSmallBinIndex(size);
		ASSERT_ERROR(ptr != forward, "Forward pointer of chunk to be unlinked is itself");
		ASSERT_ERROR(ptr != back, "Back pointer of chunk to be unlinked is itself");
		ASSERT_ERROR(chunkSize(ptr) == getSmallIndexToSize(index), "Chunk size is not equal to the expected small bin size");

		if (forward == back)
			clearSmallMap(msp, index);
		forward->bk = back;
		back->fd = forward;
	}

	// Unlink the first free chunk from a small bin
	static void unlinkFirstSmallChunk(MemorySpace* msp, MemoryChunk* ptr, uint32 index)
	{
		MemoryChunk* forward = ptr->fd;
		MemoryChunk* back = smallBinAt(msp, index);

		ASSERT_ERROR(ptr != forward, "Forward pointer of chunk to be unlinked is itself");
		ASSERT_ERROR(ptr != back, "Back pointer of chunk to be unlinked is itself");
		ASSERT_ERROR(chunkSize(ptr) == getSmallIndexToSize(index), "Chunk size is not equal to the expected small bin size");

		if (forward == back)
			clearSmallMap(msp, index);
		forward->bk = back;
		back->fd = forward;
	}

	// Insert a free chunk into a tree
	static void insertLargeChunk(MemorySpace* msp, MemoryTreeChunk* ptr, size_t size)
	{
		uint32 index = computeTreeIndex(size);
		MemoryTreeChunk** ptr_to_bin = treeBinAt(msp, index);
		ptr->index = index;
		ptr->child[0] = ptr->child[1] = 0;
		if (!isTreeMapMarked(msp, index))
		{
			// This is the first node for this index
			markTreeMap(msp, index);
			*ptr_to_bin = ptr;
			ptr->parent = reinterpret_cast<MemoryTreeChunk*>(ptr_to_bin);
			// The fd and bk pointers point to the node itself. This shows that the node is not
			// a chained node
			ptr->bk = ptr->fd = ptr;
		}
		else
		{
			MemoryTreeChunk* temp = *ptr_to_bin;
			size_t size_bits = size << leftshiftForTreeIndex(index);
			while (true)
			{
				if (chunkSize(reinterpret_cast<MemoryChunk*>(temp)) != size)
				{
					// A similar sized node does not exist.

					// Shift the sign bit of shifted_val to the other end to check which
					// child to select
					MemoryTreeChunk** curr_pptr = &(temp->child[(size_bits >> (kSizeTBitSize - 1)) & 1]);
					// The bit next to the sign bit replaces the sign bit for the next
					// iteration
					size_bits <<= 1;
					if (*curr_pptr != 0)
						// Make the child the current node
						temp = *curr_pptr;
					else
					{
						// Child doesnýt exist. Insert the passed node here
						*curr_pptr = ptr;
						ptr->parent = temp;
						ptr->fd = ptr->bk = ptr;
						break;
					}
				}
				else
				{
					// Insert the node into a chain of similar sized nodes
					MemoryTreeChunk* front = temp->fd;
					temp->fd = front->bk = ptr;
					ptr->fd = front;
					ptr->bk = temp;
					ptr->parent = 0;
					break;
				}
			}
		}
	}

	// Unlink a free chunk from a tree
	static void unlinkLargeChunk(MemorySpace* msp, MemoryTreeChunk* ptr)
	{
		MemoryTreeChunk* ptr_parent = ptr->parent;
		MemoryTreeChunk* replacement_node = 0;
		if (ptr->bk != ptr)
		{
			// This node is part of a chain of similar sized nodes
			MemoryTreeChunk* front = ptr->fd;
			replacement_node = ptr->bk;
			front->bk = replacement_node;
			replacement_node->fd = front;
		}
		else
		{
			MemoryTreeChunk** replacement_node_pptr = 0;
			// If children exist
			if ((replacement_node = *(replacement_node_pptr = &(ptr->child[1]))) ||
				(replacement_node = *(replacement_node_pptr = &(ptr->child[0]))))
			{
				// Get the rightmost descendant
				MemoryTreeChunk** curr_pptr = 0;
				while ((*(curr_pptr = &(replacement_node->child[1])) != 0) ||
					(*(curr_pptr = &(replacement_node->child[0])) != 0))
				{
					replacement_node_pptr = curr_pptr;
					replacement_node = *curr_pptr;
				}
				// Remove the rightmost descendant from the tree
				*replacement_node_pptr = 0;
			}
		}
		if (ptr_parent != 0)
		{
			MemoryTreeChunk** root_node = treeBinAt(msp, ptr->index);
			if (ptr == *root_node)
			{
				// ptr is the root node. Replace it with the rightmost descendant.
				*root_node = replacement_node;
				if (*root_node == 0)
					// ptr was the only node for this index
					clearTreeMap(msp, ptr->index);
			}
			else
			{
				// Replace ptr with replacement_node as one of the children of ptrýs parent
				if (ptr_parent->child[0] == ptr)
					ptr_parent->child[0] = replacement_node;
				else
					ptr_parent->child[1] = replacement_node;
			}
			if (replacement_node != 0)
			{
				replacement_node->parent = ptr_parent;
				if (ptr->child[0] != 0)
				{
					replacement_node->child[0] = ptr->child[0];
					replacement_node->child[0]->parent = replacement_node;
				}
				if (ptr->child[1] != 0)
				{
					replacement_node->child[1] = ptr->child[1];
					replacement_node->child[1]->parent = replacement_node;
				}
			}
		}
	}

	// Relay to small/large chunk insertion
	static void insertChunk(MemorySpace* msp, MemoryChunk* ptr, size_t size)
	{
		if (isSmall(size))
			insertSmallChunk(msp, ptr, size);
		else
			insertLargeChunk(msp, reinterpret_cast<MemoryTreeChunk*>(ptr), size);
	}

	// Relay to small/large chunk unlinking
	static void unlinkChunk(MemorySpace* msp, MemoryChunk* ptr, size_t size)
	{
		if (isSmall(size))
			unlinkSmallChunk(msp, ptr, size);
		else
			unlinkLargeChunk(msp, reinterpret_cast<MemoryTreeChunk*>(ptr));
	}

	// Replace dv node and insert the old node into a small bin.
	static void replaceDV(MemorySpace* msp, MemoryChunk* ptr, size_t size)
	{
		size_t dv_size = msp->dv_size;
		if (dv_size != 0)
		{
			MemoryChunk* dv = msp->dv;
			ASSERT_ERROR(isSmall(dv_size), "Size of DV is greater than 256 bytes");
			insertSmallChunk(msp, dv, dv_size);
		}
		msp->dv_size = size;
		msp->dv = ptr;
	}
	//--------------------------------------------------------------------------------------------------------------
	// Helper functions for alloc
	static void* treeAllocLarge(MemorySpace* msp, size_t nb)
	{
		MemoryTreeChunk* curr_ptr = 0;
		MemoryTreeChunk* prev_ptr = 0;

		size_t rem_size = -nb;	// Unsigned negation. rem_size = 2 ^ (num of bits in size_t) - nb
		uint32 index = computeTreeIndex(nb);

		if ((curr_ptr = *treeBinAt(msp, index)) != 0)
		{
			// Traverse the tree for this bin looking for node with size equal to nb
			size_t sizebits = nb << leftshiftForTreeIndex(index);
			MemoryTreeChunk* right_subtree = 0;		// The deepest untaken right subtree
			while (true)
			{
				MemoryTreeChunk* right_tree = 0;
				size_t rem = chunkSize(curr_ptr) - nb;
				if (rem < rem_size)
				{
					prev_ptr = curr_ptr;
					rem_size = rem;
					if (rem_size == 0)
						break;
				}
				right_tree = curr_ptr->child[1];
				curr_ptr = curr_ptr->child[(sizebits >> (kSizeTBitSize - 1)) & 1];
				if ((right_tree != 0) & (right_tree != curr_ptr))
					right_subtree = right_tree;
				if (curr_ptr == 0)
				{
					curr_ptr = right_subtree;	// Set curr_ptr to least subtree having size > nb
					break;
				}
				sizebits <<= 1;
			}
		}

		if (curr_ptr == 0 && prev_ptr == 0)
		{
			uint32 leftbits = indexToBit(index);
			// Create a mask with all bits to left of least set bit on
			leftbits = (leftbits << 1) | -(leftbits << 1);
			// Mask it with the tree map
			leftbits = leftbits & msp->tree_map;
			if (leftbits != 0)
			{
				uint32 leastbit = leftbits & -(leftbits);
				uint32 ind = computeBitToIndex(leastbit);
				curr_ptr = *treeBinAt(msp, ind);
			}
		}

		// Find the smallest tree or subtree
		while (curr_ptr != 0)
		{
			size_t rem = chunkSize(curr_ptr) - nb;
			if (rem < rem_size)
			{
				rem_size = rem;
				prev_ptr = curr_ptr;
			}
			curr_ptr = leftmostChild(curr_ptr);
		}

		// Check if dv is a better fit. If no, go ahead. Else, return nullptr so that malloc can use dv
		if (prev_ptr != 0 && rem_size < (msp->dv_size - nb))
		{
			unlinkLargeChunk(msp, prev_ptr);
			MemoryChunk* ptr = reinterpret_cast<MemoryChunk*>(prev_ptr);
			MemoryChunk* rem_ptr = chunkPlusOffset(ptr, nb);
			ASSERT_ERROR(chunkSize(ptr) == rem_size + nb, "Remainder size and requested size don't add up to the original chunk size");

			if (rem_size < kMinChunkSize)
				setSizeInusePinuse(msp, ptr, (rem_size + nb));
			else
			{
				setSizePinuseOfInuseChunk(msp, ptr, nb);
				setSizePinuseOfFreeChunk(msp, rem_ptr, rem_size);
				insertChunk(msp, rem_ptr, rem_size);
			}
			return chunkToMemory(ptr);
		}
		return nullptr;
	}

	static void* treeAllocSmall(MemorySpace* msp, size_t nb)
	{
		MemoryTreeChunk* ptr = 0;
		MemoryTreeChunk* curr_ptr = 0;
		size_t rem_size;
		uint32 least_bit = msp->tree_map & (-msp->tree_map);
		uint32 index = computeBitToIndex(least_bit);
		ptr = curr_ptr = *treeBinAt(msp, index);
		rem_size = chunkSize(ptr) - nb;

		while ((ptr = leftmostChild(ptr)) != 0)
		{
			size_t rem = chunkSize(ptr) - nb;
			if (rem < rem_size)
			{
				rem_size = rem;
				curr_ptr = ptr;
			}
		}
		MemoryChunk* rem_ptr = chunkPlusOffset(reinterpret_cast<MemoryChunk*>(curr_ptr), nb);
		ASSERT_ERROR(chunkSize(curr_ptr) == rem_size + nb, "Remainder size and requested size don't add up to the original chunk size");
		unlinkLargeChunk(msp, curr_ptr);
		if (rem_size < kMinChunkSize)
		{
			setSizeInusePinuse(msp, reinterpret_cast<MemoryChunk*>(curr_ptr), rem_size + nb);
		}
		else
		{
			setSizePinuseOfInuseChunk(msp, reinterpret_cast<MemoryChunk*>(curr_ptr), nb);
			setSizePinuseOfFreeChunk(msp, reinterpret_cast<MemoryChunk*>(curr_ptr), rem_size);
			replaceDV(msp, rem_ptr, rem_size);
		}
		return chunkToMemory(reinterpret_cast<MemoryChunk*>(curr_ptr));
	}
	//-----------------------------------------------------------------------------------------------------------------
	void* alloc(MemorySpace* msp, size_t bytes)
	{
		// Acquire lock
		std::lock_guard<std::mutex> guard(msp->memory_lock);

		void* mem = 0;
		size_t nb;

		if (bytes <= kMaxRequest)
		{
			if (bytes < kMinRequest)
				nb = kMinChunkSize;
			else
				nb = padRequest(bytes);
			uint32 index = getSmallBinIndex(nb);
			uint32 smallbits = msp->small_map >> index;
			// Remainderless fit to a small bin
			if ((smallbits & 0x3U) != 0)
			{
				// Use next bin if index is empty
				index += ~smallbits & 1;
				MemoryChunk* back = smallBinAt(msp, index);
				MemoryChunk* ptr = back->fd;
				ASSERT_ERROR(chunkSize(ptr) == getSmallIndexToSize(index), "Chunk size does not equal to small_index_to_size");
				unlinkFirstSmallChunk(msp, ptr, index);
				setSizeInusePinuse(msp, ptr, getSmallIndexToSize(index));
				mem = chunkToMemory(ptr);
				checkAllocedChunk(msp, mem, nb);
				return mem;
			}
			else if (nb > msp->dv_size)
			{
				// Use chunk in next non-empty small bin
				if (smallbits != 0)
				{
					MemoryChunk* rem_ptr = 0;
					uint32 bits = indexToBit(index);
					// Create mask with all bits to left of least bit set
					uint32 leftbits = (smallbits << index) & ((bits << 1) | -(bits << 1));
					// Isolate the least set bit
					uint32 leastbit = leftbits & -(leftbits);
					uint32 ind = computeBitToIndex(leastbit);
					MemoryChunk* back = smallBinAt(msp, ind);
					MemoryChunk* ptr = back->fd;
					ASSERT_ERROR(chunkSize(ptr) == getSmallIndexToSize(ind), "Chunk size does not equal to small_index_to_size");
					unlinkFirstSmallChunk(msp, ptr, ind);
					size_t rem_size = getSmallIndexToSize(ind) - nb;
					// Fit here cannot be remainderless if sizeof(size_t) is 4 bytes
					if (sizeof(size_t) != 4 && rem_size < kMinChunkSize)
						setSizeInusePinuse(msp, ptr, getSmallIndexToSize(ind));
					else
					{
						setSizePinuseOfInuseChunk(msp, ptr, nb);
						rem_ptr = chunkPlusOffset(ptr, nb);
						setSizePinuseOfFreeChunk(msp, ptr, rem_size);
						replaceDV(msp, rem_ptr, rem_size);
					}
					mem = chunkToMemory(ptr);
					checkAllocedChunk(msp, mem, nb);
					return mem;
				}
			}
			else if (msp->tree_map != 0 && (mem = treeAllocSmall(msp, nb)) != 0)
			{
				checkAllocedChunk(msp, mem, nb);
				return mem;
			}
		}
		else if (bytes >= kMaxRequest)
		{
			// Return NULL
			return nullptr;
		}
		else
		{
			nb = padRequest(bytes);
			if (msp->tree_map != 0 && (mem = treeAllocLarge(msp, nb)) != 0)
			{
				checkAllocedChunk(msp, mem, nb);
				return mem;
			}
		}

		if (nb <= msp->dv_size)
		{
			size_t rem_size = msp->dv_size - nb;
			MemoryChunk* ptr = msp->dv;
			if (rem_size >= kMinChunkSize)
			{
				// Split dv
				MemoryChunk* rem_ptr = msp->dv = chunkPlusOffset(ptr, nb);;
				msp->dv_size = rem_size;
				setSizePinuseOfFreeChunk(msp, rem_ptr, rem_size);
				setSizePinuseOfInuseChunk(msp, ptr, nb);
			}
			else
			{
				// Exhaust dv
				size_t dv_size = msp->dv_size;
				msp->dv_size = 0;
				msp->dv = 0;
				setSizeInusePinuse(msp, ptr, nb);
			}
			mem = chunkToMemory(ptr);
			checkAllocedChunk(msp, mem, nb);
			return mem;
		}
		else if (nb < msp->top_size)
		{
			// Split top
			size_t rem_size = msp->top_size -= nb;
			MemoryChunk* ptr = msp->top;
			MemoryChunk* rem_ptr = msp->top = chunkPlusOffset(ptr, nb);
			// Check if top has moved over to the next non committed page
			//if (msp->commit_page_func != 0)
			//{
				if (reinterpret_cast<uint8*>(msp->top) >(msp->least_addr + (msp->curr_page_index * msp->page_size)))
				{
					// Commit the next page in this segment
					void* next_page_ptr = reinterpret_cast<void*>(msp->least_addr + (msp->curr_page_index * msp->page_size));
					//size_t bytes_committed = msp->commit_page_func(next_page_ptr, msp->page_size);
					//ASSERT_ERROR(bytes_committed == msp->page_size, "Number of bytes committed is not equal to the page size");
					//msp->commit_page_func(next_page_ptr, msp->page_size);
					SysAlloc::commitPage(next_page_ptr, msp->page_size);
					// Increment the current page index
					++msp->curr_page_index;
				}
			//}
			rem_ptr->head = rem_size | kPinuseBit;
			setSizePinuseOfInuseChunk(msp, ptr, nb);
			mem = chunkToMemory(ptr);
			checkAllocedChunk(msp, mem, nb);
			return mem;
		}

		// Still no luck! Request additional memory from the system
		if (bytes < msp->segment_threshold)
		{
			// Allocate from system (request for memory from system will have a granularity of page size)
			size_t page_aligned_nb = (nb + (msp->page_size - 1)) & ~(msp->page_size - 1);
			void* adjacentSeg = reinterpret_cast<void*>(msp->least_addr + msp->footprint);
			// Reserve address space starting from the end of this segment
			adjacentSeg = SysAlloc::reserveSegment(msp->page_size, adjacentSeg);
			if (adjacentSeg != nullptr) // If address space is reserved
			{
				// Set the new segment size
				msp->footprint += page_aligned_nb;
				if (msp->footprint > msp->max_footprint)
					msp->max_footprint = msp->footprint;
				// Commit this page
				//if (msp->commit_page_func != 0)
				//{
					SysAlloc::commitPage(adjacentSeg, page_aligned_nb);
					++msp->curr_page_index;
				//}
				msp->top_size += page_aligned_nb;
				// Now split the top
				size_t rem_size = msp->top_size -= nb;
				MemoryChunk* ptr = msp->top;
				MemoryChunk* rem_ptr = msp->top = chunkPlusOffset(ptr, nb);
				rem_ptr->head = rem_size | kPinuseBit;
				setSizePinuseOfInuseChunk(msp, ptr, nb);
				mem = chunkToMemory(ptr);
				checkAllocedChunk(msp, mem, nb);
				return mem;
			}
		}

		// Allocate the requested space from system directly
		nb = padRequest(bytes + kChunkOverhead);	// The additional kChunkOverhead is for the imaginary trailing chunk after this chunk
		//MemoryChunk* ptr = reinterpret_cast<MemoryChunk*>(msp->reserve_segment_func(nb, NULL));
		MemoryChunk* ptr = reinterpret_cast<MemoryChunk*>(SysAlloc::reserveSegment(nb, NULL));
		setSizePinuseOfInuseChunk(msp, ptr, bytes);
		markInuseFootNull(ptr, bytes);
		mem = chunkToMemory(ptr);
		checkAllocedChunk(msp, mem, nb);
		return mem;
	}

	bool free(MemorySpace* msp, void* mem)
	{
		// Acquire lock
		std::lock_guard<std::mutex> guard(msp->memory_lock);

		if (mem != 0)
		{
			MemoryChunk* ptr = memoryToChunk(mem);
			checkInuseChunk(msp, ptr);
			if (isInuse(ptr))
			{
				// First check if the size of this allocation is greater than segment threshold or 
				// if the allocation's address does not fall within the segment
				if ((chunkSize(ptr) > msp->segment_threshold) ||
					(reinterpret_cast<uint8*>(ptr) < msp->least_addr || 
					(reinterpret_cast<uint8*>(ptr) > msp->least_addr + msp->footprint)))
				{
					// If yes, return the memory to the system directly
					size_t size = chunkSize(ptr) + kChunkOverhead;
					//msp->release_segment_func(ptr, size);
					SysAlloc::releaseSegment(ptr, size);
					return false;
				}

				size_t ptr_size = chunkSize(ptr);
				MemoryChunk* next_ptr = chunkPlusOffset(ptr, ptr_size);
				if (!getPInuse(ptr))
				{
					size_t prev_foot = ptr->prev_foot;
					MemoryChunk* prev_ptr = chunkMinusOffset(ptr, prev_foot);
					ptr_size += prev_foot;
					ptr = prev_ptr;
					// Consolidate backwards 
					if (ptr != msp->dv)
						unlinkChunk(msp, ptr, prev_foot);
					else if ((next_ptr->head & kInuseBits) == kInuseBits)
					{
						msp->dv_size = ptr_size;
						// Clear the kPinuse bit of the next chunk
						next_ptr->head &= ~kPinuseBit;
						setSizePinuseOfFreeChunk(msp, ptr, ptr_size);
						return false;
					}
				}
				if (getPInuse(next_ptr))
				{
					if (!getCInuse(next_ptr))
					{
						// Consolidate forward
						if (next_ptr == msp->top)
						{
							size_t top_size = msp->top_size += ptr_size;
							msp->top = ptr;
							ptr->head = top_size | kPinuseBit;
							if (ptr == msp->dv)
							{
								msp->dv = 0;
								msp->dv_size = 0;
							}
							// Decommit the current page if top has moved to the previous page
							//if (msp->decommit_page_func != 0)
							//{
								if (reinterpret_cast<uint8*>(msp->top) <
									(msp->least_addr + ((msp->curr_page_index - 1) * msp->page_size)))
								{
									void* curr_page_ptr = reinterpret_cast<void*>(msp->least_addr +
										(msp->curr_page_index - 1) * msp->page_size);
									//msp->decommit_page_func(curr_page_ptr, msp->page_size);
									SysAlloc::decommitPage(curr_page_ptr, msp->page_size);
									--msp->curr_page_index;
								}
							//}

							// Get the chunk right after MemorySpace struct, might be useful later
							MemoryChunk* next_chunk = nextChunk(memoryToChunk(msp));
							size_t offset = alignmentOffset(reinterpret_cast<size_t>(chunkToMemory(next_chunk)));
							next_chunk = reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(next_chunk)+offset);
							// Now check if the size of top is a multiple of segment_granularity.
							// If yes, free that portion.
							if (msp->top_size > msp->segment_granularity)
							{
								size_t size = msp->top_size + (reinterpret_cast<uint8*>(msp->top) - msp->least_addr);
								size_t size_factor = msp->top_size / msp->segment_granularity;
								for (size_t i = size_factor; i > 1; --i)
								{
									uint8* seg = msp->least_addr + (msp->segment_granularity * (i - 1));
									//msp->release_segment_func(reinterpret_cast<void*>(seg), msp->segment_granularity);
									SysAlloc::releaseSegment(reinterpret_cast<void*>(seg), msp->segment_granularity);
								}
							}
							if (next_chunk == msp->top)
							{
								// This segment needs to be destroyed, so return true
								return true;
							}
							return false;
						}
						else if (next_ptr == msp->dv)
						{
							size_t dv_size = msp->dv_size += ptr_size;
							msp->dv = ptr;
							setSizePinuseOfFreeChunk(msp, ptr, dv_size);
							return false;
						}
						else
						{
							size_t next_size = chunkSize(next_ptr);
							ptr_size += next_size;
							unlinkChunk(msp, next_ptr, next_size);
							setSizePinuseOfFreeChunk(msp, ptr, ptr_size);
							if (ptr == msp->dv)
							{
								msp->dv_size = ptr_size;
								return false;
							}
						}
					}
					else
					{
						next_ptr->head &= ~kPinuseBit;
						setSizePinuseOfFreeChunk(msp, ptr, ptr_size);
					}
					insertChunk(msp, ptr, ptr_size);
					checkFreeChunk(msp, ptr);
					return false;
				}
				else
				{
					// TODO: Add error message
					return false;
				}
			}
			else
				return false;
		}
		else
			return false;
	}
	

	void* allocAligned(MemorySpace* msp, size_t alignment, size_t bytes, size_t offset)
	{
		if (alignment <= kDefaultAlignment)
			// Just call alloc in this case
			return alloc(msp, bytes);
		if (alignment < kMinChunkSize)
			// Must be at least equal to minimum chunk size
			alignment = kMinChunkSize;
		if ((alignment & (alignment - 1)) != 0)
		{
			// Ensure the alignment is a power of 2
			size_t align = kDefaultAlignment << 1;
			while (align < alignment)
				align <<= 1;
			alignment = align;
		}

		if (bytes >= kMaxRequest - alignment)
		{
			return nullptr;	// Need to add an error message to log
		}
		else
		{
			size_t nb = requestToSize(bytes);
			size_t req = nb + alignment + offset + kMinChunkSize - kChunkOverhead;
			uint8* mem = reinterpret_cast<uint8*>(alloc(msp, req));

			if (mem != 0)
			{
				void* leader = 0;
				void* trailer = 0;
				MemoryChunk* ptr = memoryToChunk(reinterpret_cast<MemoryChunk*>(mem));
				if (reinterpret_cast<size_t>(mem + offset) % alignment != 0)
				{
					// Misaligned!
					// Find an alignmed point inside the chunk. Since a leading chunk of at least
					// kMinChunkSize has to be returned, if the first calculated address 
					// gives us a chunk less than the size, we move to the next aligned spot. 
					// We have allocated enough room so this is always possible
					uint8* aligned_spot = reinterpret_cast<uint8*>(memoryToChunk(reinterpret_cast<void*>
						((reinterpret_cast<size_t>((mem + offset) + alignment - 1))
						& -alignment)));
					uint8* pos = 0;
					if ((aligned_spot - offset - reinterpret_cast<uint8*>(ptr)) >= kMinChunkSize)
						pos = aligned_spot - offset;
					else
						pos = aligned_spot + alignment - offset;

					MemoryChunk* new_ptr = reinterpret_cast<MemoryChunk*>(pos);
					size_t leadsize = pos - reinterpret_cast<uint8*>(ptr);
					size_t newsize = chunkSize(reinterpret_cast<MemoryChunk*>(ptr)) - leadsize;

					// Now give back the leader and use the rest
					setSizeInuse(msp, new_ptr, newsize);
					setSizeInuse(msp, ptr, leadsize);
					leader = chunkToMemory(ptr);
					ptr = new_ptr;
				}
				// Now give back spare room at the end
				size_t size = chunkSize(ptr);
				if (size > nb + kMinChunkSize)
				{
					size_t rem_size = size - nb;
					MemoryChunk* rem_ptr = chunkPlusOffset(ptr, nb);
					setSizeInuse(msp, ptr, nb);
					setSizeInuse(msp, rem_ptr, rem_size);
				}

				ASSERT_ERROR(chunkSize(ptr) >= nb, "Chunk size is less than requested size in allocating aligned memory");
				ASSERT_ERROR(reinterpret_cast<size_t>(chunkToMemory(ptr)) % alignment == 0, "Aligned chunk is not aligned to necessary alignment");

				if (leader != 0)
					//odin_free(leader);
					free(msp, leader);
				if (trailer != 0)
					//odin_free(trailer);
					free(msp, trailer);

				return chunkToMemory(ptr);
			}
		}
		return nullptr;
	}

	void* calloc(MemorySpace* msp, size_t num_elements, size_t elem_size)
	{
		void* mem;
		size_t req = 0;
		if (num_elements != 0)
		{
			req = num_elements * elem_size;
			if ((num_elements | elem_size) & ~static_cast<size_t>(0xffff) &&
				(req / num_elements != elem_size))
				req = kMaxRequest;	// Force downstream failure on overflow
		}
		mem = alloc(msp, req);
		return mem;
	}

	// Function to initialize MemorySpace
	MemorySpace* initMemorySpace(void* segment_base, size_t segment_size, //ReserveSegmentFunc reserve_segment_func,
		//ReleaseSegmentFunc release_segment_func, CommitPageFunc commit_page_func,
		//DecommitPageFunc decommit_page_func, 
		size_t page_size, size_t segment_granularity,
		size_t segment_threshold)
	{
		if (segment_base != 0)
		{
			// The segment  is already initialized to 0
			// Initialize MemorySpace
			size_t msp_size = padRequest(sizeof(MemorySpace));
			MemoryChunk* ptr = reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(segment_base)+
				alignmentOffset(reinterpret_cast<size_t>(chunkToMemory(reinterpret_cast<MemoryChunk*>(segment_base)))));
			void* msp_mem = chunkToMemory(ptr);
			// Create a new MemorySpace instance using placement new
			MemorySpace* msp = new (msp_mem)MemorySpace;
			ptr->head = msp_size | kInuseBits;	// kPinuse is set so that we don't go to a previous non existent chunk
			// Initialize dv and its size
			msp->dv = 0;
			msp->dv_size = 0;
			// Initialize top and its size
			// Get the next chunk after MemorySpace
			MemoryChunk* next_chunk = nextChunk(memoryToChunk(msp));
			size_t offset = alignmentOffset(reinterpret_cast<size_t>(chunkToMemory(next_chunk)));
			next_chunk = reinterpret_cast<MemoryChunk*>(reinterpret_cast<uint8*>(next_chunk)+offset);
			size_t top_size = reinterpret_cast<uint8*>(segment_base)+segment_size - reinterpret_cast<uint8*>(next_chunk);
			top_size -= offset;
			msp->top = next_chunk;
			msp->top_size = top_size;
			// Save the function pointers
			/*msp->reserve_segment_func = reserve_segment_func;
			msp->release_segment_func = release_segment_func;
			msp->commit_page_func = commit_page_func;
			msp->decommit_page_func = decommit_page_func;*/

			// Establish circular links for small bins
			for (uint32 i = 0; i < kNumSmallBins; ++i)
			{
				MemoryChunk* bin = smallBinAt(msp, i);
				bin->fd = bin->bk = bin;
			}

			// Initialize magic
#if ODIN_PLATFORM == ODIN_PLATFORM_WIN32
			msp->magic = static_cast<size_t>(GetTickCount() ^ static_cast<size_t>(0x55555555U));
#endif
			msp->magic |= 8U;	// Ensure nonzero
			msp->magic &= ~(7U);

			msp->least_addr = reinterpret_cast<uint8*>(segment_base);
			msp->curr_page_index = 1;

			msp->page_size = page_size;
			msp->segment_granularity = segment_granularity;
			msp->segment_threshold = segment_threshold;
			msp->footprint = msp->max_footprint = segment_size;

			return msp;
		}
		else
			return nullptr;
	}


	MemorySpace* createMemorySpace(size_t initialSize, 
		size_t page_size, size_t segment_granularity,
		size_t segment_threshold)
	{
		// Reserve space for segment
		size_t size = (initialSize == 0) ? segment_granularity : initialSize;
		//void* segment = reserve_segment_func(size, NULL);
		void* segment = SysAlloc::reserveSegment(size, NULL);
		// Commit the first page
		SysAlloc::commitPage(segment, page_size);
		// Initialize the segment
		MemorySpace* msp = initMemorySpace(segment, size, 
			page_size, segment_granularity, segment_threshold);

		return msp;

	}

	MemorySpace* createMemorySpaceWithBase(void* baseSegment, size_t baseSegmentSize,
		size_t page_size, size_t segment_granularity, size_t segment_threshold)
	{
		// Commit the first page
		SysAlloc::commitPage(baseSegment, page_size);
		// Initialize the segment
		MemorySpace* msp = initMemorySpace(baseSegment, baseSegmentSize,
			page_size, segment_granularity, segment_threshold);

		return msp;
	}

	size_t destroyMemoryRegion(MemorySpace* msp)
	{
		void* ptr = reinterpret_cast<void*>(msp->least_addr);
		size_t size = msp->footprint;
		SysAlloc::decommitPage(ptr, msp->page_size);

		// Release this memory space
		SysAlloc::releaseSegment(ptr, size);
		return size;
	}

	size_t getTotalReservedMemory(MemorySpace* msp)
	{
		return msp->footprint;
	}

	size_t getMaxReservedMemory(MemorySpace* msp)
	{
		return msp->max_footprint;
	}

	size_t getUsableSize(void* mem)
	{
		if (mem != 0)
		{
			MemoryChunk* ptr = memoryToChunk(mem);
			if ((ptr->head & kInuseBits) != kPinuseBit)
			{
				return (chunkSize(ptr) - kChunkOverhead);
			}
			else
				return 0;
		}
		else
			return 0;
	}

	
	void validateMemorySpace(MemorySpace* msp)
	{
#if ODIN_DEBUG == 1
		// Check bins
		for (uint32 i = 0; i < kNumSmallBins; ++i)
			checkSmallbin(msp, i);
		for (uint32 j = 0; j < kNumTreeBins; ++j)
			checkTreebin(msp, j);
		if (msp->dv_size != 0)
		{
			// Check dv chunk
			ASSERT_ERROR(isAligned(reinterpret_cast<size_t>(chunkToMemory(msp->dv))), "DV chunk is not aligned");
			ASSERT_ERROR(msp->dv_size == chunkSize(msp->dv), "Mismatch in DV size information");
			ASSERT_ERROR(msp->dv_size >= kMinChunkSize, "DV size is less than minimum chunk size");
			ASSERT_ERROR(findInBin(msp, msp->dv) == false, "DV chunk is present in bin");
		}

		if (msp->top_size != 0)
		{
			// Check top chunk
			checkTopChunk(msp);
			ASSERT_ERROR(msp->top_size > 0, "Top size is zero");
			ASSERT_ERROR(findInBin(msp, msp->top) == false, "Top chunk is present in bin");
		}

		size_t total = traverseAndCheck(msp);
		ASSERT_ERROR(total <= msp->footprint, "Total allocated memory is greater than footprint");
#else
		return;
#endif
	}

	MemorySpace* getMemorySpaceAddr(void* mem)
	{
		MemoryChunk* ptr = memoryToChunk(mem);
		return reinterpret_cast<MemorySpace*>(reinterpret_cast<MemoryChunk*>
			((reinterpret_cast<uint8*>(ptr)+chunkSize(ptr)))->prev_foot);
	}
	
	size_t getChunkSize(void* mem)
	{
		return chunkSize(memoryToChunk(mem));
	}
}

// Restore previous warnings settings
#if ODIN_COMPILER == ODIN_COMPILER_MSVC
#   pragma warning (pop)
#endif
