#include "SysAlloc.h"

namespace Odin
{
	namespace SysAlloc
	{
		void* reserveSegment(size_t size, void* ptr)
		{
#if ODIN_PLATFORM == ODIN_PLATFORM_WIN32
			LPVOID basePtr = VirtualAlloc(
				ptr,
				size,
				MEM_RESERVE,
				PAGE_NOACCESS);
			if (basePtr == NULL)
				return 0;
			else
				return basePtr;
#endif
		}
		//----------------------------------------------------------------------------------------------------------------------
		// Release a segment
		void releaseSegment(void* ptr, size_t size)
		{
#if ODIN_PLATFORM == ODIN_PLATFORM_WIN32
			BOOL success = VirtualFree(
				ptr,
				0,
				MEM_RELEASE);
#endif
		}
		//----------------------------------------------------------------------------------------------------------------------
		// Commit a page
		void commitPage(void* ptr, size_t size)
		{
#if ODIN_PLATFORM == ODIN_PLATFORM_WIN32
			LPVOID result_ptr = VirtualAlloc(
				reinterpret_cast<LPVOID>(ptr),
				size,
				MEM_COMMIT,
				PAGE_READWRITE);
#endif
		}
		//----------------------------------------------------------------------------------------------------------------------
		// Decommit a page
		void decommitPage(void* ptr, size_t size)
		{
#if ODIN_PLATFORM == ODIN_PLATFORM_WIN32
			BOOL result = VirtualFree(
				reinterpret_cast<LPVOID>(ptr),
				size,
				MEM_DECOMMIT);
#endif
		}
		//----------------------------------------------------------------------------------------------------------------------
		// Reserve and commit a segment
		// This function will bypass the direct mapped cache and directly commits
		// pages from the system.
		void* reserveCommitSegment(size_t size)
		{
#if ODIN_PLATFORM == ODIN_PLATFORM_WIN32
			LPVOID basePtr = VirtualAlloc(
				NULL,
				size,
				MEM_COMMIT | MEM_RESERVE,
				PAGE_READWRITE);
			if (basePtr == NULL)
				return 0;
			else
				return basePtr;
#endif
		}
	}
}