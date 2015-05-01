#include "Scheduler.h"
#include "Assert.h"

namespace Odin
{
	Scheduler::Scheduler(Allocator* alloc) : mAlloc(alloc), mDone(false), mNumThreads(0),
											mGlobalWorkQueue(nullptr), mGlobalPoolAlloc(nullptr),
											mQueueAndPool(nullptr), mWorkerThreads(nullptr)
	{
		ASSERT_ERROR(alloc != nullptr, "No allocator passed to scheduler");
	}
	//-----------------------------------------------------------------------------------------
	Scheduler::~Scheduler()
	{
		// Join the threads
		if (mWorkerThreads)
		{
			for (size_t i = 0; i < mNumThreads; ++i)
			{
				if (mWorkerThreads[i].joinable())
					mWorkerThreads[i].join();
			}
			ODIN_DELETE_ARRAY(mWorkerThreads, mAlloc);
		}
		
		// Destroy the local task queues and pools
		if (mQueueAndPool)
		{
			for (size_t i = 0; i < mNumThreads; ++i)
			{
				ODIN_DELETE(mQueueAndPool[i].mLocalWorkQueue, mAlloc);
				ODIN_DELETE(mQueueAndPool[i].mLocalPoolAlloc, mAlloc);
			}
			ODIN_DELETE_ARRAY(mQueueAndPool, mAlloc);
		}

		// Destroy global task pool
		if (mGlobalPoolAlloc)
		{
			ODIN_DELETE(mGlobalPoolAlloc, mAlloc);
		}

		// Destroy global work queue
		if (mGlobalWorkQueue)
		{
			ODIN_DELETE(mGlobalWorkQueue, mAlloc);
		}
	}
	//-----------------------------------------------------------------------------------------
	bool Scheduler::init()
	{
		// Get the number of threads
#if ODIN_PLATFORM == ODIN_PLATFORM_WIN64
		LPSYSTEM_INFO system_info = nullptr;
		GetSystemInfo(system_info);
		if (system_info)
		{
			mNumThreads = system_info->dwNumberOfProcessors;
		}
#endif
		if (mNumThreads < 1)
			// Something is wrong
			return false;

		// Allocate the global task queue
		mGlobalWorkQueue = ODIN_NEW(GlobalWorkQueue, Allocator::kDefaultAlignment, mAlloc)(mAlloc);
		if(!mGlobalWorkQueue)
			return false;
		
		// Allocate the global task free list
		mGlobalPoolAlloc = ODIN_NEW(PoolAllocator, Allocator::kDefaultAlignment, mAlloc)(mAlloc,
							sizeof(Task), GLOBAL_QUEUE_SIZE, Allocator::kDefaultAlignment, 0);
		if(!mGlobalPoolAlloc)
			return false;
		if (!mGlobalPoolAlloc->init())
			return false;
		
		// Allocate local work queues and their corresponding task free lists
		mQueueAndPool = ODIN_NEW_ARRAY(TaskQueueAndPool, mNumThreads, mAlloc);
		if(!mQueueAndPool)
			return false;
		for(size_t i = 0; i < mNumThreads; ++i)
		{
			mQueueAndPool[i].mLocalWorkQueue = ODIN_NEW(WorkStealQueue, Allocator::kDefaultAlignment, mAlloc)(mAlloc);
			mQueueAndPool[i].mLocalPoolAlloc = ODIN_NEW(PoolAllocator, Allocator::kDefaultAlignment, mAlloc)(mAlloc,
							sizeof(Task), WORK_QUEUE_SIZE, Allocator::kDefaultAlignment, 0);
			ASSERT_FATAL(mQueueAndPool[i].mLocalWorkQueue != nullptr && mQueueAndPool[i].mLocalPoolAlloc != nullptr,
						"Unable to allocate memory for TaskQueueAndPool");
		}
		
		// Allocate N - 1 worker threads
		mWorkerThreads = ODIN_NEW_ARRAY(std::thread, mNumThreads - 1, mAlloc);
		if(!mWorkerThreads)
			return false;
		
		// Start each thread
		for(size_t i = 1; i < mNumThreads; ++i)	// Index 0 is for the main thread
		{
			// TODO: Add exception handling
			mWorkerThreads[i] = std::thread(&workerThread, i);
		}
		
		return true;
	}
	//-----------------------------------------------------------------------------------------
	Task* Scheduler::waitUntilTaskIsAvailable()
	{
		std::unique_lock<std::mutex> lock(mMutex);
		// Wait until a task is available in the global queue
		mCondition.wait(
			lock, [this]{ 
				mGlobalWorkQueue->mTop.load() != mGlobalWorkQueue->mBottom.load();
			});
		return mGlobalWorkQueue->pop();
	}
	//-----------------------------------------------------------------------------------------
	Task* Scheduler::stealTaskFromOtherThread(size_t curr_thread_index)
	{
		Task* t = nullptr;
		for(size_t i = 0; i < mNumThreads; ++i)
		{
			size_t index = (curr_thread_index + i + 1) % mNumThreads;
			
			if((t = mQueueAndPool[index].mLocalWorkQueue->steal()) != nullptr)
			{
				return t;
			}
		}
		return t;
	}
	//-----------------------------------------------------------------------------------------
	void Scheduler::workerThread(size_t index)
	{
		size_t my_index = index;
		WorkStealQueue* local_work_queue = mQueueAndPool[my_index].mLocalWorkQueue;
		while(!mDone.load())
		{
			// Wait until a task is available
			Task* task = waitUntilTaskIsAvailable();
			runTask(task, my_index);
		}
	}
	//-----------------------------------------------------------------------------------------
	void Scheduler::runTask(Task* task, size_t curr_queue_index)
	{
		while (task->mOpenTasks > 1)
		{
			ASSERT_ERROR(task->mOpenTasks > 0, "Number of open tasks is somehow 0");
			// This task cannot be executed at this time, so execute other tasks
			runOtherTasks(curr_queue_index);
		}
		// Execute the kernel
		(task->mKernel)(&task->mTaskData);
		// Finish the task
		finishTask(task);
	}
	//-----------------------------------------------------------------------------------------
	void Scheduler::finishTask(Task* task)
	{
		// Get the number of open tasks
		uint32 open_tasks = --task->mOpenTasks;
		
		// Notify parent this task is done
		if(task->mParent)
		{
			finishTask(task->mParent);
		}
		
		// Return this task node if this task is done completely
		if(open_tasks == 0)
		{
			size_t index = getPoolIndexFromTaskID(task->mTaskID);
			// Call destructor and return to pool
			task->~Task();
			if(index == mNumThreads)
			{
				mGlobalPoolAlloc->deallocate(task);
			}
			else
			{
				mQueueAndPool[index].mLocalPoolAlloc->deallocate(task);
			}
		}
	}
	//-----------------------------------------------------------------------------------------
	void Scheduler::runOtherTasks(size_t curr_thread_index)
	{
		Task* task = nullptr;
		
		// Get the next task to execute
		if((task = mQueueAndPool[curr_thread_index].mLocalWorkQueue->pop()) ||
		   (task = mGlobalWorkQueue->pop()) ||
		   (task = stealTaskFromOtherThread(curr_thread_index)))
		{
			runTask(task, curr_thread_index);
		}
		else
		{
			std::this_thread::yield();
		}
	}
	//-----------------------------------------------------------------------------------------
	bool Scheduler::isTaskFinished(TaskID task_id)
	{
		Task* task = getTask(task_id);
		if (task->mOpenTasks == 0)
			return true;
		return false;
	}
	//-----------------------------------------------------------------------------------------
	Task* Scheduler::getTask(TaskID task_id)
	{
		size_t offset = getOffsetFromTaskID(task_id);
		size_t index  = getPoolIndexFromTaskID(task_id);
		Task* task = nullptr;

		if (index == mNumThreads)
		{
			// Global pool
			task = const_cast<Task*>(reinterpret_cast<const Task*>
				(mGlobalPoolAlloc->getStartAddress()) + offset);
		}
		else
		{
			// One of the local pools
			task = const_cast<Task*>(reinterpret_cast<const Task*>
				(mQueueAndPool[index].mLocalPoolAlloc->getStartAddress()) + offset);
		}
		return task;
	}
	//-----------------------------------------------------------------------------------------
	TaskID Scheduler::calcTaskID(Task* task, size_t queue_index)
	{
		size_t offset = 0;
		TaskID id = 0;
		if (queue_index == mNumThreads)
		{
			// This task belongs to the global pool
			offset = task - reinterpret_cast<const Task*>(mGlobalPoolAlloc->getStartAddress());
			id = (queue_index << 16) | offset;
		}
		else
		{
			// This task belongs to one of the local pools
			offset = task - reinterpret_cast<const Task*>
				(mQueueAndPool[queue_index].mLocalPoolAlloc->getStartAddress());
			id = (queue_index << 16) | offset;
		}
		return id;
	}
}