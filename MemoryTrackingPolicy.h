#ifndef _MEMORY_TRACKING_POLICY_H_
#define _MEMORY_TRACKING_POLICY_H_

#include "DataTypes.h"
#include "Assert.h"
#include "FreeList.h"
#include "SysAlloc.h"
#include "StdHeaders.h"
#include "Logger.h"

namespace Odin
{
	// Struct to hold allocation information (line, file, function, etc.)
	struct AllocInfo
	{
		size_t line;
		char func_name[26];
		char file_name[26];
		bool valid;
	};
	
	#define DEBUG_MEM_SIZE	65536

	class NoMemoryTracking
	{
	public:
		static const size_t kOffset = 0;
		
		NoMemoryTracking() {}
		~NoMemoryTracking() {}
		bool init() {}
		inline void onAlloc(void* ptr, size_t size, size_t alignment, 
			const char* file_name, uint32 line, const char* func_name) {}
		inline void onDealloc(void* ptr) const {}
		void logMemoryLeaks(Logger* log) {}
	};

	class SimpleMemoryTracking
	{
	public:
		static const size_t kOffset = 0;	// Offset used to store pointer to an AllocInfo structure
		
		SimpleMemoryTracking() : mSegmentPtr(nullptr), mFreeList(nullptr)
		{
		}
		~SimpleMemoryTracking()
		{
			if(mSegmentPtr)
				releaseSegment(static_cast<void*>(mSegmentPtr), DEBUG_MEM_SIZE);
		}
		bool init()
		{
			mSegmentPtr = static_cast<uint8*>(reserveCommitSegment(DEBUG_MEM_SIZE));	// Reserve and commit 64kiB page for now
			if(!ptr)
				return false;
			mFreeList = new((ptr)) FreeList(static_cast<void*>(mSegmentPtr),
				DEBUG_MEM_SIZE, sizeof(AllocInfo), kDefaultAlignment, 0);
			
			return true;
		}
		inline void onAlloc(void* ptr, size_t size, size_t alignment,
			const char* file_name, uint32 line, const char* func_name)
		{
			AllocInfo* node_ptr = mFreeList.obtainNode();
			ptr->line = line;
			std::strcpy(ptr->func_name, func_name);
			std::strcpy(ptr->file_name, file_name);
			ptr->valid = true;
			*(reinterpret_cast<uint8**>(ptr)) = node_ptr;
		}
		inline void onDealloc(void* ptr)
		{
			if(ptr)
			{
				// Get the pointer to the node
				AllocInfo* node_ptr = reinterpret_cast<AllocInfo*>(*(reinterpret_cast<uint8**>(ptr)));
				// Clear the node
#if ODIN_PLATFORM == ODIN_PLATFORM_WIN64
				ZeroMemory(node_ptr, sizeof(AllocInfo));				
#endif
				node_ptr->valid = false;
				mFreeList.returnNode(nodePtr);
			}
		}
		void logMemoryLeaks(Logger* logger)
		{
			if(log)
			{
				size_t node_count = DEBUG_MEM_SIZE / sizeof(AllocInfo);
				for(size_t index = 0; index < node_count; ++index)
				{
					AllocInfo* node = reinterpret_cast<AllocInfo*>(mSegmentPtr + (index * sizeof(AllocInfo)));
					if(node->valid)
					{
						// We have leaked memory...Log the allocation information
						logger->log(LogChannel::LOG_CHANNEL_GLOBAL, LogType::LOG_TYPE_WARNING, Verbosity::LEVEL_1,
								   node->file_name, node->line, node->func_name, "Memory leaked!");
					}
				}
			}
		}
	private:
		FreeList* mFreeList;
		uint8* mSegmentPtr;
	};
}

#endif	_MEMORY_TRACKING_POLICY_H_