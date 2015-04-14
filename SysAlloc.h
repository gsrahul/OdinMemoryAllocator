#ifndef _SYS_ALLOC_H_
#define _SYS_ALLOC_H_

#include <Windows.h>
#include <WinBase.h>


namespace Odin
{
	namespace SysAlloc
	{
		// Declare functions to reserve and release memory from the system
		// Reserve a segment. if ptr is specified, reserve space starting from the specified address
		// (used by alloc to extend the segment size when more memory is needed)
		void* reserveSegment(size_t size, void* ptr = NULL);
		
		// Release a segment
		void releaseSegment(void* ptr, size_t size);
		
		// Commit a page
		void commitPage(void* ptr, size_t size);
		
		// Decommit a page
		void decommitPage(void* ptr, size_t size);
		
		// Reserve and commit a segment
		// This function will bypass the direct mapped cache and directly commits
		// pages from the system.
		void* reserveCommitSegment(size_t size);
	}
}

#endif	//_SYS_ALLOC_H_