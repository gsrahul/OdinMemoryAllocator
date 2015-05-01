#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "Allocator.h"
#include "PoolAllocator.h"
#include "WorkStealQueue.h"

namespace Odin
{
	#define GLOBAL_QUEUE_SIZE	128

	// Kernel is a function pointer to a function accepting a TaskData argument
	// and will be executed by a worker thread
	typedef void (*Kernel)(TaskData*);

	// TaskID
	typedef size_t	TaskID;

	// Structure to hold task data
	struct TaskData
	{
		void* mKernelData;

		union
		{
			// For streaming tasks
			struct StreamingData
			{
				uint32 mElementCount;
				void* mInputStreams[4];
				void* mOutputStreams[4];
			} mStreamingData;
		};
	};

	// Structure representing a task
	struct Task
	{
		std::atomic<uint32> mOpenTasks;		// Number of child tasks + 1
		TaskID mTaskID;						// The ID of this task
		Task* mParent;						// Pointer to the parent task
		Kernel mKernel;						// Function to run on the thread
		TaskData mTaskData;					// Data on which the kernel operates
	};


	class Scheduler
	{
	public:
		Scheduler(Allocator* alloc);
		~Scheduler();

		// Initialize the scheduler
		bool init();
		
		// Put the thread to sleep until a task is available
		Task* waitUntilTaskIsAvailable();
		
		// Worker thread function
		void workerThread(size_t index);
		
		// Execute a task
		void runTask(Task* task, size_t curr_queue_index);
		
		// This is called after a task has finished executing
		void finishTask(Task* task);
		
		// Run other tasks until the open task count of the current task does not reach 1
		void runOtherTasks(size_t curr_thread_index);
		
		// Function to check if a task has finished executing
		bool isTaskFinished(TaskID task_id);
		
		// Get a pointer to the task based on Task ID
		Task* getTask(TaskID task_id);
		
		/* 
			Calculate the task ID for a task
			Bits 0  - 15 -> Offset from the start of the pool
			Bits 16 - 23 -> Index of the pool (0 - N-1 for local pools and N for global pool)
		*/
		TaskID calcTaskID(Task* task, size_t curr_thread_index);
		
		// Get offset from Task ID
		size_t getOffsetFromTaskID(TaskID id) { return (id & 0xffff); }
		
		// Get pool index from Task ID
		size_t getPoolIndexFromTaskID(TaskID id) { return ((id & 0xff0000) >> 16); }
	
	private:
		// The number of threads run by this scheduler
		size_t mNumThreads;
		// The allocator passed to this scheduler
		Allocator* mAlloc;
		// Flag to signal the worker threads to stop
		std::atomic<bool> mDone;
		// Mutex
		std::mutex mMutex;
		// Condition variable
		std::condition_variable mCondition;
		// Global task queue
		GlobalWorkQueue* mGlobalWorkQueue;
		// Global task freelist
		// The global task freelist will have a queue index of one
		// greater than the largest index of a local queue (or local task freelist).
		// So its index will be equal to "mNumThreads"
		PoolAllocator* mGlobalPoolAlloc;
		// This structure is used to improve cache locality for a
		// work queue and task free list
		// TODO: Test this with a version of separate array of
		// work queues and task free lists
		struct TaskQueueAndPool
		{
			TaskQueueAndPool() : mLocalWorkQueue(nullptr), mLocalPoolAlloc(nullptr)
			{}
			WorkStealQueue* mLocalWorkQueue;	// Work stealing local queue
			PoolAllocator* mLocalPoolAlloc;		// Pool allocator for a free list of tasks
		};
		TaskQueueAndPool* mQueueAndPool;
		// Array of worker threads
		std::thread* mWorkerThreads;
		// Steal a task from another thread's queue
		Task* stealTaskFromOtherThread(size_t curr_thread_index);
	};
}

#endif	// _SCHEDULER_H_