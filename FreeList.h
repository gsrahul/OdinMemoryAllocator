#ifndef _FREELIST_H_
#define _FREELIST_H_

#include "DataTypes.h"
#include "Assert.h"

namespace Odin
{
	class FreeList
	{
	public:
		FreeList(void* start, size_t size, size_t element_size, size_t alignment, size_t offset);
		// Destructor
		~FreeList();
		// Get a node from the free list
		void* obtainNode(void);
		// Return a node to the free list
		void returnNode(void* ptr);

	private:
		// Pointer to the next free node
		uint8* mNext;
	};
}

#endif	_FREELIST_H_